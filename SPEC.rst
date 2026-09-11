SIML (Simple Item Markup Language)
==================================

SIML is a deliberately tiny subset of YAML designed so that a reference
parser can be written in short, clean ANSI C using only the standard
library.

It is line-oriented, has a fixed structure, and does not support arbitrary
nesting. It is meant for files like lists of configuration records
(e.g. CVAR definitions), not as a general data language.

Example
-------

.. code-block:: text

   - id: r_fullscreen
     default: 1
     min: 0.0
     max: 1.0
     flags: [CVAR_ARCHIVE, CVAR_TEMP]
     description: |
       Lorem ipsum dolor sit amet, consectetur adipiscing elit.
       Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
       Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.

   - id: cl_sensitivity
     default: 3.0
     min: 0.1
     max: 10.0
     flags: []
     description: |
       Lorem ipsum dolor sit amet, consectetur adipiscing elit.

       Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
       Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.

   - id: r_audio
     default: 1
     min: 0.0
     max: 1.0
     flags:
     - CVAR_ARCHIVE
     - CVAR_TEMP
     description: |
       Example using block list syntax for ``flags``.

1. Data model
-------------

A SIML document is:

* Either a single *item* (a flat mapping) or a sequence of items.
* Each item is a flat mapping from string keys to values.
* Value types:

  - Scalar (single line, stored as a string),
  - List of scalars (inline ``[a, b, c]`` or block ``key:`` + ``- item`` form),
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

A SIML document is either:

* A sequence of items (list form), separated by blank and/or comment lines,
  or
* A single item (object form) with fields at column 0.

3.1 List form (``- `` items)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each item begins with a line of the form::

   - key: value

Rules:

* The ``-`` must be the first character on the line.
* It is followed by exactly one space.
* Then a key, a colon ``:``, one space, then the value text.
* The key on the first line is just another field (often ``id``, but the
  format does not enforce this).

Following lines belonging to the same item must start with exactly
two spaces::

   ␣␣key: value
   ␣␣otherKey: value
   ␣␣description: |
   ␣␣  Literal block content...

(Here ``␣`` denotes a space.)

Rules:

* Two spaces, then ``key``, then ``:``, then one space, then the value.
* Lines starting with two spaces followed by ``-`` introduce elements of a
  block list value (see section 5.2).
* The item ends when one of these happens:

  - A new item starts (``-`` at column 0),
  - End of file.

Blank lines and comment lines inside an item are allowed (see comments).

3.2 Single-object form
~~~~~~~~~~~~~~~~~~~~~~

Instead of a list, a SIML file may contain a single item with fields that
start at column 0::

   key: value
   otherKey: value
   description: |
     Literal block content...

Rules:

* The first non-comment/non-blank line must be ``key: value`` with no
  leading ``-``.
* All fields for the single object start at column 0 (no two-space indent).
* Block lists under a key still use an indented ``-`` (e.g. ``␣␣- item``),
  so they do not conflict with the top-level structure.
* A ``- `` at column 0 after single-object parsing has started is an error
  (mixing list and single-object forms is not allowed).

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

For block list values, the key line has an empty value (nothing after the
colon, ignoring trailing spaces and inline comment), and the elements of
the list are read from subsequent ``-`` lines (see section 5.2).

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

Lists are sequences of bare words and can be written in either inline
or block form.

Inline form::

   flags: [CVAR_ARCHIVE, CVAR_TEMP]
   tags: [foo, bar, baz]
   empty: []

Block form (inside an item)::

   - id: r_audio
     flags:
       - CVAR_ARCHIVE
       - CVAR_TEMP

In both forms, list elements are “bare words” like ``CVAR_ARCHIVE``,
``foo_bar_123``, etc.

Inline form rules:

* The value must start with ``[`` and end with ``]`` on the same line.
* Inside, zero or more list items, separated by commas.
* Each list item is:

  - Optional leading/trailing spaces,
  - A non-empty sequence of characters that are not: comma, closing bracket,
    or space.

* No quoting or escaping inside inline lists.

Block form rules:

* The key line is ``key:`` (or ``- key:`` as the first field of an item)
  where the value part is empty after the colon (ignoring trailing spaces
  and inline comment).
* The list consists of subsequent lines that:

  - Belong to the same item (start with the standard item indentation of
    two spaces inside an item), and
  - After those two spaces, add at least one more space before ``-`` and a
    trailing space. A common and recommended layout is four spaces then
    ``-`` (``␣␣␣␣- item``), matching normal YAML list indentation.

  Example (logical layout, without the code-block padding)::

     - id: example
       flags:
       - CVAR_ARCHIVE
       - CVAR_TEMP

* For each such list line, everything after the ``-`` and any following
  spaces, up to an inline comment (see section 6), is parsed as a bare word
  and becomes one element of the list.
* The block list ends when one of these happens:

  - A new item starts (``-`` at column 0),
  - A new field line starts (two spaces, then a key and ``:``),
  - End of file.

* Blank lines and full-line comments between list items are allowed and
  ignored; they do not create empty elements.
* Inline and block lists are equivalent at the data-model level; both
  represent the same “list of scalars” value type.
* No quoting or escaping is allowed inside list elements; each element
  must be a single bare word.

5.3 Literal block values (``|``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A field can introduce a multi-line literal::

   description: |
     Lorem ipsum dolor sit amet, consectetur adipiscing elit.
     Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.

     Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.

Rules:

1. The field line is:

   *Either*::

      - key: |

   *or*::

      ␣␣key: |

   i.e. the value part is exactly a single ``|`` (ignoring trailing spaces
   and inline comment).

2. The block content consists of all subsequent lines until:

   * A line that starts a new item (``-`` at column 0 as first non-space), or
   * A line that starts a new field for the current item:

     - Column 0 in the single-object form, or
     - Exactly two spaces then ``key:`` inside a list item,

     with the same key/colon rules as any other field line.
   * A comment line that starts at column 0 (``#`` with no leading spaces), or
   * End of file.

   To keep text such as ``note: still part of the block`` inside the block,
   indent it by at least one extra space so it no longer matches the field
   indentation rules above.

3. For each content line:

   * If the line starts with at least one space, that leading space
     is removed.
   * Otherwise, the line is taken as-is.
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

   document       ::= single_object | item_sequence

   item_sequence  ::= { blank_or_comment | item }

   single_object  ::= field_body_line { blank_or_comment | object_field }
   object_field   ::= field_body_line
                    | "  - " block_list_element   ; block list item

   item           ::= item_first_field { blank_or_comment | item_field }
   item_first_field
                   ::= "- " field_body
   item_field     ::= "  " field_body
                    | "  - " block_list_element   ; block list item

   field_body_line
                   ::= field_body

   field_body     ::= key ":" " " field_value

   field_value    ::= list_value
                    | block_marker
                    | scalar_value

   block_marker   ::= "|"

   list_value     ::= "[" [ list_item { "," list_item } ] "]"
                    | block_list

   block_list     ::= { block_list_line }

   block_list_line
                   ::= "  - " block_list_element

   block_list_element
                   ::= bare_word   ; up to inline comment/#

   list_item      ::= SPACES? bare_word SPACES?

   scalar_value   ::= NONEMPTY_TEXT   ; up to end-of-line, before inline comment/#

   key            ::= ALPHA [ ALNUM | "_" ]*

   bare_word      ::= 1*(non-space, non-comma, non-"]", non-# chars)

   blank_or_comment
                   ::= BLANK_LINE | COMMENT_LINE

Block content after ``block_marker`` is defined by the rules in section 5.3.
Block list details are defined in section 5.2.

8. Differences from YAML
------------------------

SIML is intentionally much more limited than YAML.

Major restrictions:

* No arbitrary nesting:

  - Only: top-level sequence of flat mappings.
  - Values are only: scalar, simple list of bare words (inline or block),
    or literal block.
  - No mappings inside lists, no lists of lists, no nested mappings.

* No type system:

  - No booleans, null, or typed numbers at the syntax level.
  - Everything is returned as strings/lists of strings; type interpretation
    is up to the caller.

* No advanced YAML features:

  - No anchors (``&``), aliases (``*``), tags (``!tag``),
    directives (``%YAML``), or document separators (``---``, ``...``).
  - No folded blocks (``>``).

* Simplified syntax:

  - Fixed indentation: ``- `` at column 0 for new items,
    and two spaces for subsequent fields and block list lines.
  - Keys are unquoted identifiers.
  - Lists are either inline ``[a, b, c]`` or simple block lists using
    ``- item``, and elements are always bare words.
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
