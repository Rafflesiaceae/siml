#ifndef SIML_H_INCLUDED
#define SIML_H_INCLUDED

/*
 * SIML reference parser v1.0 (2025-12-04) (aae7bc0c269a3a6f)
 * see https://github.com/Vorschreibung/siml
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
#define SIML_MAX_KEY_LEN 64
#endif

/* Error codes */
typedef enum siml_error_code {
    SIML_ERR_NONE = 0,
    SIML_ERR_IO,
    SIML_ERR_BAD_INDENT,
    SIML_ERR_BAD_SEPARATOR,
    SIML_ERR_BAD_KEY,
    SIML_ERR_BAD_FIELD_SYNTAX,
    SIML_ERR_BAD_BLOCK_HEADER,
    SIML_ERR_BAD_BLOCK_CONTENT,
    SIML_ERR_BAD_LIST_SYNTAX,
    SIML_ERR_UNTERMINATED_LIST,
    SIML_ERR_UNEXPECTED_EOF,
    SIML_ERR_INTERNAL
} siml_error_code;

/* Event types for the pull parser */
typedef enum siml_event_type {
    SIML_EVENT_NONE = 0,
    SIML_EVENT_STREAM_START,
    SIML_EVENT_ITEM_START,
    SIML_EVENT_FIELD_SCALAR,
    SIML_EVENT_FIELD_LIST_BEGIN,
    SIML_EVENT_FIELD_LIST_ITEM,
    SIML_EVENT_FIELD_LIST_END,
    SIML_EVENT_FIELD_BLOCK_BEGIN,
    SIML_EVENT_FIELD_BLOCK_LINE,
    SIML_EVENT_FIELD_BLOCK_END,
    SIML_EVENT_ITEM_END,
    SIML_EVENT_STREAM_END,
    SIML_EVENT_ERROR
} siml_event_type;

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
 *  - For ITEM_* and STREAM_* events, key/value are empty slices.
 *  - For FIELD_* events, key is the field name.
 *  - For SCALAR, LIST_ITEM and BLOCK_LINE events, value is the text.
 *  - error_code / error_message are set only for SIML_EVENT_ERROR.
 */
typedef struct siml_event_s {
    siml_event_type  type;
    siml_slice       key;
    siml_slice       value;
    long             line;          /* 1-based physical line number */
    siml_error_code  error_code;
    const char      *error_message; /* static string; never NULL for ERROR */
} siml_event;

/* Parser mode (internal) */
typedef enum siml_mode_e {
    SIML_MODE_NORMAL = 0,
    SIML_MODE_LIST,
    SIML_MODE_BLOCK
} siml_mode;

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
    int               started;     /* STREAM_START already emitted? */
    int               in_item;     /* currently inside an item? */

    /* Current mode (normal / list / block) */
    siml_mode         mode;

    /* Field key buffer (copied from line) */
    char              key_buf[SIML_MAX_KEY_LEN + 1];
    size_t            key_len;

    /* List parsing state */
    size_t            list_pos;        /* index into line for list scanning */
    int               list_first_line; /* still on the field header line? */

    /* Block parsing state */
    long              block_start_line;

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
 * Returns the event type (same as ev->type). The sequence is:
 *   STREAM_START, (ITEM_START, FIELD_*, ITEM_END)*, STREAM_END
 * Errors are reported as SIML_EVENT_ERROR.
 */
siml_event_type siml_next(siml_parser *p, siml_event *ev);

#ifdef __cplusplus
} /* extern "C" */
#endif

/* ---------------- Implementation ---------------- */
#ifdef SIML_IMPLEMENTATION

#include <string.h> /* strlen */

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

static int siml_is_blank_line(const char *s, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        if (s[i] != ' ') return 0;
    }
    return 1;
}

static int siml_is_comment_line(const char *s, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        if (s[i] == ' ') continue;
        return (s[i] == '#');
    }
    return 0;
}

static int siml_is_doc_separator(const char *s, size_t len) {
    size_t i = 0;
    while (i < len && s[i] == ' ') {
        ++i;
    }
    if (i + 3 > len) return 0;
    if (s[i] != '-' || s[i+1] != '-' || s[i+2] != '-') return 0;
    i += 3;
    while (i < len && s[i] == ' ') {
        ++i;
    }
    if (i == len) return 1;
    if (s[i] != '#') return 0;
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
    if (!prefix) prefix = "";
    while (prefix[i] != '\0' && i + 1 < sizeof(buf)) {
        buf[i] = prefix[i];
        ++i;
    }
    if (key && key_len > 0) {
        size_t k = 0;
        while (k < key_len && i + 2 < sizeof(buf)) { /* keep room for closing quote */
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
    ev->type          = SIML_EVENT_NONE;
    ev->key.ptr       = 0;
    ev->key.len       = 0;
    ev->value.ptr     = 0;
    ev->value.len     = 0;
    ev->line          = 0;
    ev->error_code    = SIML_ERR_NONE;
    ev->error_message = 0;
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

/* Skip blank and comment lines. Leaves the first non-blank/non-comment
 * line in parser->line (have_line=1), or reaches EOF.
 * Returns 1 if a line is available, 0 if EOF, -1 on error.
 */
static int siml_skip_blank_and_comment(siml_parser *p) {
    int rc;
    for (;;) {
        if (!p->have_line) {
            rc = siml_fetch_line(p);
            if (rc <= 0) return rc;
        }
        if (p->line_len == 0 || siml_is_blank_line(p->line, p->line_len) ||
            siml_is_comment_line(p->line, p->line_len)) {
            p->have_line = 0;
            continue;
        }
        return 1;
    }
}

/* Strip inline comment and trailing spaces for scalar values.
 * Returns resulting length. Does NOT modify the underlying memory.
 * On error (invalid characters), returns (size_t)(-1).
 */
static size_t siml_scalar_strip(const char *v, size_t len) {
    size_t i;
    size_t end = len;

    /* Look for inline comment start (space before #). */
    for (i = 0; i < len; ++i) {
        char c = v[i];
        if (c == '#') {
            if (i > 0 && v[i-1] == ' ') {
                end = i - 1; /* strip space before # */
                break;
            }
        } else if (c == ',' || c == ']') {
            return (size_t)(-1); /* invalid inside scalar */
        }
    }

    if (end == len) {
        end = len;
    }
    /* Trim trailing spaces */
    while (end > 0 && v[end-1] == ' ') {
        --end;
    }

    return end;
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
    p->in_item   = 0;
    p->mode      = SIML_MODE_NORMAL;
    p->key_buf[0] = '\0';
    p->key_len   = 0;
    p->list_pos  = 0;
    p->list_first_line = 0;
    p->block_start_line = 0;
    p->error_code = SIML_ERR_NONE;
    p->error_message = 0;
    p->error_line = 0;
}

/* Forward declarations of internal state handlers */
static siml_event_type siml_next_normal(siml_parser *p, siml_event *ev);
static siml_event_type siml_next_list(siml_parser *p, siml_event *ev);
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

    if (p->mode == SIML_MODE_LIST) {
        t = siml_next_list(p, ev);
    } else if (p->mode == SIML_MODE_BLOCK) {
        t = siml_next_block(p, ev);
    } else {
        t = siml_next_normal(p, ev);
    }

    if (t == SIML_EVENT_ERROR && p->error_code != SIML_ERR_NONE) {
        ev->error_code    = p->error_code;
        ev->error_message = p->error_message;
        if (ev->line == 0) ev->line = p->error_line;
    }
    return t;
}

/* Normal mode: not inside list or block. */
static siml_event_type siml_next_normal(siml_parser *p, siml_event *ev) {
    int rc;

    for (;;) {
        /* If we are inside an item and already hit EOF and have no line,
         * then the item ends here.
         */
        if (p->in_item && p->at_eof && !p->have_line) {
            p->in_item = 0;
            ev->type = SIML_EVENT_ITEM_END;
            ev->line = p->line_no;
            return ev->type;
        }

        /* Find the next non-blank/non-comment line. */
        rc = siml_skip_blank_and_comment(p);
        if (rc < 0) {
            return SIML_EVENT_ERROR;
        }
        if (rc == 0) {
            /* EOF */
            if (p->in_item) {
                p->in_item = 0;
                ev->type = SIML_EVENT_ITEM_END;
                ev->line = p->line_no;
                return ev->type;
            }
            ev->type = SIML_EVENT_STREAM_END;
            ev->line = p->line_no;
            return ev->type;
        }

        /* Now we have a non-blank, non-comment line. */
        if (siml_is_doc_separator(p->line, p->line_len)) {
            /* Separator line */
            p->have_line = 0; /* consume it */
            if (p->in_item) {
                p->in_item = 0;
                ev->type = SIML_EVENT_ITEM_END;
                ev->line = p->line_no;
                return ev->type;
            }
            /* Separator while already between items: ignore and look again. */
            continue;
        }
        break;
    }

    /* At this point, the line must be a field. If we are not yet in an item,
     * then this starts a new item and we emit ITEM_START first, without
     * consuming the line.
     */
    if (!p->in_item) {
        p->in_item = 1;
        ev->type = SIML_EVENT_ITEM_START;
        ev->line = p->line_no;
        return ev->type;
    }

    /* Parse field line in current item. */
    {
        const char *s = p->line;
        size_t len = p->line_len;
        size_t i = 0;
        size_t key_start = 0;
        size_t key_end = 0;
        size_t k;

        if (len == 0 || s[0] == ' ' || s[0] == '\t') {
            siml_set_error(p, SIML_ERR_BAD_INDENT, "expected field at column 0 to start document");
            return SIML_EVENT_ERROR;
        }

        /* Parse key */
        if (!siml_is_alpha(s[0]) && s[0] != '_') {
            siml_set_error(p, SIML_ERR_BAD_KEY, "expected field at column 0 to start document");
            return SIML_EVENT_ERROR;
        }
        i = 1;
        while (i < len && siml_is_key_char(s[i])) {
            ++i;
        }
        key_start = 0;
        key_end = i;

        if (i >= len || s[i] != ':') {
            siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX, "expected ':' after key");
            return SIML_EVENT_ERROR;
        }

        if (key_end - key_start > SIML_MAX_KEY_LEN) {
            siml_set_error(p, SIML_ERR_BAD_KEY, "key too long");
            return SIML_EVENT_ERROR;
        }
        for (k = 0; k < key_end - key_start; ++k) {
            p->key_buf[k] = s[key_start + k];
        }
        p->key_buf[key_end - key_start] = '\0';
        p->key_len = key_end - key_start;

        ++i; /* skip ':' */
        if (i < len) {
            if (s[i] != ' ') {
                siml_set_error_with_key(p, SIML_ERR_BAD_FIELD_SYNTAX,
                                        "expected space after ':' in field '",
                                        p->key_buf, p->key_len);
                return SIML_EVENT_ERROR;
            }
            ++i; /* skip space after colon */
        }

        /* Raw value text */
        {
            const char *v = s + i;
            size_t vlen = (len >= i) ? (len - i) : 0;

            if (vlen > 0 && v[0] == '|') {
                /* Literal block header */
                size_t j = 1;
                while (j < vlen && v[j] == ' ') {
                    ++j;
                }
                if (j < vlen && v[j] != '#') {
                    siml_set_error(p, SIML_ERR_BAD_BLOCK_HEADER,
                                   "unexpected characters after '|' block marker");
                    return SIML_EVENT_ERROR;
                }
                p->mode = SIML_MODE_BLOCK;
                p->block_start_line = p->line_no;
                p->have_line = 0; /* content starts on the next physical line */

                ev->type = SIML_EVENT_FIELD_BLOCK_BEGIN;
                ev->key  = siml_make_slice(p->key_buf, p->key_len);
                ev->value.ptr = 0;
                ev->value.len = 0;
                ev->line = p->line_no;
                return ev->type;
            } else if (vlen > 0 && v[0] == '[') {
                /* List header */
                p->mode = SIML_MODE_LIST;
                p->list_first_line = 1;
                p->list_pos = i; /* index in s where '[' appears */

                ev->type = SIML_EVENT_FIELD_LIST_BEGIN;
                ev->key  = siml_make_slice(p->key_buf, p->key_len);
                ev->value.ptr = 0;
                ev->value.len = 0;
                ev->line = p->line_no;
                return ev->type;
            } else {
                /* Scalar */
                size_t slen;
                slen = siml_scalar_strip(v, vlen);
                if (slen == (size_t)(-1)) {
                    siml_set_error(p, SIML_ERR_BAD_FIELD_SYNTAX,
                                   "invalid scalar value");
                    return SIML_EVENT_ERROR;
                }
                p->have_line = 0; /* fully consumed */

                ev->type = SIML_EVENT_FIELD_SCALAR;
                ev->key  = siml_make_slice(p->key_buf, p->key_len);
                ev->value = siml_make_slice(v, slen);
                ev->line = p->line_no;
                return ev->type;
            }
        }
    }
}

/* List mode: parse list items across one or more lines. */
static int siml_fetch_list_line(siml_parser *p) {
    int rc;
    for (;;) {
        rc = siml_fetch_line(p);
        if (rc <= 0) return rc;
        if (p->line_len == 0 || siml_is_blank_line(p->line, p->line_len) ||
            siml_is_comment_line(p->line, p->line_len)) {
            p->have_line = 0;
            continue; /* skip blank/comment inside list */
        }
        p->list_pos = 0;
        return 1;
    }
}

static siml_event_type siml_next_list(siml_parser *p, siml_event *ev) {
    const char *s;
    size_t len;
    size_t i;
    size_t start;
    size_t end;
    int rc;

    for (;;) {
        if (!p->have_line) {
            if (p->list_first_line) {
                /* Should not happen; the first line is the field header.
                 * If it does, treat as internal error.
                 */
                siml_set_error(p, SIML_ERR_INTERNAL, "lost first list line");
                return SIML_EVENT_ERROR;
            }
            rc = siml_fetch_list_line(p);
            if (rc < 0) return SIML_EVENT_ERROR;
            if (rc == 0) {
                siml_set_error(p, SIML_ERR_UNTERMINATED_LIST,
                               "missing ']'");
                return SIML_EVENT_ERROR;
            }
        }

        s = p->line;
        len = p->line_len;
        i = p->list_pos;

        /* First-line handling: skip up to and including '[' once. */
        if (p->list_first_line) {
            /* list_pos points to '[' from the header line */
            if (i >= len || s[i] != '[') {
                siml_set_error(p, SIML_ERR_BAD_LIST_SYNTAX,
                               "list header must start with '['");
                return SIML_EVENT_ERROR;
            }
            i++; /* skip '[' */
            while (i < len && s[i] == ' ') {
                ++i;
            }
            p->list_first_line = 0;
            p->list_pos = i;
        }

        /* Skip leading spaces */
        while (i < len && s[i] == ' ') {
            ++i;
        }

        if (i >= len) {
            /* Nothing more on this line; move to next line. */
            p->have_line = 0;
            p->list_pos = 0;
            continue;
        }

        /* Check for closing bracket */
        if (s[i] == ']') {
            size_t j = i + 1;
            while (j < len && s[j] == ' ') {
                ++j;
            }
            if (j < len && s[j] != '#') {
                siml_set_error(p, SIML_ERR_BAD_LIST_SYNTAX,
                               "unexpected characters after closing ']' in list");
                return SIML_EVENT_ERROR;
            }
            /* Done with this line and list */
            p->have_line = 0;
            p->mode = SIML_MODE_NORMAL;
            p->list_pos = 0;

            ev->type = SIML_EVENT_FIELD_LIST_END;
            ev->key  = siml_make_slice(p->key_buf, p->key_len);
            ev->value.ptr = 0;
            ev->value.len = 0;
            ev->line = p->line_no;
            return ev->type;
        }

        /* Skip full-line comment (only if first non-space is '#') */
        if (s[i] == '#') {
            p->have_line = 0;
            p->list_pos = 0;
            continue;
        }

        /* Document separator inside list is invalid. */
        if (siml_is_doc_separator(s + i, (len >= i) ? (len - i) : 0)) {
            siml_set_error(p, SIML_ERR_BAD_LIST_SYNTAX, "missing ']'");
            return SIML_EVENT_ERROR;
        }

        /* Parse list element: from i up to ',' or ']' */
        start = i;
        while (i < len && s[i] != ',' && s[i] != ']') {
            if (s[i] == '\n' || s[i] == '\r') {
                break;
            }
            ++i;
        }
        end = i;
        while (end > start && s[end-1] == ' ') {
            --end;
        }
        if (end == start) {
            /* Empty element is not allowed */
            if (p->key_len > 0) {
                siml_set_error_with_key(p, SIML_ERR_BAD_LIST_SYNTAX,
                                        "empty list element in key '",
                                        p->key_buf, p->key_len);
            } else {
                siml_set_error(p, SIML_ERR_BAD_LIST_SYNTAX,
                               "empty list element");
            }
            return SIML_EVENT_ERROR;
        }

        ev->type = SIML_EVENT_FIELD_LIST_ITEM;
        ev->key  = siml_make_slice(p->key_buf, p->key_len);
        ev->value = siml_make_slice(s + start, end - start);
        ev->line = p->line_no;

        /* Advance past delimiter (if any) but leave ']' for the next call. */
        if (i < len && s[i] == ',') {
            ++i;
        }
        p->list_pos = i;
        return ev->type;
    }
}

/* Block mode: parse literal block lines. */
static siml_event_type siml_next_block(siml_parser *p, siml_event *ev) {
    int rc;

    /* Need a line if we don't have one. */
    if (!p->have_line) {
        rc = siml_fetch_line(p);
        if (rc < 0) return SIML_EVENT_ERROR;
        if (rc == 0) {
            /* EOF ends the block */
            p->mode = SIML_MODE_NORMAL;
            ev->type = SIML_EVENT_FIELD_BLOCK_END;
            ev->key  = siml_make_slice(p->key_buf, p->key_len);
            ev->value.ptr = 0;
            ev->value.len = 0;
            ev->line = p->block_start_line;
            return ev->type;
        }
    }

    /* Check for termination: any line whose first character is not space
     * ends the block but is NOT consumed here. Empty lines belong to the
     * block (they become empty lines in the resulting string).
     */
    if (p->line_len > 0 && p->line[0] != ' ') {
        p->mode = SIML_MODE_NORMAL;
        ev->type = SIML_EVENT_FIELD_BLOCK_END;
        ev->key  = siml_make_slice(p->key_buf, p->key_len);
        ev->value.ptr = 0;
        ev->value.len = 0;
        ev->line = p->block_start_line;
        return ev->type;
    }

    /* Empty line handling: include it only if the block continues. */
    if (p->line_len == 0) {
        int peek_rc;
        /* Look ahead to decide whether the block continues. */
        peek_rc = siml_fetch_line(p);
        if (peek_rc < 0) return SIML_EVENT_ERROR;
        if (peek_rc == 0 || (p->line_len > 0 && p->line[0] != ' ')) {
            /* Trailing blank line before termination: do not include it. */
            p->mode = SIML_MODE_NORMAL;
            ev->type = SIML_EVENT_FIELD_BLOCK_END;
            ev->key  = siml_make_slice(p->key_buf, p->key_len);
            ev->value.ptr = 0;
            ev->value.len = 0;
            ev->line = p->block_start_line;
            return ev->type;
        }
        /* Block continues; emit the empty line and keep the peeked line for next call. */
        ev->type = SIML_EVENT_FIELD_BLOCK_LINE;
        ev->key  = siml_make_slice(p->key_buf, p->key_len);
        ev->value = siml_make_slice("", 0);
        ev->line = p->line_no;
        return ev->type;
    }

    /* Non-empty content line must start with exactly two spaces. */
    if (p->line_len < 2 || p->line[0] != ' ' || p->line[1] != ' ') {
        siml_set_error(p, SIML_ERR_BAD_BLOCK_CONTENT,
                       "block line must start with exactly two spaces");
        return SIML_EVENT_ERROR;
    }

    ev->type = SIML_EVENT_FIELD_BLOCK_LINE;
    ev->key  = siml_make_slice(p->key_buf, p->key_len);
    ev->value = siml_make_slice(p->line + 2,
                                (p->line_len >= 2) ? (p->line_len - 2) : 0);
    ev->line = p->line_no;

    p->have_line = 0; /* consume this line */
    return ev->type;
}

#endif /* SIML_IMPLEMENTATION */

#endif /* SIML_H_INCLUDED */
