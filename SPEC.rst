=======================================
SIML (Simple Item Markup Language) v0.1
=======================================

SIML is a strict, line-oriented subset of YAML 1.2 designed so that a streaming
reference parser can be written in short, clean ANSI C and that it can be
**perfectly round-tripped**, such that a parser can 1:1 regenerate the input
byte-by-byte.

Example
=======

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


Requirements Language
=====================

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT",
"SHOULD", "SHOULD NOT", "RECOMMENDED", "NOT RECOMMENDED", "MAY", and
"OPTIONAL" in this document are to be interpreted as described in BCP
14 [RFC2119] [RFC8174] when, and only when, they appear in all
capitals, as shown here.

Byte lengths
------------

All maximum lengths in this specification are measured in **bytes** of the
UTF-8 encoded input, unless explicitly stated otherwise.


1. Data model (semantic layer)
==============================

A SIML file is a **document stream**: it MUST contain one or more documents
separated by ``---`` lines.

Each document MUST be exactly one node of one of these kinds:

* **Mapping**: ordered list of (key → value) entries
* **Sequence**: ordered list of values

Documents MUST be non-empty:

* A document mapping MUST have at least one entry.
* A document sequence MUST have at least one item.


2. Concrete syntax and round-tripping
=====================================

SIML defines a **concrete syntax** that includes both structure and trivia.
A conforming round-tripping parser MUST preserve:

* document order
* mapping entry order
* sequence item order
* for each scalar: its exact text content
* for each node: whether it was written as:
  - plain scalar vs literal block scalar
  - block sequence vs flow sequence
* all **comment lines**, **exactly** as they appeared, including indentation and
  spacing.
* for each **inline comment**, the exact number of spaces that preceded the
  comment ``#`` on that line (see 5.2).

Because SIML mandates **canonical structural formatting**, a serializer never
has to “choose formatting”: it only replays stored comment lines and prints
structure in the only allowed way. Therefore parse→serialize MUST be
byte-for-byte identical for any valid ``.siml`` file.


3. Encoding, line endings, whitespace
=====================================

3.1 Encoding
------------

* Input MUST be UTF-8 text and MUST NOT contain a BOM.

3.2 Line endings (LF only)
--------------------------

SIML uses **LF** line endings only.

* Every line MUST end with a single ``\n`` (LF).
* The byte sequence ``\r\n`` (CRLF) MUST NOT appear anywhere in a ``.siml`` file
  and MUST cause a parsing error.
* A standalone ``\r`` (CR) MUST NOT appear anywhere in a ``.siml`` file and MUST
  cause a parsing error.

(Implementation note: a streaming parser can enforce this by rejecting any
input line that ends with ``\r\n`` or contains ``\r`` before the terminating
``\n``.)

For perfect round-trip:

* A conforming serializer MUST emit ``\n`` as the line terminator for all lines.

3.3 Indentation
---------------

* Indentation MUST be spaces only (``0x20``), never tabs.
* Indentation MUST be exactly two spaces per nesting level (0, 2, 4, 6, …).

SIML uses indentation only for **block mappings**, **block sequences**, and
**literal block scalar content**.

Tabs:

* Outside literal block scalar content (section 7.2), a tab byte (``0x09``)
  MUST NOT appear anywhere and MUST cause a parsing error.
* Inside literal block scalar content, tab bytes MAY appear as part of the
  scalar text.

3.4 Forbidden blank lines and whitespace-only lines
---------------------------------------------------

SIML forbids blank lines and whitespace-only lines everywhere, except that
**blank lines** are allowed **only** inside literal block scalar content
(section 7.2) and only **between non-blank lines**.

Outside that exception, SIML forbids:

* **Blank lines** (an empty line: zero characters before the LF).
* **Lines containing only spaces or tabs**.

Any such line MUST cause a parsing error.

3.5 Trailing spaces
-------------------

SIML forbids trailing spaces.

* Every line MUST end immediately at the last non-space
  character (followed by the LF).

3.6 Maximum physical line length
--------------------------------

To support fixed-size, streaming parsers, SIML defines a maximum physical line
length.

* The length of any single physical line, **excluding** its terminating LF,
  MUST be at most **4608 bytes**.
* Any line longer than this limit MUST cause a parsing error.

This maximum applies to **all** lines, including:

* structural lines (mapping entries, sequence items, document separators),
* comment lines,
* and literal block scalar content lines.


4. Document stream
==================

4.1 Document separators
-----------------------

Documents are separated by a line that is exactly:

* ``---``

* MUST appear at indentation level 0
* MUST NOT have inline comments
* MUST NOT appear trailing after the last document
* MUST NOT appear leading before the first document

4.2 Leading and inter-document trivia
-------------------------------------

Only comment lines (section 5) MAY appear:

* before the first document,
* between documents (before or after ``---``),
* inside documents between nodes (subject to indentation rules).

Blank lines and whitespace-only lines MUST NOT appear outside block scalar
content.


5. Comment lines and inline comments
====================================

Comments are recognized only **outside** literal block scalar content.

Empty comments are forbidden (both comment lines and inline comments).

5.1 Comment lines
-----------------

A comment line is a line of the form:

* indentation (zero or more spaces; MUST be a multiple of 2) MUST match current
  nesting level,
* ``#``,
* **exactly one space**,
* **one or more** characters of comment text (up to end of line).

Additional size limits:

* The comment text (the bytes after the required ``"# "`` prefix) MUST be at
  most **512 bytes**.

Rules:

* Comment line indentation MUST be a multiple of 2 spaces (0, 2, 4, …).
* Comment line indentation MUST use spaces only; tabs MUST NOT be used.
* Comment lines are trivia; they MUST NOT affect structure.
* Comment lines MUST be preserved verbatim for round-trip.

5.2 Inline comments (alignment-preserving)
------------------------------------------

An inline comment starts at the first ``#`` on the line that is:

* preceded by **at least one space**, and
* (for flow sequences) occurs **after** the closing ``]`` of the flow sequence.

Inline comment formatting is:

* **N spaces** immediately before ``#`` (``N >= 1``), and
* **exactly one space** immediately after ``#``, then
* **one or more** characters of comment text (up to end of line).

Additional size limits:

* ``N`` (spaces immediately before ``#``) MUST be in the range **1..255**.
* The inline comment text (the bytes after the required ``"# "`` prefix) MUST be
  at most **256 bytes**.

Examples::

  coolobj: # an important object
  default: 1 # integer as string
  flags: [A,B]      # aligned with other fields
  description: |  # block follows

A parser MUST preserve, for each inline comment occurrence:

* the integer ``N`` (how many spaces preceded ``#``), and
* the comment text (everything after the required single space after ``#``).

Value storage rules with inline comments:

* The stored value is the text before the inline comment, with trailing spaces
  removed (those trailing spaces are exactly the ``N`` spaces before ``#``).

Notes:

* In a plain scalar value, a ``#`` that is NOT preceded by whitespace is literal
  text, not a comment: ``mode: fast#1`` stores ``fast#1``.
* For flow sequences, inline comments MAY appear only after the complete
  ``[...]``.


6. Node forms and structure
===========================

A document is a single node, but documents MUST NOT be scalars (section 1).

SIML supports three node kinds:

* mapping
* sequence
* scalar

A node is introduced either:

* as the document root, or
* as the value of a mapping entry, or
* as a sequence item.

The node kind is determined by its introducing line and (for header-only forms)
the next non-comment line.

6.1 Structural lines
--------------------

Outside literal block scalar content, every non-comment line MUST be one of:

* document separator: ``---`` (indent 0 only)
* mapping entry line (section 6.2)
* sequence item line (section 6.3)

Any other line (including blank lines or whitespace-only lines) MUST cause a
parsing error.

6.2 Mappings
------------

Mappings have only one form: **block mapping**.

There are no flow mappings (``{...}``) and no quoted scalars in SIML.

A mapping is an ordered list of entries written as mapping entry lines.

A mapping entry line is:

* indentation (multiple of 2 spaces),
* key,
* ``:``,
* either:
  - **header-only**: end of line (no trailing spaces), or
  - **inline value**: single space then an inline value (section 6.4)

Header-only mapping entry line:

* MUST be exactly ``key:`` and MUST end immediately (no spaces).
* Introduces a nested node whose first content line MUST appear at
  indentation ``(key_indent + 2)``.
* Inline comments MUST NOT appear on header-only mapping entry lines.

A header-only entry MUST be followed by at least one nested structural line
(mapping entry or sequence item) at the required indentation. Empty mappings
are not representable in SIML and are therefore NOT RECOMMENDED at the data
model level; syntactically, a header-only entry with no nested lines is an
error.

6.3 Sequences
-------------

6.3.1 Sequences (block form)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A block sequence is an ordered list of items written as sequence item lines.

A sequence item line is:

* indentation (multiple of 2 spaces),
* either:
  - **header-only**: ``-`` and end of line (no trailing spaces), or
  - **inline value**: ``-`` then single space then an inline value (section 6.4)

Header-only sequence item line:

* MUST be exactly ``-`` and MUST end immediately (no spaces).
* Introduces a nested node whose first content line MUST appear at
  indentation ``(dash_indent + 2)``.
* Inline comments MUST NOT appear on header-only sequence item lines.

A header-only item MUST be followed by at least one nested structural line at
the required indentation. Empty sequences are representable only as ``[]``
(flow form); an empty block sequence MUST NOT be used.

6.3.2 Flow sequences (single-line ``[...]`` only)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Flow sequences are allowed only as a single-line inline value. Multi-line
bracket form MUST NOT be used.

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
* The first character MUST be ``[`` and the last character before any inline
  comment MUST be ``]``.
* **No whitespace is allowed anywhere inside** the brackets.
* Elements MUST be separated by commas.
* Empty list MUST be exactly ``[]``.
* Non-empty lists MUST have the form ``[elem,elem,...]`` with no trailing comma.

Flow element constraints:

* Each element is either:
  - a **simple scalar atom**, or
  - a **nested flow sequence** (``[...]``), which follows the same rules.

* Scalar atom rules:
  - MUST be non-empty
  - MUST NOT contain ``,``, ``]``, or newline
  - MUST NOT start with ``|``
  - MUST be at most **128 bytes**

6.4 Inline values (scalar / flow sequence / literal marker)
-----------------------------------------------------------

An inline value appears only after:

* ``key: `` (key, colon, single space), or
* ``- `` (dash, single space)

An inline value is exactly one of:

* **literal block marker**: ``|``
* **flow sequence**: ``[...]`` (single line only; section 6.3.2)
* **plain scalar**: any other non-empty text (section 7.1)

Inline values MUST be non-empty.

Inline comments MAY follow inline values per section 5.2.

Additional size limits:

* The inline value text (the bytes of the value before any inline comment)
  MUST be at most **2048 bytes**.

This limit applies equally to:

* plain scalar values on a single line, and
* the complete single-line ``[...]`` representation of a flow sequence (before
  any inline comment).

6.5 Node kind consistency at a given indentation
------------------------------------------------

Within a node at indentation level ``I`` every non-comment structural line must
be of the same kind, e.g. a **mapping** node MUST only contain **mapping entry
lines** etc.

(Comments MAY appear between structural lines and do not affect this rule.)


7. Scalars
==========

SIML scalars are always strings (interpretation as int/float/enum is up to the
consumer) and have two presentation forms:

* **plain scalar** (single line)
* **literal block scalar** (multi-line, ``|``)

7.1 Plain scalars
-----------------

A plain scalar is the inline value text when it is neither ``|`` nor a flow
sequence.

Plain scalar text is:

* the characters after ``key: `` or ``- `` up to:
  - the start of an inline comment as defined in section 5.2 (if present),
    otherwise end of line.

Constraints:

* MUST be non-empty
* MUST NOT contain newline
* MUST NOT start with ``[`` or ``|``

Additional size limits:

* Plain scalar text on a single line (before any inline comment) MUST be at most
  **2048 bytes** (section 6.4).

The character ``#`` MAY appear as literal text if it does not start an inline
comment per section 5.2.

7.2 Literal block scalars (``|``)
---------------------------------

A literal block scalar is introduced by an inline value exactly equal to ``|``:

* ``key: |``
* ``- |``

(An inline comment MAY follow ``|`` using the inline comment rules in 5.2.)

Block scalar content rules:

* Let ``H`` be the indentation of the header line.
* The content consists of all following lines until the first **non-blank** line
  whose indentation is **less than** ``H + 2``, or until EOF.
* Every **non-blank** content line MUST start with **exactly** ``H + 2`` spaces
  which are stripped.
* Blank lines (empty lines) are allowed **only** when they occur **between
  non-blank content lines**. Leading or trailing blank lines are forbidden.
* Lines containing only spaces or tabs are forbidden.
* Contents MUST be non-empty, i.e. zero content lines MUST be a parsing error

Additional size limits:

* Each literal block scalar **content line**, after stripping the fixed
  indentation ``H + 2``, MUST be at most **4096 bytes** (excluding its
  terminating LF).

Semantic value:

* The semantic value is the concatenation of content lines **including their
  terminating ``\n``** (since every line ends in LF). Blank lines contribute a
  single ``\n`` to the semantic value.

Inside literal block scalar content:

* No comment parsing is performed.
* Any characters are allowed (including ``#``, ``:``, ``---``, commas, and tabs),
  except forbidden CR, which MUST cause a parsing error.


8. Nesting and termination (indent stack rules)
===============================================

SIML nesting is defined purely by indentation, using an indentation stack.

* A nested node introduced by ``key:`` or ``-`` MUST start at exactly
  parent indentation + 2 spaces.
* When the next non-comment line has indentation less than the current node,
  the current node ends and parsing resumes at the matching parent level.
* A document ends at:
  - a document separator ``---`` at indentation 0, or
  - EOF.

Maximum nesting depth:

* The maximum nesting depth (number of simultaneously-open nodes)
  MUST be at most **32**.
* Inputs that would require a deeper indentation stack MUST cause a parsing
  error.

Because indentation is fixed (2 spaces per level) and structure lines are
restricted, a streaming parser only needs:

* the current mode (mapping/sequence/block-scalar),
* an indentation stack (small integer array),
* one-line lookahead when completing header-only entries/items.


9. Key syntax
=============

Keys are unquoted identifiers:

* regex: ``[a-zA-Z_][a-zA-Z0-9_.-]*``

Additional size limits:

* A key MUST be at most **128 bytes**.

Keys:

* appear only in mapping entry lines,
* MUST start immediately after the indentation (no leading extra spaces),


10. Error Messages
==================

Implementations MUST have the following **separate** error messages:

Line endings and physical line length:

* UTF-8 BOM is forbidden
* CRLF is forbidden (``\r\n`` found)
* CR is forbidden (``\r`` found)
* physical line too long (max 4608 bytes)

Whitespace rules:

* blank lines are not allowed here
* whitespace-only lines are not allowed here
* tabs are not allowed here
* trailing spaces are not allowed here

Document stream:

* document separator must be exactly ``---``
* document separator must be at indent 0
* document separator must not have inline comments
* document separator must not appear before the first document
* document separator must not appear after the last document
* document must start at indent 0
* document root must not be a scalar

Indentation and nesting:

* indentation must be a multiple of 2 spaces
* wrong indentation, expected: X
* nested node indentation mismatch, expected X got Y
* node kind mixing at indent X is forbidden

Keys and mapping entries:

* illegal mapping key, must match: [a-zA-Z_][a-zA-Z0-9_.-]*
* mapping key too long (max 128 bytes)
* expected single space after ':'
* header-only mapping entry must not have inline comments
* header-only mapping entry must have a nested node

Sequence items:

* expected single space after '-'
* header-only sequence item must not have inline comments
* header-only sequence item must have a nested node

Comments:

* empty comment is forbidden
* comment indentation must match current nesting level
* comment text too long (max 512 bytes)
* inline comment alignment out of range (1..255 spaces)
* inline comment must have exactly 1 space after '#'
* inline comment text too long (max 256 bytes)

Inline values and flow sequences:

* inline value is empty
* inline value too long (max 2048 bytes)
* multi-line flow sequences are forbidden
* unterminated flow sequence
* flow sequence contains whitespace (forbidden)
* empty flow sequence element
* trailing comma in flow sequence is forbidden
* flow sequence atom too long (max 128 bytes)

Literal block scalars:

* block literal must not be empty
* block literal content line has wrong indentation
* block literal has leading blank line (forbidden)
* block literal has trailing blank line (forbidden)
* block literal content line too long (max 4096 bytes)
* whitespace-only lines are forbidden in block literal content


11. Disallowed YAML features (non-goals)
========================================

SIML forbids (MUST NOT support):

* flow mappings: ``{...}``
* multi-line flow sequences: any ``[`` that is not closed on the same line
* quoted scalars (single or double quotes)
* anchors, aliases, tags, directives (``%YAML``), and ``...`` terminators
* implicit typing at the syntax level (null/bool/number are not special)
* tabs outside literal block scalar content
* blank lines outside literal block scalar content
* lines containing only spaces or tabs outside literal block scalar content
* empty comments (``#`` / ``#␠``) anywhere outside literal block scalar content
* arbitrary YAML “compact” forms such as ``- key: value`` in sequences
  (in SIML, sequence items MUST be either ``- <inline>`` or ``-`` followed by a
  nested node)

12. Informal grammar (EBNF-ish)
===============================

.. code-block:: text

   stream         ::= comment* document (separator comment* document)* comment* EOF

   ; Per section 4.1, '---' MUST NOT have inline comments.
   separator      ::= '---' EOL

   document       ::= non_scalar_node_at_indent(0)

   non_scalar_node ::= mapping | sequence
   node            ::= mapping | sequence | scalar

   mapping        ::= entry (comment* entry)*
   entry          ::= INDENT key ':' EOL node_at_indent(INDENT+2)
                   |  INDENT key ':' ' ' inline_value inline_comment? EOL

   sequence       ::= item (comment* item)*
   item           ::= INDENT '-' EOL node_at_indent(INDENT+2)
                   |  INDENT '-' ' ' inline_value inline_comment? EOL

   inline_value   ::= '|'               ; literal block scalar
                   |  flow_seq
                   |  plain_scalar

   inline_comment ::= spaces1plus '# ' NONEMPTY_TEXT

   flow_seq       ::= '[' ']' | '[' flow_elem (',' flow_elem)* ']'
   flow_elem      ::= flow_seq | atom
   atom           ::= 1*(CHAR_EXCEPT_NEWLINE_COMMA_RBRACKET)
                      AND not starting with '|'

   plain_scalar   ::= 1*(CHAR_EXCEPT_NEWLINE)
                      AND not starting with '[' or '|'
                      AND with optional inline_comment removed

   comment        ::= comment_line
   comment_line   ::= INDENT2N? '# ' NONEMPTY_TEXT EOL

   INDENT2N       ::= 0 | 2 | 4 | ... spaces (no tabs)

Blank lines and whitespace-only lines are forbidden outside literal block scalar
content and therefore do not appear in the grammar.

Literal block scalar content is defined operationally in section 7.2.

Maximum sizes (key length, inline value length, comment lengths, etc.) are
defined normatively in the corresponding sections and are not expressed in this
grammar.


13. Implementation intent
=========================

These restrictions are chosen so a reference parser can be implemented as:

* a line-by-line state machine (``fgets``),
* plus a small indentation stack,
* plus minimal scanning (``strncmp``, ``strchr``),
* with round-trip support by storing:
  - node style (block vs flow sequences; plain vs literal scalars)
  - comment lines verbatim in their encountered positions
  - for each inline comment: the count of spaces preceding ``#`` (alignment),
    and the (non-empty) comment text after the required ``"# "`` prefix.

Because structural formatting is otherwise canonical (fixed indentation, fixed
spacing, no trailing spaces, no multi-line flow syntax), emitting structure is
trivial and deterministic, enabling perfect round-trip of any valid ``.siml``
file.

14. Reference Implementation
============================

* Header only.
* Pure ANSI C89.
* No dynamic allocation.
* No I/O. The caller provides a line-reading callback.
* Pull parser API: the caller repeatedly calls siml_next() to obtain events.
* Events are Variants (Tagged Unions) that can be easily stored by consumers and
  replayed to generate the exact same siml byte-by-byte.
