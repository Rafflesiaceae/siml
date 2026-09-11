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
* Indentation is strict and exactly two spaces. There is no variable-width
  indentation and no tab characters may be used for indentation.

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

    [a-zA-Z_][a-zA-Z0-9_.-]*

* Keys are unquoted.
* Keys are unique within a single item (duplicate keys may be treated
  as an error or last-one-wins, up to the application).

5. Values
---------

Everything after ``key: `` on a line is the raw value text, before stripping
trailing whitespace and optional inline comments.

SIML itself treats all simple values as strings; interpretation as integer,
float, enum, etc. is up to the consumer. Lists are sequences of such strings.

Simple scalar text
~~~~~~~~~~~~~~~~~~

A simple scalar is:

* A sequence of characters that does **not** contain:

  - a newline,
  - a comma (``","``), or
  - a closing bracket (``"]"``).

* For scalar fields (``key: value``), the simple scalar must also **not**
  start with ``"["`` or ``"|"``, because those leading characters select
  list and literal block syntax instead (see below).

* The character ``"#"`` is allowed inside simple scalars. Whether a particular
  ``"#"`` starts a comment depends only on the line-level rules in section 6
  (inline comments), not on the value itself.

5.1 Scalar values
~~~~~~~~~~~~~~~~~

A scalar value is a simple scalar taken from a field line that does not begin
with ``"["`` or ``"|"`` after ``key: ``::

   default: 1
   min: 0.0
   max: 10.0
   mode: normal
   note: va#lue

The parser determines the scalar text as:

* everything after ``key: `` up to the end of the line, then
* stripping trailing spaces and any inline comment according to section 6.

The resulting text must be a valid simple scalar (it may be empty).

5.2 List values
~~~~~~~~~~~~~~~

Lists are sequences of simple scalar values written with brackets.

Inline examples::

   flags: [CVAR_ARCHIVE, CVAR_TEMP]
   tags: [foo, bar, baz,]
   empty: []
   weird: [va#lue, foo bar]

Multi-line bracketed form::

   flags: [
     CVAR_ARCHIVE,
     CVAR_TEMP,
   ]

Rules:

* The value must start with ``"["`` and end with the matching ``"]"``.
  The closing bracket may appear on the same line or a later line of
  the same item.

* Inside, there are zero or more **list elements**, separated by commas.
  A trailing comma before ``"]"`` is allowed.

* Each list element is a simple scalar:

  - Leading and trailing spaces around the element are ignored.
  - The element text runs up to the next comma or closing ``"]"``.
  - The resulting text must not contain ``","`` or ``"]"`` and must not
    be empty.

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

   Trailing blank lines immediately before these terminators are ignored and
   are not part of the block content.

3. Every non-empty content line MUST start with exactly two spaces. The parser
   strips these two spaces and uses the remainder of the line as text. Anything
   after the first two spaces (including ``#``, ``:``, ``---``, or tabs) is
   taken literally.

4. Empty lines between content lines become empty lines in the resulting
   string.

5. The resulting block value is all content lines joined with ``\n``. A parser
   must keep the trailing newline at the end of the block.

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

   document_stream  ::= item
                        { document_separator item }

   document_separator ::= "---" SPACES? [ "#" TEXT ]?

   item             ::= field_body { blank_or_comment | field_body }

   field_body       ::= key ":" " " field_value

   field_value      ::= list_value
                      | block_marker
                      | scalar_value

   block_marker     ::= "|"

   list_value       ::= "[" [ list_item { "," list_item } ] [ "," ] "]"

   list_item        ::= SPACES? simple_scalar SPACES?

   scalar_value     ::= simple_scalar   ; after stripping trailing spaces/comment

   key              ::= ALPHA [ ALNUM | "_" | "-" | "." ]*

   simple_scalar    ::= 1*(CHAR_EXCEPT_NEWLINE_COMMA_RBRACKET)

   blank_or_comment ::= BLANK_LINE | COMMENT_LINE

``CHAR_EXCEPT_NEWLINE_COMMA_RBRACKET`` means any character except newline,
comma (","), or closing bracket ("]"). Whether a ``"#"`` starts a comment is
governed by section 6 (inline comments), not by this production. For scalar
fields, the first character of the value after ``key: `` must not be "[" or "|",
otherwise the field is parsed as a list or literal block instead.

Block content after ``block_marker`` is defined by the rules in section 5.3.

8. Differences from YAML
------------------------

SIML is intentionally much more limited than YAML.

Major restrictions:

* No arbitrary nesting:

  - Only: top-level sequence of flat mappings.
  - Values are only: scalar, list of simple scalars written with brackets,
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
    elements are always unquoted simple scalars.
  - Literal blocks are always ``|`` and can appear anywhere within an item.

Implementation intent
---------------------

These restrictions are intentional so a SIML parser can be written in
portable ANSI C easily as:

* A single line-by-line state machine,
* Using only ``fgets``, ``strchr``, ``strncmp``, and minimal string
  trimming/splitting,
* Without requiring a general tokenizer or parser generator.
