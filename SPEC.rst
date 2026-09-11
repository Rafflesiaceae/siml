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

1. Data model
-------------

A SIML document is:

* A sequence of *items* (records).
* Each item is a flat mapping from string keys to values.
* Value types:

  - Scalar (single line, stored as a string),
  - List of scalars (simple ``[a, b, c]`` form),
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

A SIML document is a sequence of items, separated by one or more blank
and/or comment lines.

3.1 Item start
~~~~~~~~~~~~~~

Each item begins with a line of the form::

   - key: value

Rules:

* The ``-`` must be the first character on the line.
* It is followed by exactly one space.
* Then a key, a colon ``:``, one space, then the value text.
* The key on the first line is just another field (often ``id``, but the
  format does not enforce this).

3.2 Item fields
~~~~~~~~~~~~~~~

Following lines belonging to the same item must start with exactly
two spaces::

   ␣␣key: value
   ␣␣otherKey: value
   ␣␣description: |
   ␣␣  Literal block content...

(Here ``␣`` denotes a space.)

Rules:

* Two spaces, then ``key``, then ``:``, then one space, then the value.
* The item ends when one of these happens:

  - A new item starts (``-`` at column 0),
  - End of file.

Blank lines and comment lines inside an item are allowed (see comments).

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

SIML itself treats all values as strings; interpretation as integer, float,
enum, etc. is up to the application.

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

Lists use a minimal inline form::

   flags: [CVAR_ARCHIVE, CVAR_TEMP]
   tags: [foo, bar, baz]
   empty: []

Rules:

* The value must start with ``[`` and end with ``]`` on the same line.
* Inside, zero or more list items, separated by commas.
* Each list item is:

  - Optional leading/trailing spaces,
  - A non-empty sequence of characters that are not: comma, closing bracket,
    or space.

  In practice, these are “bare words” like ``CVAR_ARCHIVE``,
  ``foo_bar_123``, etc.

* No quoting or escaping inside lists.

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
   * End of file.

   By design, a literal block should be the last field of the item.

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

   document       ::= { blank_or_comment | item }

   item           ::= item_first_field { blank_or_comment | item_field }
   item_first_field
                   ::= "- " field_body
   item_field     ::= "  " field_body

   field_body     ::= key ":" " " field_value

   field_value    ::= list_value
                    | block_marker
                    | scalar_value

   block_marker   ::= "|"

   list_value     ::= "[" [ list_item { "," list_item } ] "]"

   list_item      ::= SPACES? bare_word SPACES?

   scalar_value   ::= NONEMPTY_TEXT   ; up to end-of-line, before inline comment/#

   key            ::= ALPHA [ ALNUM | "_" ]*

   bare_word      ::= 1*(non-space, non-comma, non-"]", non-# chars)

   blank_or_comment
                   ::= BLANK_LINE | COMMENT_LINE

Block content after ``block_marker`` is defined by the rules in section 5.3.

8. Differences from YAML
------------------------

SIML is intentionally much more limited than YAML.

Major restrictions:

* No arbitrary nesting:

  - Only: top-level sequence of flat mappings.
  - Values are only: scalar, simple list of bare words, or literal block.
  - No mappings inside lists, no lists of lists, no nested mappings.

* No type system:

  - No booleans, null, or typed numbers at the syntax level.
  - Everything is returned as strings; type interpretation is up to the
    caller.

* No advanced YAML features:

  - No anchors (``&``), aliases (``*``), tags (``!tag``),
    directives (``%YAML``), or document separators (``---``, ``...``).
  - No folded blocks (``>``).

* Simplified syntax:

  - Fixed indentation: ``- `` at column 0 for new items,
    and two spaces for subsequent fields.
  - Keys are unquoted identifiers.
  - Lists are only the inline ``[a, b, c]`` form with bare words.
  - Literal blocks are always ``|`` and should be the last field of the item.

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
