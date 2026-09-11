SIML (Simple Item Markup Language)
==================================

SIML is a strict, line-oriented subset of YAML 1.2 designed so that a streaming
reference parser can be written in short, clean ANSI C.

SIML supports **arbitrary nesting** of mappings and sequences using YAML block
indentation. It also supports **single-line flow sequences** ``[...]``.

It can be **perfectly round-tripped**, such that a reference streaming parser
can 1:1 regenerate the input byte-by-byte.

Example
-------

.. code-block:: text

   id: r_fullscreen
   default: 1
   range:
     min: 0.0
     max: 1.0
   flags: [CVAR_ARCHIVE,CVAR_TEMP]  # aligned comment
   ui:
     labels:
       - Low
       - High
   description: |
     Lorem ipsum dolor sit amet.
     Second line.
   ---
   id: cl_sensitivity
   default: 3.0
   range:
     min: 0.1
     max: 10.0
   flags: []
   description: |
     Example with a nested mapping and a block sequence.


1. Data model (semantic layer)
------------------------------

A SIML file is a **document stream**: one or more documents separated by
``---`` lines.

Each document is a single YAML node of one of these kinds:

* **Mapping**: ordered list of (key → value) entries
* **Sequence**: ordered list of values
* **Scalar**: string

SIML has **no implicit typing**. All scalars are strings; interpretation as
int/float/enum is up to the consumer.

Scalar presentation forms:

* **Plain scalar**: single line text (SIML “plain” constraints apply)
* **Literal block scalar**: multi-line string introduced by ``|``

Sequence presentation forms:

* **Block sequence**: classic YAML dash form
* **Flow sequence**: single-line bracket form ``[a,b,c]`` only - no multi line.

Mappings have only one form: **block mapping**.

There are no flow mappings (``{...}``) and no quoted scalars in SIML.


2. Concrete syntax and round-tripping
-------------------------------------

SIML defines a **concrete syntax** that includes both structure and trivia.
A conforming round-tripping parser MUST preserve:

* document order
* mapping entry order
* sequence item order
* for each scalar: its exact text content
* for each node: whether it was written as:
  - plain scalar vs literal block scalar
  - block sequence vs flow sequence
* all **trivia lines** (blank lines and comment lines), **exactly** as they
  appeared, including indentation and spacing.
* for each **inline comment**, the exact number of spaces that preceded the
  comment ``#`` on that line (see 5.3).

Because SIML mandates **canonical structural formatting**, a serializer never
has to “choose formatting”: it only replays stored trivia and prints structure
in the only allowed way. Therefore parse→serialize is byte-for-byte identical.


3. Encoding, line endings, whitespace
-------------------------------------

3.1 Encoding
~~~~~~~~~~~~

* UTF-8 text, no BOM.

3.2 Line endings (LF only)
~~~~~~~~~~~~~~~~~~~~~~~~~~

SIML uses **LF** line endings only.

* Every line MUST end with a single ``\n`` (LF).
* The byte sequence ``\r\n`` (CRLF) is **forbidden** anywhere in a ``.siml``
  file and MUST cause a parsing error.
* A standalone ``\r`` (CR) is also **forbidden** anywhere in a ``.siml`` file
  and MUST cause a parsing error.

(Implementation note: a streaming parser can enforce this by rejecting any
input line that ends with ``\r\n`` or contains ``\r`` before the terminating
``\n``.)

For perfect round-trip:

* A conforming serializer MUST emit ``\n`` as the line terminator for all lines.

3.3 Indentation
~~~~~~~~~~~~~~~

* Indentation is **spaces only**, never tabs.
* Indentation is **exactly two spaces per nesting level**.
* Indentation levels are therefore 0, 2, 4, 6, …

SIML uses indentation only for **block mappings**, **block sequences**, and
**literal block scalar content**.

3.4 Trailing spaces
~~~~~~~~~~~~~~~~~~~

Outside literal block scalar content (section 7), SIML forbids trailing spaces.

* Every non-block-content line MUST end immediately at the last non-space
  character (followed by the LF).


4. Document stream
------------------

4.1 Document separators
~~~~~~~~~~~~~~~~~~~~~~~

Documents are separated by a line that is exactly:

* ``---``

Optionally, a separator line may include a trailing inline comment in the form::

  ---<N spaces>#<one space>comment text

Examples::

  --- # doc comment
  ---      # aligned doc comment

where ``N >= 1``. The exact number of spaces ``N`` preceding ``#`` is preserved
for perfect round-trip.

Separators:

* MUST appear at indentation level 0
* MUST NOT appear inside literal block scalar content
* A trailing ``---`` after the last document is NOT allowed.

4.2 Leading trivia
~~~~~~~~~~~~~~~~~~

Blank lines and comment lines may appear:

* before the first document,
* between documents (before or after ``---``),
* inside documents between nodes (subject to indentation rules).


5. Trivia lines (blank lines and comments)
------------------------------------------

Trivia is recognized only **outside** literal block scalar content.

5.1 Blank lines
~~~~~~~~~~~~~~~

A blank line is an empty line (zero characters before the LF).

5.2 Comment lines
~~~~~~~~~~~~~~~~~

A comment line is any line whose first non-space character is ``#``.

* Comment lines may be indented (using only spaces).
* Comment lines are trivia; they do not affect structure.
* Comment lines are preserved verbatim for round-trip.

5.3 Inline comments (alignment-preserving)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Inline comments are allowed only on **structural lines that already contain an
inline value** (plain scalar, flow sequence, or block scalar marker). They are
not allowed on “header-only” lines (``key:`` or ``-``).

An inline comment starts at the first ``#`` on the line that is:

* preceded by **at least one space**, and
* (for flow sequences) occurs **after** the closing ``]`` of the flow sequence.

Inline comment formatting is:

* **N spaces** immediately before ``#`` (``N >= 1``), and
* **exactly one space** immediately after ``#``, then
* zero or more characters of comment text (up to end of line).

Examples::

  default: 1 # integer as string
  flags: [A,B]      # aligned with other fields
  description: |  # block follows

The parser MUST preserve, for each inline comment occurrence:

* the integer ``N`` (how many spaces preceded ``#``), and
* the comment text (everything after the required single space after ``#``).

Value storage rules with inline comments:

* The stored value is the text before the inline comment, with trailing spaces
  removed (those trailing spaces are exactly the ``N`` spaces before ``#``).

Notes:

* In a plain scalar value, a ``#`` that is NOT preceded by whitespace is literal
  text, not a comment: ``mode: fast#1`` stores ``fast#1``.
* For flow sequences, inline comments may only follow the complete ``[...]``.


6. Node forms and structure
---------------------------

A document is a single node. SIML supports three node kinds:

* mapping
* sequence
* scalar

A node is introduced either:

* as the document root, or
* as the value of a mapping entry, or
* as a sequence item.

The node kind is determined by its introducing line and (for header-only forms)
the next non-trivia line.

6.1 Structural lines
~~~~~~~~~~~~~~~~~~~~

Outside literal block scalar content, every non-trivia line must be one of:

* document separator: ``---`` (indent 0 only)
* mapping entry line (section 6.2)
* sequence item line (section 6.3)

Any other non-trivia line is an error.

6.2 Mappings
~~~~~~~~~~~~

A mapping is an ordered list of entries written as mapping entry lines.

A mapping entry line is:

* indentation (multiple of 2 spaces),
* key,
* ``:``,
* either:
  - **header-only**: end of line (no trailing spaces), or
  - **inline value**: single space then an inline value (section 6.4)

Header-only mapping entry line:

* Must be exactly ``key:`` and end immediately (no spaces).
* Introduces a nested node whose first content line must appear at
  indentation ``(key_indent + 2)``.
* Inline comments are NOT allowed on header-only mapping entry lines.

A header-only entry MUST be followed by at least one nested structural line
(mapping entry or sequence item) at the required indentation. Empty mappings
are not representable in SIML and are therefore not allowed.

6.3 Sequences (block form)
~~~~~~~~~~~~~~~~~~~~~~~~~~

A block sequence is an ordered list of items written as sequence item lines.

A sequence item line is:

* indentation (multiple of 2 spaces),
* either:
  - **header-only**: ``-`` and end of line (no trailing spaces), or
  - **inline value**: ``-`` then single space then an inline value (section 6.4)

Header-only sequence item line:

* Must be exactly ``-`` and end immediately (no spaces).
* Introduces a nested node whose first content line must appear at
  indentation ``(dash_indent + 2)``.
* Inline comments are NOT allowed on header-only sequence item lines.

A header-only item MUST be followed by at least one nested structural line at
the required indentation. Empty sequences are representable only as ``[]``
(flow form); an empty block sequence is not allowed.

6.4 Inline values (scalar / flow sequence / literal marker)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An inline value appears only after:

* ``key: `` (key, colon, single space), or
* ``- `` (dash, single space)

An inline value is exactly one of:

* **literal block marker**: ``|``
* **flow sequence**: ``[...]`` (single line only; section 6.5)
* **plain scalar**: any other non-empty text (section 7.1)

Inline values MUST be non-empty.

Inline comments may follow inline values per section 5.3.

6.5 Flow sequences (single-line ``[...]`` only)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Flow sequences are allowed only as a single-line inline value. Multi-line
bracket form is forbidden.

FORBIDDEN::

  foo: [
    1,
    2,
  ]

ALLOWED::

  foo: [1,2]
  empty: []
  foo: [1,2]      # aligned comment

Rules:

* The entire sequence MUST be on one line.
* The first character is ``[`` and the last character before any inline comment
  is ``]``.
* **No whitespace is allowed anywhere inside** the brackets.
* Elements are separated by commas.
* Empty list is exactly ``[]``.
* Non-empty lists have the form ``[elem,elem,...]`` with no trailing comma.

Flow element constraints:

* Each element is a **simple scalar atom**:
  - must be non-empty
  - must not contain ``,``, ``]``, or newline
  - must not start with ``[`` or ``|``


7. Scalars
----------

SIML scalars are strings and have two presentation forms:

* plain scalar (single line)
* literal block scalar (multi-line, ``|``)

7.1 Plain scalars
~~~~~~~~~~~~~~~~~

A plain scalar is the inline value text when it is neither ``|`` nor a flow
sequence.

Plain scalar text is:

* the characters after ``key: `` or ``- `` up to:
  - the start of an inline comment as defined in section 5.3 (if present),
    otherwise end of line.

Constraints:

* must be non-empty
* must not contain newline
* must not start with ``[`` or ``|``

The character ``#`` is allowed as literal text if it does not start an inline
comment per section 5.3.

7.2 Literal block scalars (``|``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A literal block scalar is introduced by an inline value exactly equal to ``|``:

* ``key: |``
* ``- |``

(An inline comment may follow ``|`` using the inline comment rules in 5.3.)

Block scalar content rules:

* Let ``H`` be the indentation of the header line.
* The content consists of all following lines until the first **non-empty**
  line whose indentation is **less than** ``H + 2``, or until EOF.
* Every non-empty content line MUST start with exactly ``H + 2`` spaces.
  The parser strips exactly those ``H + 2`` spaces and takes the remainder
  verbatim as the content line text.
* Empty lines (completely empty) are allowed and are part of the content.
* SIML does **not** trim trailing blank lines in the block.

Inside literal block scalar content:

* No comment parsing is performed.
* Any characters are allowed (including ``#``, ``:``, ``---``, commas, tabs).


8. Nesting and termination (indent stack rules)
-----------------------------------------------

SIML nesting is defined purely by indentation, using an indentation stack.

* A nested node introduced by ``key:`` or ``-`` must start at exactly
  parent indentation + 2 spaces.
* When the next non-trivia line has indentation less than the current container,
  the current container ends and parsing resumes at the matching parent level.
* A document ends at:
  - a document separator ``---`` at indentation 0, or
  - EOF.

Because indentation is fixed (2 spaces per level) and structure lines are
restricted, a streaming parser only needs:

* the current mode (mapping/sequence/block-scalar),
* an indentation stack (small integer array),
* one-line lookahead when completing header-only entries/items.


9. Key syntax
-------------

Keys are unquoted identifiers:

* regex: ``[a-zA-Z_][a-zA-Z0-9_.-]*``

Keys:

* appear only in mapping entry lines,
* must start immediately after the indentation (no leading extra spaces),
* must be unique within a mapping (duplicate keys are an error).


10. Disallowed YAML features (non-goals)
----------------------------------------

SIML forbids:

* flow mappings: ``{...}``
* multi-line flow sequences: any ``[`` that is not closed on the same line
* quoted scalars (single or double quotes)
* anchors, aliases, tags, directives (``%YAML``), and ``...`` terminators
* implicit typing at the syntax level (null/bool/number are not special)
* tabs for indentation
* trailing spaces outside literal block scalar content
* arbitrary YAML “compact” forms such as ``- key: value`` in sequences
  (in SIML, sequence items are either ``- <inline>`` or ``-`` followed by a
  nested node)


11. Informal grammar (EBNF-ish)
-------------------------------

.. code-block:: text

   stream        ::= trivia* document (separator trivia* document)* trivia* EOF

   separator     ::= '---' ( spaces1plus '# ' TEXT )? EOL

   document      ::= node_at_indent(0)

   node          ::= mapping | sequence | scalar

   mapping       ::= entry (trivia* entry)*
   entry         ::= INDENT key ':' EOL node_at_indent(INDENT+2)
                  |  INDENT key ':' ' ' inline_value inline_comment? EOL

   sequence      ::= item (trivia* item)*
   item          ::= INDENT '-' EOL node_at_indent(INDENT+2)
                  |  INDENT '-' ' ' inline_value inline_comment? EOL

   inline_value  ::= '|'               ; literal block scalar
                  |  flow_seq
                  |  plain_scalar

   inline_comment ::= spaces1plus '# ' TEXT

   flow_seq      ::= '[' ']' | '[' atom (',' atom)* ']'
   atom          ::= 1*(CHAR_EXCEPT_NEWLINE_COMMA_RBRACKET)
                     AND not starting with '[' or '|'

   plain_scalar  ::= 1*(CHAR_EXCEPT_NEWLINE)
                     AND not starting with '[' or '|'
                     AND with optional inline_comment removed

   trivia        ::= blank_line | comment_line
   blank_line    ::= EOL
   comment_line  ::= INDENT? '#' TEXT EOL

Literal block scalar content is defined operationally in section 7.2.


12. Implementation intent
-------------------------

These restrictions are chosen so a reference parser can be implemented as:

* a line-by-line state machine (``fgets``),
* plus a small indentation stack,
* plus minimal scanning (``strncmp``, ``strchr``),
* with round-trip support by storing:
  - node style (block vs flow sequences; plain vs literal scalars)
  - trivia lines verbatim in their encountered positions
  - for each inline comment: the count of spaces preceding ``#`` (alignment),
    and the comment text after the required ``"# "`` prefix.

Because structural formatting is otherwise canonical (fixed indentation, fixed
spacing, no trailing spaces, no multi-line flow syntax), emitting structure is
trivial and deterministic, enabling perfect round-trip of any valid ``.siml``
file.
