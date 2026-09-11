SIML (Simple Item Markup Language)
==================================

SIML is a strict small subset of YAML designed so that a streaming reference
parser can be written in short, clean ANSI C.

It is line-oriented, has a fixed structure, and does not support arbitrary
nesting.

Example
-------

.. code-block:: text

   id: r_fullscreen
   default: 1
   min: 0.0
   max: 1.0
   flags: [CVAR_ARCHIVE, CVAR_TEMP]
   description: |
     Lorem ipsum dolor sit amet, consectetur adipiscing elit.
     Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
     Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.
   ---
   id: cl_sensitivity
   default: 3.0
   min: 0.1
   max: 10.0
   flags: []
   description: |
     Lorem ipsum dolor sit amet, consectetur adipiscing elit.

     Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
     Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.
   ---
   id: r_audio
   default: 1
   min: 0.0
   max: 1.0
   flags: [
     CVAR_ARCHIVE,
     CVAR_TEMP,
   ]
   description: |
     Example using bracketed multi-line list syntax for ``flags``.

1. Data model
-------------

A SIML file is a sequence of one or more items (documents). Items are
separated by ``---`` lines; the first separator is optional and a trailing
separator is allowed.

* Each item is a flat mapping from string keys to values.
* Value types:

  - Scalar (single line, stored as a string),
  - List of scalars (bracketed ``[a, b, c]`` form, single-line or multi-line),
  - Literal block (multi-line string starting with ``|``).

No nested mappings, no lists of lists, no lists of mappings.

2. Encoding and whitespace
--------------------------

* Text encoding: UTF-8, no BOM.
* Line endings: ``\n`` or ``\r\n`` (treated the same).
* Tabs are only allowed inside literal block contents, not for indentation.
* Indentation is done with spaces.

3. Top-level structure
----------------------

* Items are separated by lines that are exactly ``---`` (optionally followed
  by spaces and/or an inline comment).
* The first item may omit the leading ``---``; a trailing ``---`` after the
  last item is allowed and ignored.
* Blank lines and full-line comments may appear before separators.
* Each item is written as a mapping whose fields start at column 0.
  There is no top-level list syntax like ``- key: value``; every field starts
  with a key.

4. Keys
-------

* Keys are simple identifiers::

    [a-zA-Z_][a-zA-Z0-9_]*

* Keys are unquoted.
* Keys are unique within a single item (duplicate keys may be treated
  as an error or last-one-wins, up to the application).

5. Values
---------

Everything after ``key: `` on a line is the raw value text, before stripping
trailing whitespace and optional inline comments.

SIML itself treats all non-list values as strings; interpretation as integer,
float, enum, etc. is up to the application. Lists are sequences of strings
(bare words).

5.1 Scalar values
~~~~~~~~~~~~~~~~~

A scalar value is any non-empty text not starting with ``[`` or ``|``::

   default: 1
   min: 0.0
   max: 10.0
   mode: normal

The parser returns the scalar as a string. Numeric parsing (``strtol``,
``strtod``) or enum interpretation is done by the caller.

5.2 List values
~~~~~~~~~~~~~~~

Lists are sequences of bare words written with brackets.

Inline examples::

   flags: [CVAR_ARCHIVE, CVAR_TEMP]
   tags: [foo, bar, baz,]
   empty: []

Multi-line bracketed form::

   flags: [
     CVAR_ARCHIVE,
     CVAR_TEMP,
   ]

Rules:

* The value must start with ``[`` and end with the matching ``]``. The closing
  bracket may appear on the same line or a later line of the same item.
* Inside, zero or more list items separated by commas; a trailing comma before
  ``]`` is allowed.
* Each list item is optional leading/trailing spaces followed by a non-empty
  sequence of characters that are not a comma or closing bracket.
* No quoting or escaping inside lists. ``#`` is just another character.

5.3 Literal block values (``|``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A field can introduce a multi-line literal::

   description: |
     Lorem ipsum dolor sit amet, consectetur adipiscing elit.
     Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.

     Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.

Rules:

1. The field line is ``key: |``. The value part is exactly a single ``|``
   (ignoring trailing spaces and inline comment).

2. The block content consists of all subsequent lines until:

   * A line that starts a new field for the current item (column 0) with the
     same key/colon rules as any other field line.
   * A document separator line ``---`` at column 0,
   * A comment line that starts at column 0 (``#`` with no leading spaces), or
   * End of file.

   To keep text such as ``note: still part of the block`` inside the block,
   indent it by at least one extra space so it no longer matches the field
   indentation rules above.

3. For each content line:

   * Determine the smallest number of leading spaces across all
     non-blank content lines. Strip exactly that many leading spaces
     from every line (blank lines are left empty). Tabs are left as-is,
     matching YAML’s indentation stripping.
   * The resulting lines are joined with ``\n`` to form the value string.
   * The final newline at the end of the block is optional; an implementation
     may keep or drop it, as long as it is consistent.

4. Empty lines between text lines belong to the block and produce empty
   lines in the resulting string.

6. Comments and blank lines
---------------------------

* A line whose first non-space character is ``#`` is a comment
  and is ignored.
* A line that is all whitespace or empty is a blank line and is ignored,
  except:

  - Inside a literal block, blank lines are part of the content (because
    they appear after a ``key: |`` and before the next item or EOF).

Inline comments:

* On key/value lines (non-block), a ``#`` that appears after the value
  can start an inline comment.
* For simplicity, SIML parsers may implement the following rule:

  *Strip from the first ``#`` that is preceded by at least one space.*

  Example::

     default: 1  # default fullscreen

  Here the stored value is ``"1"``.

* Inside lists and literal blocks, ``#`` has no special meaning.

7. Informal grammar
-------------------

This is an informal EBNF-style description of SIML:

.. code-block:: text

   document_stream  ::= [ document_separator ]
                        item
                        { document_separator item }
                        [ document_separator ]

   document_separator ::= "---" SPACES? [ "#" TEXT ]?

   item             ::= field_body { blank_or_comment | field_body }

   field_body       ::= key ":" " " field_value

   field_value      ::= list_value
                      | block_marker
                      | scalar_value

   block_marker     ::= "|"

   list_value       ::= "[" [ list_item { "," list_item } ] [ "," ] "]"

   list_item        ::= SPACES? bare_word SPACES?

   scalar_value     ::= NONEMPTY_TEXT   ; up to end-of-line, before inline comment/#

   key              ::= ALPHA [ ALNUM | "_" ]*

   bare_word        ::= 1*(non-space, non-comma, non-"]", non-# chars)

   blank_or_comment ::= BLANK_LINE | COMMENT_LINE

Block content after ``block_marker`` is defined by the rules in section 5.3.

8. Differences from YAML
------------------------

SIML is intentionally much more limited than YAML.

Major restrictions:

* No arbitrary nesting:

  - Only: top-level sequence of flat mappings.
  - Values are only: scalar, list of bare words written with brackets,
    or literal block.
  - No mappings inside lists, no lists of lists, no nested mappings.

* No type system:

  - No booleans, null, or typed numbers at the syntax level.
  - Everything is returned as strings/lists of strings; type interpretation
    is up to the caller.

* No advanced YAML features:

  - No anchors (``&``), aliases (``*``), tags (``!tag``),
    directives (``%YAML``), or ``...`` document terminators.
  - No folded blocks (``>``).

* Simplified syntax:

  - Items are separated by ``---``; fields are at column 0.
  - Keys are unquoted identifiers.
  - Lists use bracket syntax ``[a, b, c]`` (single-line or multi-line) and
    elements are always bare words.
  - Literal blocks are always ``|`` and can appear anywhere within an item.

* Minimal comment behavior:

  - Only full-line comments and simple inline comments on scalar/list lines.
  - ``#`` in literal blocks is just a character.

Implementation intent
---------------------

These restrictions are intentional so a SIML parser can be written in
portable ANSI C as:

* A single line-by-line state machine,
* Using only ``fgets``, ``strchr``, ``strncmp``, and minimal string
  trimming/splitting,
* Without requiring a general tokenizer or parser generator.

This makes SIML suitable as a configuration source in small C codebases
that want YAML-like readability with trivial parsing complexity.
