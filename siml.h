#ifndef SIML_H_INCLUDED
#define SIML_H_INCLUDED

/*
 * SIML reference parser v0.1
 *
 * Header-only, pure ANSI C89 implementation.
 *
 * - No dynamic allocation.
 * - No I/O. The caller provides a line-reading callback.
 * - Pull parser API: the caller repeatedly calls siml_next() to obtain events.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h> /* size_t */

#ifndef SIML_MAX_KEY_LEN
#define SIML_MAX_KEY_LEN 128
#endif

#ifndef SIML_MAX_NESTING
#define SIML_MAX_NESTING 32
#endif

#ifndef SIML_MAX_LINE_LEN
#define SIML_MAX_LINE_LEN 4608
#endif

#ifndef SIML_MAX_INLINE_VALUE_LEN
#define SIML_MAX_INLINE_VALUE_LEN 2048
#endif

#ifndef SIML_MAX_FLOW_ELEMENT_LEN
#define SIML_MAX_FLOW_ELEMENT_LEN 128
#endif

#ifndef SIML_MAX_COMMENT_TEXT_LEN
#define SIML_MAX_COMMENT_TEXT_LEN 512
#endif

#ifndef SIML_MAX_INLINE_COMMENT_TEXT_LEN
#define SIML_MAX_INLINE_COMMENT_TEXT_LEN 256
#endif

#ifndef SIML_MAX_INLINE_COMMENT_SPACES
#define SIML_MAX_INLINE_COMMENT_SPACES 255
#endif

#ifndef SIML_MAX_BLOCK_LINE_LEN
#define SIML_MAX_BLOCK_LINE_LEN 4096
#endif

/* Error codes */
typedef enum siml_error_code {
    SIML_ERR_NONE = 0,
    SIML_ERR_IO,
    SIML_ERR_UTF8_BOM,
    SIML_ERR_CRLF,
    SIML_ERR_CR,
    SIML_ERR_LINE_TOO_LONG,
    SIML_ERR_BLANK_LINE,
    SIML_ERR_WHITESPACE_ONLY,
    SIML_ERR_TABS,
    SIML_ERR_TRAILING_SPACES,
    SIML_ERR_SEPARATOR_FORMAT,
    SIML_ERR_SEPARATOR_INDENT,
    SIML_ERR_SEPARATOR_INLINE_COMMENT,
    SIML_ERR_SEPARATOR_BEFORE_DOC,
    SIML_ERR_SEPARATOR_AFTER_DOC,
    SIML_ERR_DOC_INDENT,
    SIML_ERR_DOC_SCALAR,
    SIML_ERR_INDENT_MULTIPLE,
    SIML_ERR_INDENT_WRONG,
    SIML_ERR_INDENT_NEST_MISMATCH,
    SIML_ERR_NODE_KIND_MIX,
    SIML_ERR_KEY_ILLEGAL,
    SIML_ERR_KEY_TOO_LONG,
    SIML_ERR_EXPECT_SPACE_AFTER_COLON,
    SIML_ERR_HEADER_MAP_INLINE_COMMENT,
    SIML_ERR_HEADER_MAP_NO_NESTED,
    SIML_ERR_EXPECT_SPACE_AFTER_DASH,
    SIML_ERR_HEADER_SEQ_INLINE_COMMENT,
    SIML_ERR_HEADER_SEQ_NO_NESTED,
    SIML_ERR_EMPTY_COMMENT,
    SIML_ERR_COMMENT_INDENT,
    SIML_ERR_COMMENT_TOO_LONG,
    SIML_ERR_INLINE_COMMENT_ALIGN,
    SIML_ERR_INLINE_COMMENT_SPACE,
    SIML_ERR_INLINE_COMMENT_TOO_LONG,
    SIML_ERR_INLINE_VALUE_EMPTY,
    SIML_ERR_INLINE_VALUE_TOO_LONG,
    SIML_ERR_FLOW_MULTI_LINE,
    SIML_ERR_FLOW_UNTERMINATED,
    SIML_ERR_FLOW_UNTERMINATED_SAME_LINE,
    SIML_ERR_FLOW_TRAILING_CHARS,
    SIML_ERR_FLOW_WHITESPACE,
    SIML_ERR_FLOW_EMPTY_ELEM,
    SIML_ERR_FLOW_TRAILING_COMMA,
    SIML_ERR_FLOW_ATOM_TOO_LONG,
    SIML_ERR_BLOCK_EMPTY,
    SIML_ERR_BLOCK_WRONG_INDENT,
    SIML_ERR_BLOCK_LEADING_BLANK,
    SIML_ERR_BLOCK_TRAILING_BLANK,
    SIML_ERR_BLOCK_LINE_TOO_LONG,
    SIML_ERR_BLOCK_WHITESPACE_ONLY
} siml_error_code;

/* Event types for the pull parser */
typedef enum siml_event_type {
    SIML_EVENT_NONE = 0,
    SIML_EVENT_STREAM_START,
    SIML_EVENT_DOCUMENT_START,
    SIML_EVENT_MAPPING_START,
    SIML_EVENT_SEQUENCE_START,
    SIML_EVENT_SCALAR,
    SIML_EVENT_BLOCK_SCALAR_START,
    SIML_EVENT_BLOCK_SCALAR_LINE,
    SIML_EVENT_BLOCK_SCALAR_END,
    SIML_EVENT_SEQUENCE_END,
    SIML_EVENT_MAPPING_END,
    SIML_EVENT_DOCUMENT_END,
    SIML_EVENT_STREAM_END,
    SIML_EVENT_COMMENT,
    SIML_EVENT_ERROR
} siml_event_type;

typedef enum siml_seq_style {
    SIML_SEQ_STYLE_BLOCK = 0,
    SIML_SEQ_STYLE_FLOW  = 1
} siml_seq_style;

/* String slice referencing caller-owned memory */
typedef struct siml_slice_s {
    const char *ptr;
    size_t      len;
} siml_slice;

/* Read-next-line callback: must return
 *   >0 : success, *out_line / *out_len set
 *    0 : end of stream (EOF)
 *   <0 : error
 *
 * The returned line must NOT include the trailing newline.
 * The memory must remain valid until the next call to the callback.
 */
typedef int (*siml_read_line_fn)(void *userdata,
                                 const char **out_line,
                                 size_t *out_len);

/* Event structure filled by siml_next().
 *
 *  - key/value are empty slices for stream/document/container events.
 *  - For mapping values, key is the mapping key on the introducing event.
 *  - For sequence items, key is empty.
 *  - For SCALAR, value is the scalar text.
 *  - For BLOCK_SCALAR_LINE, value is the line text (without indent).
 *  - For COMMENT, value is the entire line (without the trailing LF).
 *  - inline_comment_* are set only on events introduced by inline values.
 */
typedef struct siml_event_s {
    siml_event_type  type;
    siml_slice       key;
    siml_slice       value;
    siml_slice       inline_comment;
    unsigned int     inline_comment_spaces; /* 0 if none */
    siml_seq_style   seq_style;             /* for SEQUENCE_START */
    long             line;                  /* 1-based physical line number */
    siml_error_code  error_code;
    const char      *error_message;         /* static string; never NULL for ERROR */
} siml_event;

/* Internal types */
typedef enum siml_mode_e {
    SIML_MODE_NORMAL = 0,
    SIML_MODE_FLOW,
    SIML_MODE_BLOCK
} siml_mode;

typedef enum siml_container_type_e {
    SIML_CONTAINER_MAP = 0,
    SIML_CONTAINER_SEQ = 1
} siml_container_type;

typedef struct siml_container_s {
    siml_container_type type;
    size_t              indent;
    int                 item_count;
} siml_container;

typedef enum siml_pending_kind_e {
    SIML_PENDING_NONE = 0,
    SIML_PENDING_MAP,
    SIML_PENDING_SEQ
} siml_pending_kind;

/* Parser state */
typedef struct siml_parser_s {
    /* User-supplied input */
    siml_read_line_fn read_line;
    void             *userdata;

    /* Current physical line */
    const char       *line;
    size_t            line_len;
    long              line_no;
    int               have_line;   /* boolean */
    int               at_eof;      /* boolean */

    /* High-level document state */
    int               started;
    int               in_document;
    int               seen_document;
    int               awaiting_document;

    /* Current mode */
    siml_mode         mode;

    /* Container stack */
    siml_container    stack[SIML_MAX_NESTING];
    int               depth;

    /* Pending header-only value */
    siml_pending_kind pending_kind;
    size_t            pending_indent;
    char              pending_key[SIML_MAX_KEY_LEN + 1];
    size_t            pending_key_len;

    /* Pending end/start events */
    int               pending_close;
    int               target_depth;
    int               pending_doc_end;
    int               pending_doc_start;
    int               pending_container_start;
    siml_container_type pending_container_type;
    siml_seq_style    pending_seq_style;
    char              pending_container_key[SIML_MAX_KEY_LEN + 1];
    size_t            pending_container_key_len;
    int               pending_stream_end;

    /* Flow sequence parsing state */
    int               flow_depth;
    size_t            flow_stack_start[SIML_MAX_NESTING];
    size_t            flow_stack_end[SIML_MAX_NESTING];
    size_t            flow_stack_pos[SIML_MAX_NESTING];
    int               flow_stack_started[SIML_MAX_NESTING];
    char              flow_key[SIML_MAX_KEY_LEN + 1];
    size_t            flow_key_len;
    unsigned int      flow_inline_spaces;
    const char       *flow_inline_comment;
    size_t            flow_inline_comment_len;

    /* Block scalar parsing state */
    size_t            block_indent;
    char              block_key[SIML_MAX_KEY_LEN + 1];
    size_t            block_key_len;
    unsigned int      block_inline_spaces;
    const char       *block_inline_comment;
    size_t            block_inline_comment_len;
    long              block_start_line;
    int               block_seen_content;
    size_t            block_blank_count;
    long              block_blank_start_line;
    int               block_emit_blanks;

    /* Error state */
    siml_error_code   error_code;
    const char       *error_message;
    char              error_buf[160];
    long              error_line;
} siml_parser;

/* Initialize parser. The parser object can be stack- or statically-allocated.
 * It does not own userdata.
 */
void siml_parser_init(siml_parser *p,
                      siml_read_line_fn read_line,
                      void *userdata);

/* Reset parser to initial state but keep the same read callback and userdata. */
void siml_parser_reset(siml_parser *p);

/* Main pull API: obtain the next event from the stream.
 *
 * Errors are reported as SIML_EVENT_ERROR.
 */
siml_event_type siml_next(siml_parser *p, siml_event *ev);

#ifdef __cplusplus
} /* extern "C" */
#endif

/* ---------------- Implementation ---------------- */
#ifdef SIML_IMPLEMENTATION

#include <string.h> /* memcpy */

/* Internal helpers ------------------------------------------------------ */

static int siml_is_alpha(char c) {
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

static int siml_is_digit(char c) {
    return (c >= '0' && c <= '9');
}

static int siml_is_key_char(char c) {
    return siml_is_alpha(c) || siml_is_digit(c) || c == '_' || c == '-' || c == '.';
}

static int siml_is_space_only(const char *s, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        if (s[i] != ' ') return 0;
    }
    return 1;
}

static int siml_is_space_or_tab_only(const char *s, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        if (s[i] != ' ' && s[i] != '\t') return 0;
    }
    return 1;
}

static void siml_set_error(siml_parser *p, siml_error_code code, const char *msg) {
    if (p->error_code == SIML_ERR_NONE) {
        size_t i = 0;
        p->error_code = code;
        p->error_line = p->line_no;
        if (msg) {
            while (msg[i] != '\0' && i + 1 < sizeof(p->error_buf)) {
                p->error_buf[i] = msg[i];
                ++i;
            }
        }
        p->error_buf[i] = '\0';
        p->error_message = p->error_buf;
    }
}

static size_t siml_append_ulong(char *buf, size_t pos, size_t cap,
                                unsigned long value) {
    char tmp[32];
    size_t n = 0;
    if (value == 0) {
        tmp[n++] = '0';
    } else {
        while (value > 0 && n < sizeof(tmp)) {
            tmp[n++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    while (n > 0 && pos + 1 < cap) {
        buf[pos++] = tmp[--n];
    }
    return pos;
}

static void siml_set_error_one(siml_parser *p, siml_error_code code,
                               const char *prefix, unsigned long value,
                               const char *suffix) {
    size_t i = 0;
    if (p->error_code != SIML_ERR_NONE) return;
    p->error_code = code;
    p->error_line = p->line_no;
    if (prefix) {
        while (prefix[i] != '\0' && i + 1 < sizeof(p->error_buf)) {
            p->error_buf[i] = prefix[i];
            ++i;
        }
    }
    i = siml_append_ulong(p->error_buf, i, sizeof(p->error_buf), value);
    if (suffix) {
        size_t j = 0;
        while (suffix[j] != '\0' && i + 1 < sizeof(p->error_buf)) {
            p->error_buf[i++] = suffix[j++];
        }
    }
    p->error_buf[i] = '\0';
    p->error_message = p->error_buf;
}

static void siml_set_error_two(siml_parser *p, siml_error_code code,
                               const char *prefix, unsigned long a,
                               const char *middle, unsigned long b) {
    size_t i = 0;
    if (p->error_code != SIML_ERR_NONE) return;
    p->error_code = code;
    p->error_line = p->line_no;
    if (prefix) {
        while (prefix[i] != '\0' && i + 1 < sizeof(p->error_buf)) {
            p->error_buf[i] = prefix[i];
            ++i;
        }
    }
    i = siml_append_ulong(p->error_buf, i, sizeof(p->error_buf), a);
    if (middle) {
        size_t j = 0;
        while (middle[j] != '\0' && i + 1 < sizeof(p->error_buf)) {
            p->error_buf[i++] = middle[j++];
        }
    }
    i = siml_append_ulong(p->error_buf, i, sizeof(p->error_buf), b);
    p->error_buf[i] = '\0';
    p->error_message = p->error_buf;
}

static void siml_clear_event(siml_event *ev) {
    ev->type                  = SIML_EVENT_NONE;
    ev->key.ptr               = 0;
    ev->key.len               = 0;
    ev->value.ptr             = 0;
    ev->value.len             = 0;
    ev->inline_comment.ptr    = 0;
    ev->inline_comment.len    = 0;
    ev->inline_comment_spaces = 0;
    ev->seq_style             = SIML_SEQ_STYLE_BLOCK;
    ev->line                  = 0;
    ev->error_code            = SIML_ERR_NONE;
    ev->error_message         = 0;
}

static siml_slice siml_make_slice(const char *p, size_t len) {
    siml_slice s;
    s.ptr = p;
    s.len = len;
    return s;
}

/* Fetch next physical line into parser->line/line_len. Returns:
 *   1 on success, 0 on EOF, -1 on error.
 */
static int siml_fetch_line(siml_parser *p) {
    const char *line;
    size_t len;
    int rc;

    if (p->at_eof) {
        p->have_line = 0;
        return 0;
    }
    rc = p->read_line(p->userdata, &line, &len);
    if (rc > 0) {
        p->line      = line;
        p->line_len  = len;
        p->have_line = 1;
        p->line_no  += 1;
        return 1;
    }
    if (rc == 0) {
        p->at_eof    = 1;
        p->have_line = 0;
        return 0;
    }
    p->have_line = 0;
    siml_set_error(p, SIML_ERR_IO, "read_line IO error");
    return -1;
}

static int siml_check_line_common(siml_parser *p) {
    size_t i;
    const char *s = p->line;
    size_t len = p->line_len;

    if (len > SIML_MAX_LINE_LEN) {
        siml_set_error(p, SIML_ERR_LINE_TOO_LONG,
                       "physical line too long (max 4608 bytes)");
        return 0;
    }
    if (p->line_no == 1 && len >= 3) {
        if ((unsigned char)s[0] == 0xEF &&
            (unsigned char)s[1] == 0xBB &&
            (unsigned char)s[2] == 0xBF) {
            siml_set_error(p, SIML_ERR_UTF8_BOM, "UTF-8 BOM is forbidden");
            return 0;
        }
    }
    for (i = 0; i < len; ++i) {
        if (s[i] == '\r') {
            if (i + 1 == len) {
                siml_set_error(p, SIML_ERR_CRLF, "CRLF is forbidden (\\r\\n found)");
            } else {
                siml_set_error(p, SIML_ERR_CR, "CR is forbidden (\\r found)");
            }
            return 0;
        }
    }
    return 1;
}

static int siml_check_line_nonblock(siml_parser *p) {
    size_t i;
    const char *s = p->line;
    size_t len = p->line_len;

    if (!siml_check_line_common(p)) return 0;

    if (len == 0) {
        siml_set_error(p, SIML_ERR_BLANK_LINE, "blank lines are not allowed here");
        return 0;
    }
    for (i = 0; i < len; ++i) {
        if (s[i] == '\t') {
            siml_set_error(p, SIML_ERR_TABS, "tabs are not allowed here");
            return 0;
        }
    }
    if (siml_is_space_only(s, len)) {
        siml_set_error(p, SIML_ERR_WHITESPACE_ONLY,
                       "whitespace-only lines are not allowed here");
        return 0;
    }
    return 1;
}

static int siml_count_indent(siml_parser *p, const char *s, size_t len,
                             size_t *out_indent) {
    size_t i = 0;
    while (i < len && s[i] == ' ') {
        ++i;
    }
    if ((i % 2) != 0) {
        siml_set_error(p, SIML_ERR_INDENT_MULTIPLE,
                       "indentation must be a multiple of 2 spaces");
        return 0;
    }
    *out_indent = i;
    return 1;
}

static int siml_parse_comment_line(siml_parser *p, const char *s, size_t len,
                                   size_t *out_indent) {
    size_t indent = 0;
    size_t text_len = 0;

    if (!siml_count_indent(p, s, len, &indent)) return -1;
    *out_indent = indent;
    if (indent >= len) return 0;
    if (s[indent] != '#') return 0;
    if (indent + 1 >= len) {
        siml_set_error(p, SIML_ERR_EMPTY_COMMENT, "empty comment is forbidden");
        return -1;
    }
    if (s[indent + 1] != ' ') {
        siml_set_error(p, SIML_ERR_EMPTY_COMMENT, "empty comment is forbidden");
        return -1;
    }
    if (indent + 2 >= len) {
        siml_set_error(p, SIML_ERR_EMPTY_COMMENT, "empty comment is forbidden");
        return -1;
    }
    text_len = len - (indent + 2);
    if (text_len > SIML_MAX_COMMENT_TEXT_LEN) {
        siml_set_error(p, SIML_ERR_COMMENT_TOO_LONG,
                       "comment text too long (max 512 bytes)");
        return -1;
    }
    return 1;
}

static int siml_parse_inline_comment(siml_parser *p,
                                     const char *s,
                                     size_t start,
                                     size_t len,
                                     size_t *out_value_len,
                                     unsigned int *out_spaces,
                                     const char **out_comment,
                                     size_t *out_comment_len) {
    size_t i;
    size_t hash_pos = (size_t)(-1);

    *out_spaces = 0;
    *out_comment = 0;
    *out_comment_len = 0;

    for (i = start; i < len; ++i) {
        if (s[i] == '#') {
            if (i > 0 && s[i - 1] == ' ') {
                hash_pos = i;
                break;
            }
        }
    }

    if (hash_pos == (size_t)(-1)) {
        *out_value_len = len - start;
        return 1;
    }

    {
        size_t j = hash_pos;
        size_t value_end;
        while (j > 0 && s[j - 1] == ' ') {
            --j;
        }
        if (hash_pos <= j) {
            siml_set_error(p, SIML_ERR_INLINE_COMMENT_ALIGN,
                           "inline comment alignment out of range (1..255 spaces)");
            return 0;
        }
        if (hash_pos - j > SIML_MAX_INLINE_COMMENT_SPACES) {
            siml_set_error(p, SIML_ERR_INLINE_COMMENT_ALIGN,
                           "inline comment alignment out of range (1..255 spaces)");
            return 0;
        }
        *out_spaces = (unsigned int)(hash_pos - j);
        value_end = hash_pos - *out_spaces;
        if (value_end < start) value_end = start;
        *out_value_len = value_end - start;
    }

    if (hash_pos + 1 >= len || s[hash_pos + 1] != ' ') {
        siml_set_error(p, SIML_ERR_INLINE_COMMENT_SPACE,
                       "inline comment must have exactly 1 space after '#'");
        return 0;
    }
    if (hash_pos + 2 >= len) {
        siml_set_error(p, SIML_ERR_EMPTY_COMMENT, "empty comment is forbidden");
        return 0;
    }

    *out_comment = s + hash_pos + 2;
    *out_comment_len = len - (hash_pos + 2);
    if (*out_comment_len > SIML_MAX_INLINE_COMMENT_TEXT_LEN) {
        siml_set_error(p, SIML_ERR_INLINE_COMMENT_TOO_LONG,
                       "inline comment text too long (max 256 bytes)");
        return 0;
    }

    return 1;
}

static int siml_push_container(siml_parser *p, siml_container_type type,
                               size_t indent) {
    siml_container *c;

    if (p->depth >= SIML_MAX_NESTING) {
        size_t expected = 0;
        if (p->depth > 0) {
            expected = p->stack[p->depth - 1].indent;
        }
        siml_set_error_one(p, SIML_ERR_INDENT_WRONG,
                           "wrong indentation, expected: ",
                           (unsigned long)expected,
                           "");
        return 0;
    }
    c = &p->stack[p->depth++];
    c->type = type;
    c->indent = indent;
    c->item_count = 0;
    return 1;
}

static int siml_request_container_start(siml_parser *p,
                                        siml_container_type type,
                                        siml_seq_style seq_style,
                                        size_t indent,
                                        const char *key,
                                        size_t key_len) {
    size_t i;

    if (!siml_push_container(p, type, indent)) return 0;
    p->pending_container_start = 1;
    p->pending_container_type = type;
    p->pending_seq_style = seq_style;
    p->pending_container_key_len = key_len;
    for (i = 0; i < key_len && i < SIML_MAX_KEY_LEN; ++i) {
        p->pending_container_key[i] = key[i];
    }
    p->pending_container_key[key_len] = '\0';
    return 1;
}

static siml_event_type siml_emit_pending_end(siml_parser *p, siml_event *ev) {
    if (p->pending_close) {
        if (p->depth > p->target_depth) {
            siml_container c = p->stack[p->depth - 1];
            p->depth -= 1;
            ev->type = (c.type == SIML_CONTAINER_MAP) ? SIML_EVENT_MAPPING_END
                                                      : SIML_EVENT_SEQUENCE_END;
            ev->line = p->line_no;
            return ev->type;
        }
        p->pending_close = 0;
    }
    if (p->pending_doc_end) {
        p->pending_doc_end = 0;
        p->in_document = 0;
        ev->type = SIML_EVENT_DOCUMENT_END;
        ev->line = p->line_no;
        return ev->type;
    }
    if (p->pending_stream_end) {
        p->pending_stream_end = 0;
        ev->type = SIML_EVENT_STREAM_END;
        ev->line = p->line_no;
        return ev->type;
    }
    return SIML_EVENT_NONE;
}

static siml_event_type siml_emit_pending_start(siml_parser *p, siml_event *ev) {
    if (p->pending_doc_start) {
        p->pending_doc_start = 0;
        ev->type = SIML_EVENT_DOCUMENT_START;
        ev->line = p->line_no;
        return ev->type;
    }
    if (p->pending_container_start) {
        p->pending_container_start = 0;
        if (p->pending_container_type == SIML_CONTAINER_MAP) {
            ev->type = SIML_EVENT_MAPPING_START;
        } else {
            ev->type = SIML_EVENT_SEQUENCE_START;
            ev->seq_style = p->pending_seq_style;
        }
        ev->key = siml_make_slice(p->pending_container_key,
                                  p->pending_container_key_len);
        ev->line = p->line_no;
        return ev->type;
    }
    return SIML_EVENT_NONE;
}

/* Parser public functions ----------------------------------------------- */

void siml_parser_init(siml_parser *p,
                      siml_read_line_fn read_line,
                      void *userdata) {
    if (!p) return;
    p->read_line = read_line;
    p->userdata  = userdata;
    siml_parser_reset(p);
}

void siml_parser_reset(siml_parser *p) {
    if (!p) return;
    p->line      = 0;
    p->line_len  = 0;
    p->line_no   = 0;
    p->have_line = 0;
    p->at_eof    = 0;
    p->started   = 0;
    p->in_document = 0;
    p->seen_document = 0;
    p->awaiting_document = 0;
    p->mode      = SIML_MODE_NORMAL;
    p->depth     = 0;
    p->pending_kind = SIML_PENDING_NONE;
    p->pending_indent = 0;
    p->pending_key[0] = '\0';
    p->pending_key_len = 0;
    p->pending_close = 0;
    p->target_depth = 0;
    p->pending_doc_end = 0;
    p->pending_doc_start = 0;
    p->pending_container_start = 0;
    p->pending_container_key[0] = '\0';
    p->pending_container_key_len = 0;
    p->pending_stream_end = 0;
    p->flow_depth = 0;
    p->flow_stack_start[0] = 0;
    p->flow_stack_end[0] = 0;
    p->flow_stack_pos[0] = 0;
    p->flow_stack_started[0] = 0;
    p->flow_key[0] = '\0';
    p->flow_key_len = 0;
    p->flow_inline_spaces = 0;
    p->flow_inline_comment = 0;
    p->flow_inline_comment_len = 0;
    p->block_indent = 0;
    p->block_key[0] = '\0';
    p->block_key_len = 0;
    p->block_inline_spaces = 0;
    p->block_inline_comment = 0;
    p->block_inline_comment_len = 0;
    p->block_start_line = 0;
    p->block_seen_content = 0;
    p->block_blank_count = 0;
    p->block_blank_start_line = 0;
    p->block_emit_blanks = 0;
    p->error_code = SIML_ERR_NONE;
    p->error_message = 0;
    p->error_line = 0;
}

/* Forward declarations of internal state handlers */
static siml_event_type siml_next_normal(siml_parser *p, siml_event *ev);
static siml_event_type siml_next_flow(siml_parser *p, siml_event *ev);
static siml_event_type siml_next_block(siml_parser *p, siml_event *ev);

siml_event_type siml_next(siml_parser *p, siml_event *ev) {
    siml_event_type t;

    if (!p || !ev) return SIML_EVENT_ERROR;

    siml_clear_event(ev);

    if (p->error_code != SIML_ERR_NONE) {
        ev->type          = SIML_EVENT_ERROR;
        ev->error_code    = p->error_code;
        ev->error_message = p->error_message;
        ev->line          = p->error_line;
        return ev->type;
    }

    if (!p->started) {
        p->started = 1;
        ev->type   = SIML_EVENT_STREAM_START;
        ev->line   = 0;
        return ev->type;
    }

    if (p->mode == SIML_MODE_FLOW) {
        t = siml_next_flow(p, ev);
        if (t == SIML_EVENT_ERROR && p->error_code != SIML_ERR_NONE) {
            ev->error_code    = p->error_code;
            ev->error_message = p->error_message;
            if (ev->line == 0) ev->line = p->error_line;
        }
        return t;
    }

    if (p->mode == SIML_MODE_BLOCK) {
        t = siml_next_block(p, ev);
        if (t == SIML_EVENT_ERROR && p->error_code != SIML_ERR_NONE) {
            ev->error_code    = p->error_code;
            ev->error_message = p->error_message;
            if (ev->line == 0) ev->line = p->error_line;
        }
        return t;
    }

    t = siml_emit_pending_end(p, ev);
    if (t != SIML_EVENT_NONE) return t;

    t = siml_emit_pending_start(p, ev);
    if (t != SIML_EVENT_NONE) return t;

    t = siml_next_normal(p, ev);
    if (t == SIML_EVENT_ERROR && p->error_code != SIML_ERR_NONE) {
        ev->error_code    = p->error_code;
        ev->error_message = p->error_message;
        if (ev->line == 0) ev->line = p->error_line;
    }
    return t;
}

static int siml_parse_mapping_entry(siml_parser *p,
                                    const char *s,
                                    size_t len,
                                    size_t indent,
                                    size_t *out_key_len,
                                    size_t *out_value_start,
                                    size_t *out_value_len,
                                    unsigned int *out_spaces,
                                    const char **out_comment,
                                    size_t *out_comment_len,
                                    int *out_has_inline_value) {
    size_t i;

    if (indent >= len) {
        siml_set_error(p, SIML_ERR_KEY_ILLEGAL,
                       "illegal mapping key, must match: [a-zA-Z_][a-zA-Z0-9_.-]*");
        return 0;
    }
    if (!siml_is_alpha(s[indent]) && s[indent] != '_') {
        siml_set_error(p, SIML_ERR_KEY_ILLEGAL,
                       "illegal mapping key, must match: [a-zA-Z_][a-zA-Z0-9_.-]*");
        return 0;
    }
    i = indent + 1;
    while (i < len && siml_is_key_char(s[i])) {
        ++i;
    }
    if (i >= len || s[i] != ':') {
        siml_set_error(p, SIML_ERR_KEY_ILLEGAL,
                       "illegal mapping key, must match: [a-zA-Z_][a-zA-Z0-9_.-]*");
        return 0;
    }
    *out_key_len = i - indent;
    if (*out_key_len > SIML_MAX_KEY_LEN) {
        siml_set_error(p, SIML_ERR_KEY_TOO_LONG,
                       "mapping key too long (max 128 bytes)");
        return 0;
    }

    if (i + 1 == len) {
        *out_has_inline_value = 0;
        return 1;
    }
    if (s[i + 1] != ' ' || (i + 2 < len && s[i + 2] == ' ')) {
        siml_set_error(p, SIML_ERR_EXPECT_SPACE_AFTER_COLON,
                       "expected single space after ':'");
        return 0;
    }
    if (i + 2 >= len) {
        siml_set_error(p, SIML_ERR_INLINE_VALUE_EMPTY, "inline value is empty");
        return 0;
    }

    *out_value_start = i + 2;
    if (!siml_parse_inline_comment(p, s, *out_value_start, len,
                                   out_value_len, out_spaces,
                                   out_comment, out_comment_len)) {
        return 0;
    }
    *out_has_inline_value = 1;
    return 1;
}

static int siml_parse_sequence_item(siml_parser *p,
                                    const char *s,
                                    size_t len,
                                    size_t indent,
                                    size_t *out_value_start,
                                    size_t *out_value_len,
                                    unsigned int *out_spaces,
                                    const char **out_comment,
                                    size_t *out_comment_len,
                                    int *out_has_inline_value) {
    size_t dash = indent;

    if (dash >= len || s[dash] != '-') {
        siml_set_error(p, SIML_ERR_EXPECT_SPACE_AFTER_DASH,
                       "expected single space after '-'");
        return 0;
    }
    if (dash + 1 == len) {
        *out_has_inline_value = 0;
        return 1;
    }
    if (s[dash + 1] != ' ' || (dash + 2 < len && s[dash + 2] == ' ')) {
        siml_set_error(p, SIML_ERR_EXPECT_SPACE_AFTER_DASH,
                       "expected single space after '-'");
        return 0;
    }
    if (dash + 2 >= len) {
        siml_set_error(p, SIML_ERR_INLINE_VALUE_EMPTY, "inline value is empty");
        return 0;
    }

    *out_value_start = dash + 2;
    if (!siml_parse_inline_comment(p, s, *out_value_start, len,
                                   out_value_len, out_spaces,
                                   out_comment, out_comment_len)) {
        return 0;
    }
    *out_has_inline_value = 1;
    return 1;
}

static int siml_prepare_flow_sequence(siml_parser *p,
                                      size_t value_start,
                                      size_t value_len) {
    size_t i;
    size_t end_index;
    int depth = 0;
    int saw_close = 0;

    if (value_len < 2) {
        siml_set_error(p, SIML_ERR_FLOW_MULTI_LINE,
                       "multi-line flow sequences are forbidden");
        return 0;
    }
    end_index = value_start + value_len - 1;

    for (i = value_start; i <= end_index; ++i) {
        char c = p->line[i];
        if (c == ' ' || c == '\t') {
            siml_set_error(p, SIML_ERR_FLOW_WHITESPACE,
                           "flow sequence contains whitespace (forbidden)");
            return 0;
        }
        if (c == '[') {
            if (depth == 0 && i != value_start) {
                siml_set_error(p, SIML_ERR_FLOW_TRAILING_CHARS,
                               "trailing characters after flow sequence are forbidden");
                return 0;
            }
            depth += 1;
        } else if (c == ']') {
            saw_close = 1;
            depth -= 1;
            if (depth < 0) {
                siml_set_error(p, SIML_ERR_FLOW_UNTERMINATED,
                               "unterminated flow sequence");
                return 0;
            }
            if (depth == 0 && i != end_index) {
                siml_set_error(p, SIML_ERR_FLOW_TRAILING_CHARS,
                               "trailing characters after flow sequence are forbidden");
                return 0;
            }
        }
    }

    if (!saw_close || depth != 0) {
        siml_set_error(p, SIML_ERR_FLOW_UNTERMINATED_SAME_LINE,
                       "unterminated flow sequence on the same line");
        return 0;
    }

    p->flow_depth = 1;
    p->flow_stack_start[0] = value_start;
    p->flow_stack_end[0] = end_index;
    p->flow_stack_pos[0] = value_start + 1;
    p->flow_stack_started[0] = 0;
    return 1;
}

static siml_event_type siml_next_flow(siml_parser *p, siml_event *ev) {
    const char *s = p->line;

    for (;;) {
        int depth = p->flow_depth - 1;
        size_t end;
        size_t pos;

        if (depth < 0) {
            siml_set_error(p, SIML_ERR_FLOW_UNTERMINATED,
                           "unterminated flow sequence");
            return SIML_EVENT_ERROR;
        }

        end = p->flow_stack_end[depth];
        pos = p->flow_stack_pos[depth];

        if (!p->flow_stack_started[depth]) {
            p->flow_stack_started[depth] = 1;
            ev->type = SIML_EVENT_SEQUENCE_START;
            ev->seq_style = SIML_SEQ_STYLE_FLOW;
            if (depth == 0) {
                ev->key = siml_make_slice(p->flow_key, p->flow_key_len);
                ev->inline_comment_spaces = p->flow_inline_spaces;
                ev->inline_comment = siml_make_slice(p->flow_inline_comment,
                                                     p->flow_inline_comment_len);
            }
            ev->line = p->line_no;
            return ev->type;
        }

        if (pos >= end) {
            ev->type = SIML_EVENT_SEQUENCE_END;
            ev->line = p->line_no;
            if (depth == 0) {
                p->mode = SIML_MODE_NORMAL;
                p->have_line = 0;
                p->flow_depth = 0;
            } else {
                p->flow_depth -= 1;
            }
            return ev->type;
        }

        if (s[pos] == ',') {
            siml_set_error(p, SIML_ERR_FLOW_EMPTY_ELEM,
                           "empty flow sequence element");
            return SIML_EVENT_ERROR;
        }

        if (s[pos] == '[') {
            size_t i = pos;
            int nest = 0;
            size_t match = (size_t)(-1);
            while (i <= end) {
                if (s[i] == '[') nest += 1;
                else if (s[i] == ']') {
                    nest -= 1;
                    if (nest == 0) {
                        match = i;
                        break;
                    }
                }
                ++i;
            }
            if (match == (size_t)(-1) || match > end) {
                siml_set_error(p, SIML_ERR_FLOW_UNTERMINATED,
                               "unterminated flow sequence");
                return SIML_EVENT_ERROR;
            }

            p->flow_stack_pos[depth] = match + 1;
            if (p->flow_stack_pos[depth] < end) {
                if (s[p->flow_stack_pos[depth]] != ',') {
                    siml_set_error(p, SIML_ERR_FLOW_TRAILING_CHARS,
                                   "trailing characters after flow sequence are forbidden");
                    return SIML_EVENT_ERROR;
                }
                p->flow_stack_pos[depth] += 1;
                if (p->flow_stack_pos[depth] == end) {
                    siml_set_error(p, SIML_ERR_FLOW_TRAILING_COMMA,
                                   "trailing comma in flow sequence is forbidden");
                    return SIML_EVENT_ERROR;
                }
            }

            if (p->flow_depth >= SIML_MAX_NESTING) {
                siml_set_error_one(p, SIML_ERR_INDENT_WRONG,
                                   "wrong indentation, expected: ",
                                   0,
                                   "");
                return SIML_EVENT_ERROR;
            }
            p->flow_stack_start[p->flow_depth] = pos;
            p->flow_stack_end[p->flow_depth] = match;
            p->flow_stack_pos[p->flow_depth] = pos + 1;
            p->flow_stack_started[p->flow_depth] = 0;
            p->flow_depth += 1;
            continue;
        }

        {
            size_t i = pos;
            size_t item_len;
            while (i < end && s[i] != ',') {
                if (s[i] == '[' || s[i] == ']') break;
                ++i;
            }
            if (i <= pos) {
                siml_set_error(p, SIML_ERR_FLOW_EMPTY_ELEM,
                               "empty flow sequence element");
                return SIML_EVENT_ERROR;
            }
            if (i < end && (s[i] == '[' || s[i] == ']')) {
                siml_set_error(p, SIML_ERR_FLOW_TRAILING_CHARS,
                               "trailing characters after flow sequence are forbidden");
                return SIML_EVENT_ERROR;
            }
            item_len = i - pos;
            if (item_len > SIML_MAX_FLOW_ELEMENT_LEN) {
                siml_set_error(p, SIML_ERR_FLOW_ATOM_TOO_LONG,
                               "flow sequence atom too long (max 128 bytes)");
                return SIML_EVENT_ERROR;
            }
            if (s[pos] == '|') {
                siml_set_error(p, SIML_ERR_FLOW_EMPTY_ELEM,
                               "empty flow sequence element");
                return SIML_EVENT_ERROR;
            }

            ev->type = SIML_EVENT_SCALAR;
            ev->value = siml_make_slice(s + pos, item_len);
            ev->line = p->line_no;

            p->flow_stack_pos[depth] = pos + item_len;
            if (p->flow_stack_pos[depth] < end) {
                if (s[p->flow_stack_pos[depth]] != ',') {
                    siml_set_error(p, SIML_ERR_FLOW_TRAILING_CHARS,
                                   "trailing characters after flow sequence are forbidden");
                    return SIML_EVENT_ERROR;
                }
                p->flow_stack_pos[depth] += 1;
                if (p->flow_stack_pos[depth] == end) {
                    siml_set_error(p, SIML_ERR_FLOW_TRAILING_COMMA,
                                   "trailing comma in flow sequence is forbidden");
                    return SIML_EVENT_ERROR;
                }
            }
            return ev->type;
        }
    }
}

static siml_event_type siml_next_block(siml_parser *p, siml_event *ev) {
    int rc;

    for (;;) {
        if (p->block_emit_blanks && p->block_blank_count > 0) {
            ev->type = SIML_EVENT_BLOCK_SCALAR_LINE;
            ev->key = siml_make_slice(p->block_key, p->block_key_len);
            ev->value = siml_make_slice("", 0);
            ev->line = p->block_blank_start_line;
            p->block_blank_start_line += 1;
            p->block_blank_count -= 1;
            if (p->block_blank_count == 0) {
                p->block_emit_blanks = 0;
            }
            return ev->type;
        }

        if (!p->have_line) {
            rc = siml_fetch_line(p);
            if (rc < 0) return SIML_EVENT_ERROR;
            if (rc == 0) {
                if (!p->block_seen_content) {
                    siml_set_error(p, SIML_ERR_BLOCK_EMPTY,
                                   "block literal must not be empty");
                    return SIML_EVENT_ERROR;
                }
                if (p->block_blank_count > 0) {
                    siml_set_error(p, SIML_ERR_BLOCK_TRAILING_BLANK,
                                   "block literal has trailing blank line (forbidden)");
                    return SIML_EVENT_ERROR;
                }
                p->mode = SIML_MODE_NORMAL;
                ev->type = SIML_EVENT_BLOCK_SCALAR_END;
                ev->key = siml_make_slice(p->block_key, p->block_key_len);
                ev->line = p->block_start_line;
                return ev->type;
            }
        }

        if (!siml_check_line_common(p)) return SIML_EVENT_ERROR;

        if (p->line_len == 0) {
            if (!p->block_seen_content) {
                siml_set_error(p, SIML_ERR_BLOCK_LEADING_BLANK,
                               "block literal has leading blank line (forbidden)");
                return SIML_EVENT_ERROR;
            }
            if (p->block_blank_count == 0) {
                p->block_blank_start_line = p->line_no;
            }
            p->block_blank_count += 1;
            p->have_line = 0;
            continue;
        }

        if (siml_is_space_or_tab_only(p->line, p->line_len)) {
            siml_set_error(p, SIML_ERR_BLOCK_WHITESPACE_ONLY,
                           "whitespace-only lines are forbidden in block literal content");
            return SIML_EVENT_ERROR;
        }

        if (p->line[p->line_len - 1] == ' ') {
            siml_set_error(p, SIML_ERR_TRAILING_SPACES,
                           "trailing spaces are not allowed here");
            return SIML_EVENT_ERROR;
        }

        {
            size_t indent = 0;
            size_t i;
            const char *s = p->line;
            size_t len = p->line_len;

            for (i = 0; i < len && s[i] == ' '; ++i) {
                indent += 1;
            }

            if (indent < p->block_indent + 2) {
                if (!p->block_seen_content) {
                    siml_set_error(p, SIML_ERR_BLOCK_EMPTY,
                                   "block literal must not be empty");
                    return SIML_EVENT_ERROR;
                }
                if (p->block_blank_count > 0) {
                    siml_set_error(p, SIML_ERR_BLOCK_TRAILING_BLANK,
                                   "block literal has trailing blank line (forbidden)");
                    return SIML_EVENT_ERROR;
                }
                p->mode = SIML_MODE_NORMAL;
                ev->type = SIML_EVENT_BLOCK_SCALAR_END;
                ev->key = siml_make_slice(p->block_key, p->block_key_len);
                ev->line = p->block_start_line;
                return ev->type;
            }

            if (len - (p->block_indent + 2) > SIML_MAX_BLOCK_LINE_LEN) {
                siml_set_error(p, SIML_ERR_BLOCK_LINE_TOO_LONG,
                               "block literal content line too long (max 4096 bytes)");
                return SIML_EVENT_ERROR;
            }

            if (p->block_blank_count > 0) {
                p->block_emit_blanks = 1;
                continue;
            }

            ev->type = SIML_EVENT_BLOCK_SCALAR_LINE;
            ev->key = siml_make_slice(p->block_key, p->block_key_len);
            ev->value = siml_make_slice(s + p->block_indent + 2,
                                        len - (p->block_indent + 2));
            ev->line = p->line_no;
            p->block_seen_content = 1;
            p->have_line = 0;
            return ev->type;
        }
    }
}

static int siml_comment_indent_allowed(siml_parser *p, size_t indent) {
    int i;

    if (p->pending_kind != SIML_PENDING_NONE) {
        return indent == p->pending_indent;
    }

    if (p->depth == 0) {
        return indent == 0;
    }

    for (i = p->depth - 1; i >= 0; --i) {
        if (p->stack[i].indent == indent) return 1;
    }
    return 0;
}

static int siml_find_indent_target(siml_parser *p, size_t indent) {
    int i;
    for (i = p->depth - 1; i >= 0; --i) {
        if (p->stack[i].indent == indent) {
            return i + 1;
        }
    }
    return -1;
}

static siml_event_type siml_next_normal(siml_parser *p, siml_event *ev) {
    int rc;

    for (;;) {
        if (!p->have_line) {
            rc = siml_fetch_line(p);
            if (rc < 0) return SIML_EVENT_ERROR;
            if (rc == 0) {
                if (p->pending_kind == SIML_PENDING_MAP) {
                    siml_set_error(p, SIML_ERR_HEADER_MAP_NO_NESTED,
                                   "header-only mapping entry must have a nested node");
                    return SIML_EVENT_ERROR;
                }
                if (p->pending_kind == SIML_PENDING_SEQ) {
                    siml_set_error(p, SIML_ERR_HEADER_SEQ_NO_NESTED,
                                   "header-only sequence item must have a nested node");
                    return SIML_EVENT_ERROR;
                }
                if (p->awaiting_document) {
                    siml_set_error(p, SIML_ERR_SEPARATOR_AFTER_DOC,
                                   "document separator must not appear after the last document");
                    return SIML_EVENT_ERROR;
                }
                if (!p->seen_document) {
                    siml_set_error(p, SIML_ERR_DOC_SCALAR,
                                   "document root must not be a scalar");
                    return SIML_EVENT_ERROR;
                }
                if (p->in_document) {
                    p->pending_close = 1;
                    p->target_depth = 0;
                    p->pending_doc_end = 1;
                    p->pending_stream_end = 1;
                    return siml_emit_pending_end(p, ev);
                }
                p->pending_stream_end = 1;
                return siml_emit_pending_end(p, ev);
            }
        }

        if (!siml_check_line_nonblock(p)) return SIML_EVENT_ERROR;

        {
            size_t indent = 0;
            int comment_rc;
            size_t trimmed_len = p->line_len;
            int has_trailing_spaces = 0;
            while (trimmed_len > 0 && p->line[trimmed_len - 1] == ' ') {
                trimmed_len -= 1;
            }
            has_trailing_spaces = (trimmed_len != p->line_len);

            comment_rc = siml_parse_comment_line(p, p->line, trimmed_len, &indent);
            if (comment_rc < 0) return SIML_EVENT_ERROR;
            if (comment_rc > 0) {
                if (has_trailing_spaces) {
                    siml_set_error(p, SIML_ERR_TRAILING_SPACES,
                                   "trailing spaces are not allowed here");
                    return SIML_EVENT_ERROR;
                }
                if (!siml_comment_indent_allowed(p, indent)) {
                    siml_set_error(p, SIML_ERR_COMMENT_INDENT,
                                   "comment indentation must match current nesting level");
                    return SIML_EVENT_ERROR;
                }
                if (p->pending_kind == SIML_PENDING_NONE && p->depth > 0 &&
                    indent < p->stack[p->depth - 1].indent) {
                    int target = siml_find_indent_target(p, indent);
                    if (target < 0) {
                        siml_set_error_one(p, SIML_ERR_INDENT_WRONG,
                                           "wrong indentation, expected: ",
                                           (unsigned long)p->stack[p->depth - 1].indent,
                                           "");
                        return SIML_EVENT_ERROR;
                    }
                    p->pending_close = 1;
                    p->target_depth = target;
                    return siml_emit_pending_end(p, ev);
                }
                ev->type = SIML_EVENT_COMMENT;
                ev->value = siml_make_slice(p->line, trimmed_len);
                ev->line = p->line_no;
                p->have_line = 0;
                return ev->type;
            }
        }

        {
            const char *s = p->line;
            size_t trimmed_len = p->line_len;
            int has_trailing_spaces = 0;
            while (trimmed_len > 0 && s[trimmed_len - 1] == ' ') {
                trimmed_len -= 1;
            }
            has_trailing_spaces = (trimmed_len != p->line_len);
            {
            size_t len = trimmed_len;
            size_t indent = 0;
            size_t key_len = 0;
            size_t value_start = 0;
            size_t value_len = 0;
            unsigned int ic_spaces = 0;
            const char *ic_ptr = 0;
            size_t ic_len = 0;
            int has_inline_value = 0;
            int is_mapping = 0;
            int is_sequence = 0;
            siml_container *cur = 0;

            if (!siml_count_indent(p, s, len, &indent)) return SIML_EVENT_ERROR;

            if (indent == 0 && len >= 3 && s[0] == '-' && s[1] == '-' && s[2] == '-') {
                if (has_trailing_spaces && len == 3) {
                    siml_set_error(p, SIML_ERR_SEPARATOR_FORMAT,
                                   "document separator must be exactly ---");
                    return SIML_EVENT_ERROR;
                }
                if (len == 3) {
                    if (p->pending_kind == SIML_PENDING_MAP) {
                        siml_set_error(p, SIML_ERR_HEADER_MAP_NO_NESTED,
                                       "header-only mapping entry must have a nested node");
                        return SIML_EVENT_ERROR;
                    }
                    if (p->pending_kind == SIML_PENDING_SEQ) {
                        siml_set_error(p, SIML_ERR_HEADER_SEQ_NO_NESTED,
                                       "header-only sequence item must have a nested node");
                        return SIML_EVENT_ERROR;
                    }
                    if (!p->in_document) {
                        siml_set_error(p, SIML_ERR_SEPARATOR_BEFORE_DOC,
                                       "document separator must not appear before the first document");
                        return SIML_EVENT_ERROR;
                    }
                    p->awaiting_document = 1;
                    p->pending_close = 1;
                    p->target_depth = 0;
                    p->pending_doc_end = 1;
                    p->have_line = 0;
                    return siml_emit_pending_end(p, ev);
                }
                {
                    size_t i;
                    int has_inline_comment = 0;
                    for (i = 3; i + 2 < len; ++i) {
                        if (s[i] == '#' && s[i - 1] == ' ' && s[i + 1] == ' ') {
                            has_inline_comment = 1;
                            break;
                        }
                    }
                    if (has_inline_comment) {
                        siml_set_error(p, SIML_ERR_SEPARATOR_INLINE_COMMENT,
                                       "document separator must not have inline comments");
                        return SIML_EVENT_ERROR;
                    }
                }
                siml_set_error(p, SIML_ERR_SEPARATOR_FORMAT,
                               "document separator must be exactly ---");
                return SIML_EVENT_ERROR;
            }
            if (indent > 0 && len - indent == 3 &&
                s[indent] == '-' && s[indent + 1] == '-' && s[indent + 2] == '-') {
                siml_set_error(p, SIML_ERR_SEPARATOR_INDENT,
                               "document separator must be at indent 0");
                return SIML_EVENT_ERROR;
            }

            if (indent >= len) {
                siml_set_error(p, SIML_ERR_DOC_SCALAR,
                               "document root must not be a scalar");
                return SIML_EVENT_ERROR;
            }

            if (s[indent] == '-') {
                is_sequence = 1;
            } else {
                is_mapping = 1;
            }

            if (p->pending_kind != SIML_PENDING_NONE) {
                if (indent != p->pending_indent) {
                    siml_set_error_two(p, SIML_ERR_INDENT_NEST_MISMATCH,
                                       "nested node indentation mismatch, expected ",
                                       (unsigned long)p->pending_indent,
                                       " got ",
                                       (unsigned long)indent);
                    return SIML_EVENT_ERROR;
                }
                if (!p->in_document) {
                    siml_set_error(p, SIML_ERR_DOC_SCALAR,
                                   "document root must not be a scalar");
                    return SIML_EVENT_ERROR;
                }
                if (is_mapping) {
                    if (!siml_request_container_start(p, SIML_CONTAINER_MAP,
                                                      SIML_SEQ_STYLE_BLOCK, indent,
                                                      p->pending_key,
                                                      p->pending_key_len)) {
                        return SIML_EVENT_ERROR;
                    }
                } else {
                    if (!siml_request_container_start(p, SIML_CONTAINER_SEQ,
                                                      SIML_SEQ_STYLE_BLOCK, indent,
                                                      p->pending_key,
                                                      p->pending_key_len)) {
                        return SIML_EVENT_ERROR;
                    }
                }
                p->pending_kind = SIML_PENDING_NONE;
                p->pending_key[0] = '\0';
                p->pending_key_len = 0;
                return siml_emit_pending_start(p, ev);
            }

            if (!p->in_document) {
                if (indent != 0) {
                    siml_set_error(p, SIML_ERR_DOC_INDENT,
                                   "document must start at indent 0");
                    return SIML_EVENT_ERROR;
                }
                p->seen_document = 1;
                p->in_document = 1;
                p->awaiting_document = 0;
                p->pending_doc_start = 1;
                if (is_mapping) {
                    if (!siml_request_container_start(p, SIML_CONTAINER_MAP,
                                                      SIML_SEQ_STYLE_BLOCK, indent,
                                                      "", 0)) {
                        return SIML_EVENT_ERROR;
                    }
                } else {
                    if (!siml_request_container_start(p, SIML_CONTAINER_SEQ,
                                                      SIML_SEQ_STYLE_BLOCK, indent,
                                                      "", 0)) {
                        return SIML_EVENT_ERROR;
                    }
                }
                return siml_emit_pending_start(p, ev);
            }

            if (p->depth <= 0) {
                siml_set_error(p, SIML_ERR_DOC_SCALAR,
                               "document root must not be a scalar");
                return SIML_EVENT_ERROR;
            }
            cur = &p->stack[p->depth - 1];

            if (indent > cur->indent) {
                siml_set_error_one(p, SIML_ERR_INDENT_WRONG,
                                   "wrong indentation, expected: ",
                                   (unsigned long)cur->indent,
                                   "");
                return SIML_EVENT_ERROR;
            }

            if (indent < cur->indent) {
                int target = siml_find_indent_target(p, indent);
                if (target < 0) {
                    siml_set_error_one(p, SIML_ERR_INDENT_WRONG,
                                       "wrong indentation, expected: ",
                                       (unsigned long)cur->indent,
                                       "");
                    return SIML_EVENT_ERROR;
                }
                p->pending_close = 1;
                p->target_depth = target;
                return siml_emit_pending_end(p, ev);
            }

            if ((cur->type == SIML_CONTAINER_MAP && !is_mapping) ||
                (cur->type == SIML_CONTAINER_SEQ && !is_sequence)) {
                siml_set_error_one(p, SIML_ERR_NODE_KIND_MIX,
                                   "node kind mixing at indent ",
                                   (unsigned long)indent,
                                   " is forbidden");
                return SIML_EVENT_ERROR;
            }

            if (is_mapping) {
                if (!siml_parse_mapping_entry(p, s, len, indent,
                                              &key_len, &value_start, &value_len,
                                              &ic_spaces, &ic_ptr, &ic_len,
                                              &has_inline_value)) {
                    return SIML_EVENT_ERROR;
                }
                if (has_trailing_spaces &&
                    (!has_inline_value || s[value_start] != '[')) {
                    siml_set_error(p, SIML_ERR_TRAILING_SPACES,
                                   "trailing spaces are not allowed here");
                    return SIML_EVENT_ERROR;
                }
                cur->item_count += 1;

                if (!has_inline_value) {
                    p->pending_kind = SIML_PENDING_MAP;
                    p->pending_indent = indent + 2;
                    memcpy(p->pending_key, s + indent, key_len);
                    p->pending_key[key_len] = '\0';
                    p->pending_key_len = key_len;
                    p->have_line = 0;
                    continue;
                }

                if (value_len > SIML_MAX_INLINE_VALUE_LEN) {
                    siml_set_error(p, SIML_ERR_INLINE_VALUE_TOO_LONG,
                                   "inline value too long (max 2048 bytes)");
                    return SIML_EVENT_ERROR;
                }
                if (value_len == 0 && ic_ptr) {
                    siml_set_error(p, SIML_ERR_HEADER_MAP_INLINE_COMMENT,
                                   "header-only mapping entry must not have inline comments");
                    return SIML_EVENT_ERROR;
                }
                if (value_len == 0) {
                    siml_set_error(p, SIML_ERR_INLINE_VALUE_EMPTY, "inline value is empty");
                    return SIML_EVENT_ERROR;
                }

                if (s[value_start] == '|') {
                    if (value_len != 1) {
                        siml_set_error(p, SIML_ERR_INLINE_VALUE_EMPTY,
                                       "inline value is empty");
                        return SIML_EVENT_ERROR;
                    }
                    p->mode = SIML_MODE_BLOCK;
                    p->block_indent = indent;
                    memcpy(p->block_key, s + indent, key_len);
                    p->block_key[key_len] = '\0';
                    p->block_key_len = key_len;
                    p->block_inline_spaces = ic_spaces;
                    p->block_inline_comment = ic_ptr;
                    p->block_inline_comment_len = ic_len;
                    p->block_start_line = p->line_no;
                    p->block_seen_content = 0;
                    p->block_blank_count = 0;
                    p->block_blank_start_line = 0;
                    p->block_emit_blanks = 0;
                    p->have_line = 0;

                    ev->type = SIML_EVENT_BLOCK_SCALAR_START;
                    ev->key = siml_make_slice(p->block_key, p->block_key_len);
                    ev->inline_comment_spaces = p->block_inline_spaces;
                    ev->inline_comment = siml_make_slice(p->block_inline_comment,
                                                         p->block_inline_comment_len);
                    ev->line = p->line_no;
                    return ev->type;
                }

                if (s[value_start] == '[') {
                    if (!siml_prepare_flow_sequence(p, value_start, value_len)) {
                        return SIML_EVENT_ERROR;
                    }
                    p->mode = SIML_MODE_FLOW;
                    memcpy(p->flow_key, s + indent, key_len);
                    p->flow_key[key_len] = '\0';
                    p->flow_key_len = key_len;
                    p->flow_inline_spaces = ic_spaces;
                    p->flow_inline_comment = ic_ptr;
                    p->flow_inline_comment_len = ic_len;
                    return siml_next_flow(p, ev);
                }

                if (s[value_start] == '[' || s[value_start] == '|') {
                    siml_set_error(p, SIML_ERR_INLINE_VALUE_EMPTY,
                                   "inline value is empty");
                    return SIML_EVENT_ERROR;
                }

                ev->type = SIML_EVENT_SCALAR;
                ev->key = siml_make_slice(s + indent, key_len);
                ev->value = siml_make_slice(s + value_start, value_len);
                ev->inline_comment_spaces = ic_spaces;
                ev->inline_comment = siml_make_slice(ic_ptr, ic_len);
                ev->line = p->line_no;
                p->have_line = 0;
                return ev->type;
            }

            if (!siml_parse_sequence_item(p, s, len, indent,
                                          &value_start, &value_len,
                                          &ic_spaces, &ic_ptr, &ic_len,
                                          &has_inline_value)) {
                return SIML_EVENT_ERROR;
            }
            if (has_trailing_spaces &&
                (!has_inline_value || s[value_start] != '[')) {
                siml_set_error(p, SIML_ERR_TRAILING_SPACES,
                               "trailing spaces are not allowed here");
                return SIML_EVENT_ERROR;
            }
            cur->item_count += 1;

            if (!has_inline_value) {
                p->pending_kind = SIML_PENDING_SEQ;
                p->pending_indent = indent + 2;
                p->pending_key[0] = '\0';
                p->pending_key_len = 0;
                p->have_line = 0;
                continue;
            }

            if (value_len > SIML_MAX_INLINE_VALUE_LEN) {
                siml_set_error(p, SIML_ERR_INLINE_VALUE_TOO_LONG,
                               "inline value too long (max 2048 bytes)");
                return SIML_EVENT_ERROR;
            }
            if (value_len == 0 && ic_ptr) {
                siml_set_error(p, SIML_ERR_HEADER_SEQ_INLINE_COMMENT,
                               "header-only sequence item must not have inline comments");
                return SIML_EVENT_ERROR;
            }
            if (value_len == 0) {
                siml_set_error(p, SIML_ERR_INLINE_VALUE_EMPTY, "inline value is empty");
                return SIML_EVENT_ERROR;
            }

            if (s[value_start] == '|') {
                if (value_len != 1) {
                    siml_set_error(p, SIML_ERR_INLINE_VALUE_EMPTY,
                                   "inline value is empty");
                    return SIML_EVENT_ERROR;
                }
                p->mode = SIML_MODE_BLOCK;
                p->block_indent = indent;
                p->block_key[0] = '\0';
                p->block_key_len = 0;
                p->block_inline_spaces = ic_spaces;
                p->block_inline_comment = ic_ptr;
                p->block_inline_comment_len = ic_len;
                p->block_start_line = p->line_no;
                p->block_seen_content = 0;
                p->block_blank_count = 0;
                p->block_blank_start_line = 0;
                p->block_emit_blanks = 0;
                p->have_line = 0;

                ev->type = SIML_EVENT_BLOCK_SCALAR_START;
                ev->inline_comment_spaces = p->block_inline_spaces;
                ev->inline_comment = siml_make_slice(p->block_inline_comment,
                                                     p->block_inline_comment_len);
                ev->line = p->line_no;
                return ev->type;
            }

            if (s[value_start] == '[') {
                if (!siml_prepare_flow_sequence(p, value_start, value_len)) {
                    return SIML_EVENT_ERROR;
                }
                p->mode = SIML_MODE_FLOW;
                p->flow_key[0] = '\0';
                p->flow_key_len = 0;
                p->flow_inline_spaces = ic_spaces;
                p->flow_inline_comment = ic_ptr;
                p->flow_inline_comment_len = ic_len;
                return siml_next_flow(p, ev);
            }

            if (s[value_start] == '[' || s[value_start] == '|') {
                siml_set_error(p, SIML_ERR_INLINE_VALUE_EMPTY,
                               "inline value is empty");
                return SIML_EVENT_ERROR;
            }

            ev->type = SIML_EVENT_SCALAR;
            ev->value = siml_make_slice(s + value_start, value_len);
            ev->inline_comment_spaces = ic_spaces;
            ev->inline_comment = siml_make_slice(ic_ptr, ic_len);
            ev->line = p->line_no;
            p->have_line = 0;
            return ev->type;
            }
        }
    }
}

#endif /* SIML_IMPLEMENTATION */

#endif /* SIML_H_INCLUDED */
