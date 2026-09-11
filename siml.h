#ifndef SIML_H_INCLUDED
#define SIML_H_INCLUDED

/*
 * SIML reference parser v0.1
 *
 * Header-only, pure ANSI C89 implementation of the SIML specification.
 *
 * Features:
 *  - No dynamic allocation.
 *  - No I/O. The caller provides a line-reading callback.
 *  - Pull parser API: the caller repeatedly calls siml_next() to obtain events.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h> /* size_t */

#ifndef SIML_MAX_KEY_LEN
#define SIML_MAX_KEY_LEN 128
#endif

#ifndef SIML_MAX_MAPPING_KEYS
#define SIML_MAX_MAPPING_KEYS 512
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
    SIML_ERR_BAD_ENCODING,
    SIML_ERR_BAD_LINE_ENDING,
    SIML_ERR_LINE_TOO_LONG,
    SIML_ERR_BAD_WHITESPACE,
    SIML_ERR_BAD_INDENT,
    SIML_ERR_BAD_SEPARATOR,
    SIML_ERR_BAD_COMMENT,
    SIML_ERR_BAD_INLINE_COMMENT,
    SIML_ERR_BAD_KEY,
    SIML_ERR_DUPLICATE_KEY,
    SIML_ERR_TOO_MANY_KEYS,
    SIML_ERR_BAD_FIELD_SYNTAX,
    SIML_ERR_BAD_SCALAR,
    SIML_ERR_BAD_FLOW_SEQUENCE,
    SIML_ERR_BAD_BLOCK_SCALAR,
    SIML_ERR_BAD_CONTAINER,
    SIML_ERR_TOO_DEEP,
    SIML_ERR_UNEXPECTED_EOF,
    SIML_ERR_INTERNAL
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
 *   <0 : error (SIML_ERR_IO)
 *
 * The returned line must NOT include the trailing newline characters.
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
    size_t              key_start;
    size_t              key_count;
} siml_container;

typedef struct siml_key_entry_s {
    char   key[SIML_MAX_KEY_LEN + 1];
    size_t len;
} siml_key_entry;

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

    /* Mapping key storage */
    siml_key_entry    key_pool[SIML_MAX_MAPPING_KEYS];
    size_t            key_pool_len;

    /* Pending header-only value */
    int               pending_node;
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

    /* Error state */
    siml_error_code   error_code;
    const char       *error_message;
    char              error_buf[128];
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

#include <string.h> /* memcmp */

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

static int siml_is_whitespace_only(const char *s, size_t len) {
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

static void siml_set_error(siml_parser *p, siml_error_code code,
                           const char *msg) {
    if (p->error_code == SIML_ERR_NONE) {
        size_t i = 0;
        p->error_code    = code;
        p->error_line    = p->line_no;
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

static void siml_set_error_with_key(siml_parser *p, siml_error_code code,
                                    const char *prefix,
                                    const char *key, size_t key_len) {
    char buf[128];
    size_t i = 0;
    size_t k = 0;
    if (!prefix) prefix = "";
    while (prefix[i] != '\0' && i + 1 < sizeof(buf)) {
        buf[i] = prefix[i];
        ++i;
    }
    if (key && key_len > 0) {
        while (k < key_len && i + 2 < sizeof(buf)) {
            buf[i++] = key[k++];
        }
        if (i + 1 < sizeof(buf)) {
            buf[i++] = '\'';
        }
    }
    buf[i] = '\0';
    siml_set_error(p, code, buf);
}

static void siml_clear_event(siml_event *ev) {
    ev->type                 = SIML_EVENT_NONE;
    ev->key.ptr              = 0;
    ev->key.len              = 0;
    ev->value.ptr            = 0;
    ev->value.len            = 0;
    ev->inline_comment.ptr   = 0;
    ev->inline_comment.len   = 0;
    ev->inline_comment_spaces = 0;
    ev->seq_style            = SIML_SEQ_STYLE_BLOCK;
    ev->line                 = 0;
    ev->error_code           = SIML_ERR_NONE;
    ev->error_message        = 0;
}

static siml_slice siml_make_slice(const char *p, size_t len) {
    siml_slice s;
    s.ptr = p;
    s.len = len;
    return s;
}

/* Fetch next physical line into parser->line/line_len. Returns:
 *   1 on success, 0 on EOF, -1 on IO error.
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
    /* rc < 0: IO error */
    p->have_line = 0;
    siml_set_error(p, SIML_ERR_IO, "read_line IO error");
    return -1;
}

static int siml_check_line_common(siml_parser *p) {
    size_t i;
    const char *s = p->line;
    size_t len = p->line_len;

    if (len > SIML_MAX_LINE_LEN) {
        siml_set_error(p, SIML_ERR_LINE_TOO_LONG, "line too long");
        return 0;
    }
    if (p->line_no == 1 && len >= 3) {
        if ((unsigned char)s[0] == 0xEF &&
            (unsigned char)s[1] == 0xBB &&
            (unsigned char)s[2] == 0xBF) {
            siml_set_error(p, SIML_ERR_BAD_ENCODING, "UTF-8 BOM is not allowed");
            return 0;
        }
    }
    for (i = 0; i < len; ++i) {
        if (s[i] == '\r') {
            siml_set_error(p, SIML_ERR_BAD_LINE_ENDING, "CR is not allowed");
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
        siml_set_error(p, SIML_ERR_BAD_WHITESPACE, "blank lines are not allowed");
        return 0;
    }
    if (siml_is_whitespace_only(s, len)) {
        siml_set_error(p, SIML_ERR_BAD_WHITESPACE, "whitespace-only lines are not allowed");
        return 0;
    }
    if (s[len - 1] == ' ') {
        siml_set_error(p, SIML_ERR_BAD_WHITESPACE, "trailing spaces are not allowed");
        return 0;
    }
    for (i = 0; i < len; ++i) {
        if (s[i] == '\t') {
            siml_set_error(p, SIML_ERR_BAD_WHITESPACE, "tabs are not allowed");
            return 0;
        }
    }
    return 1;
}

static int siml_count_indent(siml_parser *p, const char *s, size_t len, size_t *out_indent) {
    size_t i = 0;
    while (i < len && s[i] == ' ') {
        ++i;
    }
    if ((i % 2) != 0) {
        siml_set_error(p, SIML_ERR_BAD_INDENT, "indentation must be a multiple of 2 spaces");
        return 0;
    }
    *out_indent = i;
    return 1;
}

static int siml_parse_comment_line(siml_parser *p, const char *s, size_t len) {
    size_t indent = 0;
    size_t text_len = 0;

    if (!siml_count_indent(p, s, len, &indent)) return -1;
    if (indent >= len) return 0;
    if (s[indent] != '#') return 0;
    if (indent + 1 >= len) {
        siml_set_error(p, SIML_ERR_BAD_COMMENT, "empty comment is not allowed");
        return -1;
    }
    if (s[indent + 1] != ' ') {
        siml_set_error(p, SIML_ERR_BAD_COMMENT, "comment must start with '# '");
        return -1;
    }
    if (indent + 2 >= len) {
        siml_set_error(p, SIML_ERR_BAD_COMMENT, "empty comment text is not allowed");
        return -1;
    }
    text_len = len - (indent + 2);
    if (text_len > SIML_MAX_COMMENT_TEXT_LEN) {
        siml_set_error(p, SIML_ERR_BAD_COMMENT, "comment too long");
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

    if (hash_pos + 1 >= len || s[hash_pos + 1] != ' ') {
        siml_set_error(p, SIML_ERR_BAD_INLINE_COMMENT, "inline comment must start with '# '");
        return 0;
    }
    if (hash_pos + 2 >= len) {
        siml_set_error(p, SIML_ERR_BAD_INLINE_COMMENT, "inline comment text is empty");
        return 0;
    }

    {
        size_t j = hash_pos;
        size_t value_end;
        while (j > 0 && s[j - 1] == ' ') {
            --j;
        }
        if (hash_pos <= j) {
            siml_set_error(p, SIML_ERR_BAD_INLINE_COMMENT, "inline comment needs space before '#'");
            return 0;
        }
        if (hash_pos - j > SIML_MAX_INLINE_COMMENT_SPACES) {
            siml_set_error(p, SIML_ERR_BAD_INLINE_COMMENT, "too many spaces before inline comment");
            return 0;
        }
        *out_spaces = (unsigned int)(hash_pos - j);
        value_end = hash_pos - *out_spaces;
        if (value_end < start) value_end = start;
        *out_value_len = value_end - start;
        *out_comment = s + hash_pos + 2;
        *out_comment_len = len - (hash_pos + 2);
        if (*out_comment_len > SIML_MAX_INLINE_COMMENT_TEXT_LEN) {
            siml_set_error(p, SIML_ERR_BAD_INLINE_COMMENT, "inline comment too long");
            return 0;
        }
    }

    return 1;
}

static int siml_is_doc_separator(const char *s, size_t len) {
    return (len == 3 && s[0] == '-' && s[1] == '-' && s[2] == '-');
}

static int siml_push_container(siml_parser *p, siml_container_type type, size_t indent) {
    siml_container *c;

    if (p->depth >= SIML_MAX_NESTING) {
        siml_set_error(p, SIML_ERR_TOO_DEEP, "nesting too deep");
        return 0;
    }
    c = &p->stack[p->depth++];
    c->type = type;
    c->indent = indent;
    c->item_count = 0;
    c->key_start = p->key_pool_len;
    c->key_count = 0;
    return 1;
}

static int siml_add_mapping_key(siml_parser *p, const char *key, size_t key_len) {
    siml_container *c;
    size_t i;

    if (p->depth <= 0) {
        siml_set_error(p, SIML_ERR_INTERNAL, "no active mapping for key");
        return 0;
    }
    c = &p->stack[p->depth - 1];
    if (c->type != SIML_CONTAINER_MAP) {
        siml_set_error(p, SIML_ERR_BAD_CONTAINER, "expected mapping container");
        return 0;
    }

    for (i = c->key_start; i < c->key_start + c->key_count; ++i) {
        if (p->key_pool[i].len == key_len &&
            memcmp(p->key_pool[i].key, key, key_len) == 0) {
            siml_set_error_with_key(p, SIML_ERR_DUPLICATE_KEY,
                                    "duplicate key '", key, key_len);
            return 0;
        }
    }

    if (p->key_pool_len >= SIML_MAX_MAPPING_KEYS) {
        siml_set_error(p, SIML_ERR_TOO_MANY_KEYS, "too many mapping keys");
        return 0;
    }

    p->key_pool[p->key_pool_len].len = key_len;
    memcpy(p->key_pool[p->key_pool_len].key, key, key_len);
    p->key_pool[p->key_pool_len].key[key_len] = '\0';
    p->key_pool_len += 1;
    c->key_count += 1;
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
            if (c.item_count == 0) {
                siml_set_error(p, SIML_ERR_BAD_CONTAINER, "empty container is not allowed");
                return SIML_EVENT_ERROR;
            }
            p->depth -= 1;
            p->key_pool_len = c.key_start;
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
    p->key_pool_len = 0;
    p->pending_node = 0;
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
        siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX, "expected mapping entry");
        return 0;
    }
    if (!siml_is_alpha(s[indent]) && s[indent] != '_') {
        siml_set_error(p, SIML_ERR_BAD_KEY, "invalid mapping key");
        return 0;
    }
    i = indent + 1;
    while (i < len && siml_is_key_char(s[i])) {
        ++i;
    }
    if (i >= len || s[i] != ':') {
        siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX, "expected ':' after key");
        return 0;
    }
    *out_key_len = i - indent;
    if (*out_key_len > SIML_MAX_KEY_LEN) {
        siml_set_error(p, SIML_ERR_BAD_KEY, "key too long");
        return 0;
    }

    if (i + 1 == len) {
        *out_has_inline_value = 0;
        return 1;
    }
    if (s[i + 1] != ' ') {
        siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX, "expected single space after ':'");
        return 0;
    }
    if (i + 2 >= len) {
        siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX, "inline value must be non-empty");
        return 0;
    }
    if (s[i + 2] == ' ') {
        siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX, "inline value must not start with space");
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
        siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX, "expected sequence item");
        return 0;
    }
    if (dash + 1 == len) {
        *out_has_inline_value = 0;
        return 1;
    }
    if (s[dash + 1] != ' ') {
        siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX, "expected single space after '-'");
        return 0;
    }
    if (dash + 2 >= len) {
        siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX, "inline value must be non-empty");
        return 0;
    }
    if (s[dash + 2] == ' ') {
        siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX, "inline value must not start with space");
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

    if (value_len < 2) {
        siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "flow sequence too short");
        return 0;
    }
    end_index = value_start + value_len - 1;
    if (p->line[end_index] != ']') {
        siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "flow sequence must end with ']'");
        return 0;
    }
    for (i = value_start; i <= end_index; ++i) {
        char c = p->line[i];
        if (c == ' ' || c == '\t') {
            siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "whitespace inside flow sequence is not allowed");
            return 0;
        }
        if (c == '[') {
            depth += 1;
        } else if (c == ']') {
            depth -= 1;
            if (depth < 0) {
                siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "unbalanced ']' in flow sequence");
                return 0;
            }
        }
    }
    if (depth != 0) {
        siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "unbalanced '[' in flow sequence");
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
            siml_set_error(p, SIML_ERR_INTERNAL, "invalid flow sequence depth");
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
                siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "unbalanced '[' in flow sequence element");
                return SIML_EVENT_ERROR;
            }

            p->flow_stack_pos[depth] = match + 1;
            if (p->flow_stack_pos[depth] < end) {
                if (s[p->flow_stack_pos[depth]] != ',') {
                    siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "expected ',' after nested flow sequence");
                    return SIML_EVENT_ERROR;
                }
                p->flow_stack_pos[depth] += 1;
                if (p->flow_stack_pos[depth] == end) {
                    siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "trailing comma in flow sequence");
                    return SIML_EVENT_ERROR;
                }
            }

            if (p->flow_depth >= SIML_MAX_NESTING) {
                siml_set_error(p, SIML_ERR_TOO_DEEP, "nesting too deep");
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
                siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "empty flow sequence element");
                return SIML_EVENT_ERROR;
            }
            if (s[i] == '[') {
                siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "unexpected '[' inside flow element");
                return SIML_EVENT_ERROR;
            }
            if (s[i] == ']') {
                i = end;
            }
            item_len = i - pos;
            if (item_len == 0) {
                siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "empty flow sequence element");
                return SIML_EVENT_ERROR;
            }
            if (item_len > SIML_MAX_FLOW_ELEMENT_LEN) {
                siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "flow sequence element too long");
                return SIML_EVENT_ERROR;
            }
            if (s[pos] == '|') {
                siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "flow sequence element has invalid start");
                return SIML_EVENT_ERROR;
            }

            ev->type = SIML_EVENT_SCALAR;
            ev->value = siml_make_slice(s + pos, item_len);
            ev->line = p->line_no;

            p->flow_stack_pos[depth] = pos + item_len;
            if (p->flow_stack_pos[depth] < end) {
                if (s[p->flow_stack_pos[depth]] != ',') {
                    siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "expected ',' between flow elements");
                    return SIML_EVENT_ERROR;
                }
                p->flow_stack_pos[depth] += 1;
                if (p->flow_stack_pos[depth] == end) {
                    siml_set_error(p, SIML_ERR_BAD_FLOW_SEQUENCE, "trailing comma in flow sequence");
                    return SIML_EVENT_ERROR;
                }
            }
            return ev->type;
        }
    }
}

static siml_event_type siml_next_block(siml_parser *p, siml_event *ev) {
    int rc;

    if (!p->have_line) {
        rc = siml_fetch_line(p);
        if (rc < 0) return SIML_EVENT_ERROR;
        if (rc == 0) {
            p->mode = SIML_MODE_NORMAL;
            ev->type = SIML_EVENT_BLOCK_SCALAR_END;
            ev->key = siml_make_slice(p->block_key, p->block_key_len);
            ev->line = p->block_start_line;
            return ev->type;
        }
    }

    if (!siml_check_line_common(p)) return SIML_EVENT_ERROR;

    if (p->line_len == 0) {
        long blank_line_no;
        int peek_rc;

        if (!p->block_seen_content) {
            siml_set_error(p, SIML_ERR_BAD_WHITESPACE, "blank lines are not allowed");
            return SIML_EVENT_ERROR;
        }

        blank_line_no = p->line_no;
        peek_rc = siml_fetch_line(p);
        if (peek_rc < 0) return SIML_EVENT_ERROR;
        if (peek_rc == 0) {
            siml_set_error(p, SIML_ERR_BAD_WHITESPACE, "blank lines are not allowed");
            return SIML_EVENT_ERROR;
        }
        if (!siml_check_line_common(p)) return SIML_EVENT_ERROR;
        if (p->line_len == 0 || siml_is_space_or_tab_only(p->line, p->line_len)) {
            siml_set_error(p, SIML_ERR_BAD_WHITESPACE, "blank lines are not allowed");
            return SIML_EVENT_ERROR;
        }
        {
            size_t peek_indent = 0;
            size_t j;
            for (j = 0; j < p->line_len && p->line[j] == ' '; ++j) {
                peek_indent += 1;
            }
            if (peek_indent < p->block_indent + 2) {
                siml_set_error(p, SIML_ERR_BAD_WHITESPACE, "blank lines are not allowed");
                return SIML_EVENT_ERROR;
            }
        }

        ev->type = SIML_EVENT_BLOCK_SCALAR_LINE;
        ev->key = siml_make_slice(p->block_key, p->block_key_len);
        ev->value = siml_make_slice("", 0);
        ev->line = blank_line_no;
        return ev->type;
    }
    if (siml_is_space_or_tab_only(p->line, p->line_len)) {
        siml_set_error(p, SIML_ERR_BAD_WHITESPACE, "whitespace-only lines are not allowed");
        return SIML_EVENT_ERROR;
    }

    {
        size_t i;
        size_t indent = 0;
        const char *s = p->line;
        size_t len = p->line_len;

        for (i = 0; i < len && s[i] == ' '; ++i) {
            indent += 1;
        }

        if (indent < p->block_indent + 2) {
            p->mode = SIML_MODE_NORMAL;
            ev->type = SIML_EVENT_BLOCK_SCALAR_END;
            ev->key = siml_make_slice(p->block_key, p->block_key_len);
            ev->line = p->block_start_line;
            return ev->type;
        }

        if (len < p->block_indent + 2) {
            siml_set_error(p, SIML_ERR_BAD_BLOCK_SCALAR, "block line too short");
            return SIML_EVENT_ERROR;
        }
        for (i = 0; i < p->block_indent + 2; ++i) {
            if (s[i] != ' ') {
                siml_set_error(p, SIML_ERR_BAD_BLOCK_SCALAR, "block indentation must be spaces");
                return SIML_EVENT_ERROR;
            }
        }

        if (len - (p->block_indent + 2) > SIML_MAX_BLOCK_LINE_LEN) {
            siml_set_error(p, SIML_ERR_BAD_BLOCK_SCALAR, "block line too long");
            return SIML_EVENT_ERROR;
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

static siml_event_type siml_next_normal(siml_parser *p, siml_event *ev) {
    int rc;

    for (;;) {
        if (!p->have_line) {
            rc = siml_fetch_line(p);
            if (rc < 0) return SIML_EVENT_ERROR;
            if (rc == 0) {
                if (p->pending_node) {
                    siml_set_error(p, SIML_ERR_UNEXPECTED_EOF, "unexpected EOF after header-only entry");
                    return SIML_EVENT_ERROR;
                }
                if (p->awaiting_document) {
                    siml_set_error(p, SIML_ERR_UNEXPECTED_EOF, "unexpected EOF after document separator");
                    return SIML_EVENT_ERROR;
                }
                if (!p->seen_document) {
                    siml_set_error(p, SIML_ERR_UNEXPECTED_EOF, "no documents in stream");
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

        rc = siml_parse_comment_line(p, p->line, p->line_len);
        if (rc < 0) return SIML_EVENT_ERROR;
        if (rc > 0) {
            ev->type = SIML_EVENT_COMMENT;
            ev->value = siml_make_slice(p->line, p->line_len);
            ev->line = p->line_no;
            p->have_line = 0;
            return ev->type;
        }

        if (siml_is_doc_separator(p->line, p->line_len)) {
            if (p->pending_node) {
                siml_set_error(p, SIML_ERR_BAD_SEPARATOR, "separator after header-only entry");
                return SIML_EVENT_ERROR;
            }
            if (!p->in_document) {
                siml_set_error(p, SIML_ERR_BAD_SEPARATOR, "separator before first document");
                return SIML_EVENT_ERROR;
            }
            p->awaiting_document = 1;
            p->pending_close = 1;
            p->target_depth = 0;
            p->pending_doc_end = 1;
            p->have_line = 0;
            return siml_emit_pending_end(p, ev);
        }

        /* Structural line */
        {
            const char *s = p->line;
            size_t len = p->line_len;
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

            if (indent >= len) {
                siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX, "expected structural line");
                return SIML_EVENT_ERROR;
            }

            if (s[indent] == '-') {
                is_sequence = 1;
            } else {
                is_mapping = 1;
            }

            if (p->pending_node) {
                if (indent != p->pending_indent) {
                    siml_set_error(p, SIML_ERR_BAD_INDENT, "nested node indentation mismatch");
                    return SIML_EVENT_ERROR;
                }
                if (!p->in_document) {
                    siml_set_error(p, SIML_ERR_INTERNAL, "pending node outside document");
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
                p->pending_node = 0;
                p->pending_key[0] = '\0';
                p->pending_key_len = 0;
                return siml_emit_pending_start(p, ev);
            }

            if (!p->in_document) {
                if (indent != 0) {
                    siml_set_error(p, SIML_ERR_BAD_INDENT, "document must start at indent 0");
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
                siml_set_error(p, SIML_ERR_INTERNAL, "missing container");
                return SIML_EVENT_ERROR;
            }
            cur = &p->stack[p->depth - 1];

            if (indent > cur->indent) {
                siml_set_error(p, SIML_ERR_BAD_INDENT, "unexpected indentation increase");
                return SIML_EVENT_ERROR;
            }

            if (indent < cur->indent) {
                int target = -1;
                int i;
                for (i = p->depth - 1; i >= 0; --i) {
                    if (p->stack[i].indent == indent) {
                        target = i + 1;
                        break;
                    }
                }
                if (target < 0) {
                    siml_set_error(p, SIML_ERR_BAD_INDENT, "indentation does not match any open container");
                    return SIML_EVENT_ERROR;
                }
                p->pending_close = 1;
                p->target_depth = target;
                return siml_emit_pending_end(p, ev);
            }

            if ((cur->type == SIML_CONTAINER_MAP && !is_mapping) ||
                (cur->type == SIML_CONTAINER_SEQ && !is_sequence)) {
                siml_set_error(p, SIML_ERR_BAD_CONTAINER, "line kind does not match current container");
                return SIML_EVENT_ERROR;
            }

            if (is_mapping) {
                if (!siml_parse_mapping_entry(p, s, len, indent,
                                              &key_len, &value_start, &value_len,
                                              &ic_spaces, &ic_ptr, &ic_len,
                                              &has_inline_value)) {
                    return SIML_EVENT_ERROR;
                }
                if (!siml_add_mapping_key(p, s + indent, key_len)) {
                    return SIML_EVENT_ERROR;
                }
                cur->item_count += 1;

                if (!has_inline_value) {
                    p->pending_node = 1;
                    p->pending_indent = indent + 2;
                    memcpy(p->pending_key, s + indent, key_len);
                    p->pending_key[key_len] = '\0';
                    p->pending_key_len = key_len;
                    p->have_line = 0;
                    continue;
                }

                if (value_len > SIML_MAX_INLINE_VALUE_LEN) {
                    siml_set_error(p, SIML_ERR_BAD_SCALAR, "inline value too long");
                    return SIML_EVENT_ERROR;
                }
                if (value_len == 0) {
                    siml_set_error(p, SIML_ERR_BAD_SCALAR, "inline value must be non-empty");
                    return SIML_EVENT_ERROR;
                }

                if (s[value_start] == '|') {
                    if (value_len != 1) {
                        siml_set_error(p, SIML_ERR_BAD_BLOCK_SCALAR, "block scalar marker must be '|'");
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
                    siml_set_error(p, SIML_ERR_BAD_SCALAR, "plain scalar cannot start with '[' or '|'");
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

            /* Sequence item */
            if (!siml_parse_sequence_item(p, s, len, indent,
                                          &value_start, &value_len,
                                          &ic_spaces, &ic_ptr, &ic_len,
                                          &has_inline_value)) {
                return SIML_EVENT_ERROR;
            }
            cur->item_count += 1;

            if (!has_inline_value) {
                p->pending_node = 1;
                p->pending_indent = indent + 2;
                p->pending_key[0] = '\0';
                p->pending_key_len = 0;
                p->have_line = 0;
                continue;
            }

            if (value_len > SIML_MAX_INLINE_VALUE_LEN) {
                siml_set_error(p, SIML_ERR_BAD_SCALAR, "inline value too long");
                return SIML_EVENT_ERROR;
            }
            if (value_len == 0) {
                siml_set_error(p, SIML_ERR_BAD_SCALAR, "inline value must be non-empty");
                return SIML_EVENT_ERROR;
            }

            if (s[value_start] == '|') {
                if (value_len != 1) {
                    siml_set_error(p, SIML_ERR_BAD_BLOCK_SCALAR, "block scalar marker must be '|'");
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
                siml_set_error(p, SIML_ERR_BAD_SCALAR, "plain scalar cannot start with '[' or '|'");
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

#endif /* SIML_IMPLEMENTATION */

#endif /* SIML_H_INCLUDED */
