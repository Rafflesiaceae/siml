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
separated by ``---`` lines.

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
* Indentation is strict and exactly two spaces (no variable size, no Tabs)

3. Top-level structure
----------------------

* Items are separated by lines that are exactly ``---`` (optionally followed
  by spaces and/or an inline comment).
* The first item may omit the leading ``---``; a trailing ``---`` after the
  last item is NOT allowed.
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

SIML itself treats all scalar values as strings; interpretation as integer,
float, enum, etc. is up to the application. Lists are sequences of strings.

5.1 Scalar values
~~~~~~~~~~~~~~~~~

A scalar value is any non-empty text not starting with ``[`` or ``|``::

   default: 1
   min: 0.0
   max: 10.0
   mode: normal

The parser returns the scalar as a string.

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

5.3 Literal block values (``|``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A field can introduce a multi-line literal::

   description: |
     Lorem ipsum dolor sit amet, consectetur adipiscing elit.
     Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.

     Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.

Rules:

1. The field line is written as ``key: |``. The value part is exactly ``|``,
   ignoring trailing spaces and any inline comment.

2. The block content consists of all following lines until one of:

   * Any line with a non-space character at column 0, or
   * End of file.

3. Every non-empty content line MUST start with exactly two spaces. The parser
   strips these two spaces and uses the remainder of the line as text. Anything
   after the first two spaces (including ``#``, ``:``, ``---``, or tabs) is
   taken literally.

4. Empty lines between content lines become empty lines in the resulting
   string.

5. The resulting block value is all content lines joined with ``\n``. A parser
   must keep trailing newline at the end of the block.

6. Comments and blank lines
---------------------------

Full-line comments
~~~~~~~~~~~~~~~~~~

* A line *outside* of literal blocks whose first non-space character is ``#``
  is a comment and is ignored.

Inline comments
~~~~~~~~~~~~~~~

Inline comments are only recognized **outside** literal blocks.

On scalar and list field lines, an inline comment starts at the first ``#``
that is:

* preceded by at least one space, and
* for list values, appears **after** the closing ``]`` (if any).

The comment runs from that ``#`` to end of line. The stored value is the text
before the ``#``, with trailing spaces removed.

Scalars:

* For scalar fields, a ``#`` without preceding whitespace (e.g. ``mode: fast#1``)
  is part of the value, not a comment.

Lists:

* For single-line lists, inline comments may only follow the complete list, e.g.::

    flags: [CVAR_ARCHIVE, CVAR_TEMP]  # two flags

* In multi-line lists, blank lines and full-line comments (lines whose first
  non-space character is ``#``) may appear between items, e.g.::

    flags: [
      foo,
      # some comment
      bar,
    ]

  Here the items are ``foo`` and ``bar``; the ``#`` line is ignored.

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
    is up to the consumer.

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

Implementation intent
---------------------

These restrictions are intentional so a SIML parser can be written in
portable ANSI C as:

* A single line-by-line state machine,
* Using only ``fgets``, ``strchr``, ``strncmp``, and minimal string
  trimming/splitting,
* Without requiring a general tokenizer or parser generator.

This makes SIML suitable as a configuration source in small C codebases
that want YAML-like readability with trivial parsing complexity. 0000000000000000000000000000000000000000
