#ifndef SIML_H_INCLUDED
#define SIML_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifndef SIML_MAX_LINE
#define SIML_MAX_LINE 4096
#endif

#ifndef SIML_MAX_KEY
#define SIML_MAX_KEY 64
#endif

enum siml_event_type {
    SIML_EVENT_NONE = 0,
    SIML_EVENT_BEGIN_ITEM,
    SIML_EVENT_SCALAR,
    SIML_EVENT_LIST_ELEMENT,
    SIML_EVENT_LIST_EMPTY,
    SIML_EVENT_BEGIN_LITERAL,
    SIML_EVENT_LITERAL_LINE,
    SIML_EVENT_END_LITERAL,
    SIML_EVENT_ERROR,
    SIML_EVENT_EOF
};

struct siml_event {
    enum siml_event_type type;
    int item_index;
    long line_no;
    char *key;
    char *value;
    char *message;
};

struct siml_iter;

int siml_iter_init(struct siml_iter *it, FILE *fp, const char *filename);
void siml_iter_destroy(struct siml_iter *it);
int siml_next(struct siml_iter *it, struct siml_event *ev);
void siml_event_cleanup(struct siml_event *ev);

#ifdef __cplusplus
}
#endif

#ifdef SIML_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif

enum siml_state {
    ST_OUTSIDE = 0,
    ST_ITEM,
    ST_BLOCK_LITERAL,
    ST_BLOCK_LIST
};

struct siml_iter {
    FILE *fp;
    const char *filename;
    char line[SIML_MAX_LINE];
    long line_no;
    enum siml_state state;
    int item_index;
    char block_list_key[SIML_MAX_KEY];
    int block_list_has_items;
    int block_list_pending_scalar;
    char literal_key[SIML_MAX_KEY];
    char **block_lines;
    size_t block_lines_count;
    size_t block_lines_cap;
    int block_min_indent;
    struct siml_event *pending;
    size_t pending_count;
    size_t pending_cap;
    int finished;
    int failed;
};

void
siml_event_cleanup(struct siml_event *ev)
{
    if (!ev) {
        return;
    }
    free(ev->key);
    free(ev->value);
    free(ev->message);
    memset(ev, 0, sizeof(*ev));
}

static char *
siml_strdup(const char *s)
{
    size_t len;
    char *copy;

    if (!s) {
        return NULL;
    }
    len = strlen(s);
    copy = (char *) malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, s, len + 1);
    return copy;
}

static void
siml_trim_newline(char *s)
{
    size_t len = strlen(s);

    while (len > 0 &&
           (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

static void
siml_rstrip_spaces(char *s)
{
    size_t len = strlen(s);

    while (len > 0 &&
           (s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

static char *
siml_lskip_spaces(char *s)
{
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

static int
siml_is_blank(const char *s)
{
    const char *p = s;

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return *p == '\0';
}

static int
siml_is_comment(const char *s)
{
    const char *p = s;

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return *p == '#';
}

static int
siml_is_doc_separator(const char *line)
{
    const char *p;

    if (!(line[0] == '-' && line[1] == '-' && line[2] == '-')) {
        return 0;
    }

    p = line + 3;
    if (*p == '\0') {
        return 1;
    }
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return (*p == '\0' || *p == '#');
}

static int
siml_is_valid_key(const char *key)
{
    const unsigned char *p = (const unsigned char *) key;

    if (*p == '\0') {
        return 0;
    }
    if (!isalpha(*p) && *p != '_') {
        return 0;
    }
    p++;
    while (*p) {
        if (!isalnum(*p) && *p != '_') {
            return 0;
        }
        p++;
    }
    return 1;
}

static int
siml_is_valid_key_span(const char *start, size_t len)
{
    size_t i;

    if (len == 0) {
        return 0;
    }
    if (!isalpha((unsigned char) start[0]) && start[0] != '_') {
        return 0;
    }
    for (i = 1; i < len; i++) {
        unsigned char c = (unsigned char) start[i];

        if (!isalnum(c) && c != '_') {
            return 0;
        }
    }
    return 1;
}

static void
siml_block_buffer_reset(struct siml_iter *p)
{
    size_t i;

    for (i = 0; i < p->block_lines_count; i++) {
        free(p->block_lines[i]);
    }
    free(p->block_lines);
    p->block_lines = NULL;
    p->block_lines_count = 0;
    p->block_lines_cap = 0;
    p->block_min_indent = -1;
}

static int
siml_block_buffer_append(struct siml_iter *p,
                         const char *line,
                         int indent_is_spaces)
{
    char *copy;

    if (p->block_lines_count == p->block_lines_cap) {
        size_t new_cap = p->block_lines_cap ? p->block_lines_cap * 2 : 8;
        char **new_lines = (char **) realloc(p->block_lines,
                                             new_cap * sizeof(char *));

        if (!new_lines) {
            return 1;
        }
        p->block_lines = new_lines;
        p->block_lines_cap = new_cap;
    }

    copy = siml_strdup(line);
    if (!copy) {
        return 1;
    }
    p->block_lines[p->block_lines_count++] = copy;

    if (indent_is_spaces >= 0) {
        if (p->block_min_indent == -1 ||
            indent_is_spaces < p->block_min_indent) {
            p->block_min_indent = indent_is_spaces;
        }
    }
    return 0;
}

static void
siml_queue_clear(struct siml_iter *p)
{
    size_t i;

    for (i = 0; i < p->pending_count; i++) {
        siml_event_cleanup(&p->pending[i]);
    }
    free(p->pending);
    p->pending = NULL;
    p->pending_count = 0;
    p->pending_cap = 0;
}

static int
siml_queue_event(struct siml_iter *p,
                 enum siml_event_type type,
                 const char *key,
                 const char *value,
                 const char *message,
                 int item_index,
                 long line_no)
{
    struct siml_event ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.item_index = item_index;
    ev.line_no = line_no;
    ev.key = siml_strdup(key);
    ev.value = siml_strdup(value);
    ev.message = siml_strdup(message);

    if ((key && !ev.key) || (value && !ev.value) || (message && !ev.message)) {
        siml_event_cleanup(&ev);
        return 1;
    }

    if (p->pending_count == p->pending_cap) {
        size_t new_cap = p->pending_cap ? p->pending_cap * 2 : 16;
        struct siml_event *new_events =
            (struct siml_event *) realloc(p->pending,
                                          new_cap * sizeof(*p->pending));

        if (!new_events) {
            siml_event_cleanup(&ev);
            return 1;
        }
        p->pending = new_events;
        p->pending_cap = new_cap;
    }

    p->pending[p->pending_count++] = ev;
    return 0;
}

static int
siml_fail(struct siml_iter *p, long line_no, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    int rc;

    p->failed = 1;
    va_start(ap, fmt);
    rc = vsnprintf(buf, sizeof(buf), fmt, ap);
    (void) rc;
    va_end(ap);

    return siml_queue_event(p,
                            SIML_EVENT_ERROR,
                            NULL,
                            NULL,
                            buf,
                            p->item_index,
                            line_no);
}

static int
siml_block_buffer_emit(struct siml_iter *p, long line_no)
{
    size_t i;
    int trim = (p->block_min_indent < 0) ? 0 : p->block_min_indent;

    while (p->block_lines_count > 0) {
        char *line = p->block_lines[p->block_lines_count - 1];
        char *s = line;

        while (*s == ' ' || *s == '\t') {
            s++;
        }
        if (*s == '\0') {
            free(line);
            p->block_lines_count--;
            continue;
        }
        break;
    }

    for (i = 0; i < p->block_lines_count; i++) {
        char *line = p->block_lines[i];
        char *s = line;
        int to_trim = trim;

        while (to_trim > 0 && *s == ' ') {
            s++;
            to_trim--;
        }
        if (siml_queue_event(p,
                             SIML_EVENT_LITERAL_LINE,
                             NULL,
                             s,
                             NULL,
                             p->item_index,
                             line_no) != 0) {
            siml_block_buffer_reset(p);
            return 1;
        }
    }

    siml_block_buffer_reset(p);
    return 0;
}

static int
siml_is_field_line(struct siml_iter *p,
                   const char *line,
                   int indent,
                   int *body_offset)
{
    const char *body;
    const char *colon;
    const char *key_end;

    (void) p;

    if (!(indent == 0 || indent == 2)) {
        return 0;
    }

    body = line + indent;
    if (siml_is_blank(body) || siml_is_comment(body)) {
        return 0;
    }

    colon = strchr(body, ':');
    if (!colon) {
        return 0;
    }

    key_end = colon;
    while (key_end > body && (key_end[-1] == ' ' || key_end[-1] == '\t')) {
        key_end--;
    }
    if (key_end == body) {
        return 0;
    }

    if (!siml_is_valid_key_span(body,
                                (size_t) (key_end - body))) {
        return 0;
    }

    if (colon[1] != '\0' && colon[1] != ' ') {
        return 0;
    }

    if (body_offset) {
        *body_offset = indent;
    }
    return 1;
}

static int
siml_parse_field(struct siml_iter *p,
                 char *body,
                 long line_no)
{
    char *colon;
    char *key;
    char *value;

    siml_rstrip_spaces(body);

    colon = strchr(body, ':');
    if (!colon) {
        return siml_fail(p, line_no, "%s:%ld: missing ':' in field",
                         p->filename, line_no);
    }

    *colon = '\0';
    key = body;
    siml_rstrip_spaces(key);

    if (!siml_is_valid_key(key)) {
        return siml_fail(p, line_no, "%s:%ld: invalid key '%s'",
                         p->filename, line_no, key);
    }

    value = colon + 1;
    if (*value == ' ') {
        value++;
    } else if (*value != '\0') {
        return siml_fail(p, line_no,
                         "%s:%ld: expected space after ':' in field '%s'",
                         p->filename, line_no, key);
    }

    /* detect block list (empty value, ignoring inline comment) */
    {
        int value_is_empty = 1;
        char last = ' ';
        char *scan = value;

        while (*scan) {
            if (*scan == '#' &&
                (last == ' ' || last == '\t')) {
                break;
            }
            if (*scan != ' ' && *scan != '\t') {
                value_is_empty = 0;
                break;
            }
            last = *scan;
            scan++;
        }

        if (value_is_empty) {
            size_t key_len = strlen(key);

            if (key_len + 1 > sizeof(p->block_list_key)) {
                return siml_fail(p, line_no,
                                 "%s:%ld: block list key too long '%s'",
                                 p->filename, line_no, key);
            }
            strcpy(p->block_list_key, key);
            p->block_list_has_items = 0;
            p->block_list_pending_scalar = 1;
            p->state = ST_BLOCK_LIST;
            return 0;
        }
    }

    if (value[0] == '|') {
        size_t key_len = strlen(key);

        if (key_len + 1 > sizeof(p->literal_key)) {
            return siml_fail(p, line_no,
                             "%s:%ld: literal block key too long '%s'",
                             p->filename, line_no, key);
        }
        strcpy(p->literal_key, key);
        siml_block_buffer_reset(p);
        if (siml_queue_event(p,
                             SIML_EVENT_BEGIN_LITERAL,
                             key,
                             NULL,
                             NULL,
                             p->item_index,
                             line_no) != 0) {
            return siml_fail(p, line_no,
                             "%s:%ld: out of memory buffering literal",
                             p->filename, line_no);
        }
        p->state = ST_BLOCK_LITERAL;
        return 0;
    }

    if (value[0] == '[') {
        /* list value */
        char *closing;
        char *p_char;
        char *inside;
        char *end;
        char *elem;

        closing = strrchr(value, ']');
        if (!closing) {
            return siml_fail(p, line_no,
                             "%s:%ld: list for key '%s' missing ']'",
                             p->filename, line_no, key);
        }
        p_char = closing + 1;
        while (*p_char == ' ' || *p_char == '\t') {
            p_char++;
        }
        if (*p_char != '\0' && *p_char != '#') {
            return siml_fail(p, line_no,
                             "%s:%ld: unexpected text after list for key '%s'",
                             p->filename, line_no, key);
        }
        /* terminate at ']' */
        closing[1] = '\0';
        siml_rstrip_spaces(value);

        /* now parse inside [ ... ] */
        inside = value + 1;
        end = strchr(inside, ']');
        if (!end) {
            return siml_fail(p, line_no,
                             "%s:%ld: internal error parsing list '%s'",
                             p->filename, line_no, key);
        }
        *end = '\0';

        elem = siml_lskip_spaces(inside);
        if (*elem == '\0') {
            if (siml_queue_event(p,
                                 SIML_EVENT_LIST_EMPTY,
                                 key,
                                 NULL,
                                 NULL,
                                 p->item_index,
                                 line_no) != 0) {
                return siml_fail(p, line_no,
                                 "%s:%ld: out of memory",
                                 p->filename, line_no);
            }
            return 0;
        }

        while (*elem) {
            char *comma;
            char *next;

            comma = strchr(elem, ',');
            if (comma) {
                *comma = '\0';
                next = comma + 1;
            } else {
                next = elem + strlen(elem);
            }

            siml_rstrip_spaces(elem);
            if (*elem == '\0') {
                return siml_fail(p, line_no,
                                 "%s:%ld: empty list element in key '%s'",
                                 p->filename, line_no, key);
            }

            if (siml_queue_event(p,
                                 SIML_EVENT_LIST_ELEMENT,
                                 key,
                                 elem,
                                 NULL,
                                 p->item_index,
                                 line_no) != 0) {
                return siml_fail(p, line_no,
                                 "%s:%ld: out of memory",
                                 p->filename, line_no);
            }

            elem = siml_lskip_spaces(next);
            if (*elem == '\0') {
                break;
            }
        }

        return 0;
    } else {
        /* scalar, handle inline comment */
        char *hash_scan = value;
        char *closing_hash = NULL;
        char last = '\0';

        while (*hash_scan) {
            if (*hash_scan == '#' &&
                (last == ' ' || last == '\t')) {
                closing_hash = hash_scan;
                break;
            }
            last = *hash_scan;
            hash_scan++;
        }
        if (closing_hash) {
            *closing_hash = '\0';
        }
        siml_rstrip_spaces(value);
        if (siml_queue_event(p,
                             SIML_EVENT_SCALAR,
                             key,
                             value,
                             NULL,
                             p->item_index,
                             line_no) != 0) {
            return siml_fail(p, line_no,
                             "%s:%ld: out of memory",
                             p->filename, line_no);
        }
        return 0;
    }
}

int
siml_iter_init(struct siml_iter *p, FILE *fp, const char *filename)
{
    if (!p || !fp) {
        return 1;
    }
    memset(p, 0, sizeof(*p));
    p->fp = fp;
    p->filename = filename ? filename : "<input>";
    p->state = ST_OUTSIDE;
    p->item_index = -1;
    p->block_list_key[0] = '\0';
    p->literal_key[0] = '\0';
    return 0;
}

void
siml_iter_destroy(struct siml_iter *p)
{
    if (!p) {
        return;
    }
    siml_block_buffer_reset(p);
    siml_queue_clear(p);
}

static int
siml_emit_pending_block_list_if_needed(struct siml_iter *p, long line_no)
{
    if (p->state == ST_BLOCK_LIST &&
        !p->block_list_has_items) {
        if (p->block_list_pending_scalar) {
            if (siml_queue_event(p,
                                 SIML_EVENT_SCALAR,
                                 p->block_list_key,
                                 "",
                                 NULL,
                                 p->item_index,
                                 line_no) != 0) {
                return siml_fail(p, line_no,
                                 "%s:%ld: out of memory",
                                 p->filename, line_no);
            }
        } else {
            if (siml_queue_event(p,
                                 SIML_EVENT_LIST_EMPTY,
                                 p->block_list_key,
                                 NULL,
                                 NULL,
                                 p->item_index,
                                 line_no) != 0) {
                return siml_fail(p, line_no,
                                 "%s:%ld: out of memory",
                                 p->filename, line_no);
            }
        }
    }
    p->block_list_pending_scalar = 0;
    p->block_list_has_items = 0;
    return 0;
}

static int
siml_finish_literal_if_needed(struct siml_iter *p, long line_no)
{
    if (p->state == ST_BLOCK_LITERAL) {
        if (siml_block_buffer_emit(p, line_no) != 0) {
            return siml_fail(p, line_no,
                             "%s:%ld: out of memory buffering literal block",
                             p->filename, line_no);
        }
        if (siml_queue_event(p,
                             SIML_EVENT_END_LITERAL,
                             p->literal_key,
                             NULL,
                             NULL,
                             p->item_index,
                             line_no) != 0) {
            return siml_fail(p, line_no,
                             "%s:%ld: out of memory",
                             p->filename, line_no);
        }
        p->state = ST_ITEM;
        p->literal_key[0] = '\0';
    }
    return 0;
}

static int
siml_process_line(struct siml_iter *p, char *line)
{
    char *p_line;
    long line_no = p->line_no;

    if (p->state == ST_BLOCK_LITERAL) {
        int indent = 0;
        char *bp = line;
        int body_offset = 0;
        char *s;

        if (siml_is_doc_separator(line)) {
            if (siml_finish_literal_if_needed(p, line_no) != 0) {
                return 1;
            }
            p->state = ST_OUTSIDE;
            return 0;
        }

        while (*bp == ' ') {
            indent++;
            bp++;
        }
        if (indent == 0 && *bp == '#') {
            if (siml_finish_literal_if_needed(p, line_no) != 0) {
                return 1;
            }
            p->state = ST_ITEM;
            return 0;
        }
        if (siml_is_field_line(p, line, indent, &body_offset)) {
            if (siml_block_buffer_emit(p, line_no) != 0) {
                return siml_fail(p, line_no,
                                 "%s:%ld: out of memory buffering literal block",
                                 p->filename, line_no);
            }
            if (siml_queue_event(p,
                                 SIML_EVENT_END_LITERAL,
                                 p->literal_key,
                                 NULL,
                                 NULL,
                                 p->item_index,
                                 line_no) != 0) {
                return siml_fail(p, line_no,
                                 "%s:%ld: out of memory",
                                 p->filename, line_no);
            }
            p->state = ST_ITEM;
            return siml_parse_field(p,
                                    line + body_offset,
                                    line_no);
        }
        s = line;
        while (*s == ' ') {
            s++;
        }
        if (siml_is_blank(s)) {
            if (siml_block_buffer_append(p, "", -1) != 0) {
                return siml_fail(p, line_no,
                                 "%s:%ld: out of memory buffering literal block",
                                 p->filename, line_no);
            }
            return 0;
        }
        if (siml_block_buffer_append(p,
                                     line,
                                     indent) != 0) {
            return siml_fail(p, line_no,
                             "%s:%ld: out of memory buffering literal block",
                             p->filename, line_no);
        }
        return 0;
    }

    if (p->state == ST_BLOCK_LIST) {
        char *bp = siml_lskip_spaces(line);
        int indent = (int) (bp - line);
        int body_offset = 0;

        if (siml_is_doc_separator(line)) {
            if (siml_emit_pending_block_list_if_needed(p, line_no) != 0) {
                return 1;
            }
            p->state = ST_OUTSIDE;
            return 0;
        }

        if (siml_is_blank(bp) || siml_is_comment(bp)) {
            return 0;
        }

        if (indent >= 2 &&
            bp[0] == '-' && bp[1] == ' ') {
            /* block list element */
            char *elem = bp + 2;
            char *hash_scan = elem;
            char *closing_hash = NULL;
            char last = ' ';

            while (*hash_scan) {
                if (*hash_scan == '#' &&
                    (last == ' ' || last == '\t')) {
                    closing_hash = hash_scan;
                    break;
                }
                last = *hash_scan;
                hash_scan++;
            }
            if (closing_hash) {
                *closing_hash = '\0';
            }
            siml_rstrip_spaces(elem);
            if (*elem == '\0') {
                return siml_fail(p, line_no,
                                 "%s:%ld: empty block list element for key '%s'",
                                 p->filename, line_no,
                                 p->block_list_key);
            }
            if (siml_queue_event(p,
                                 SIML_EVENT_LIST_ELEMENT,
                                 p->block_list_key,
                                 elem,
                                 NULL,
                                 p->item_index,
                                 line_no) != 0) {
                return siml_fail(p, line_no,
                                 "%s:%ld: out of memory",
                                 p->filename, line_no);
            }
            p->block_list_has_items = 1;
            p->block_list_pending_scalar = 0;
            return 0;
        } else if (siml_is_field_line(p, line, indent, &body_offset)) {
            int rc;
            char *body = line + body_offset;

            if (siml_emit_pending_block_list_if_needed(p, line_no) != 0) {
                return 1;
            }
            p->state = ST_ITEM;
            rc = siml_parse_field(p, body, line_no);
            return rc;
        } else {
            return siml_fail(p, line_no,
                             "%s:%ld: expected block list item, field, or document separator",
                             p->filename, line_no);
        }
    }

    p_line = siml_lskip_spaces(line);
    if (siml_is_blank(p_line) || siml_is_comment(p_line)) {
        return 0;
    }

    if (siml_is_doc_separator(line)) {
        p->state = ST_OUTSIDE;
        return 0;
    }

    if (p->state == ST_OUTSIDE) {
        int rc;
        int body_offset = 0;
        int indent = (int) (p_line - line);

        if (!siml_is_field_line(p, line, indent, &body_offset)) {
            return siml_fail(p, line_no,
                             "%s:%ld: expected field to start document (use '---' between documents)",
                             p->filename, line_no);
        }

        p->item_index++;
        if (siml_queue_event(p,
                             SIML_EVENT_BEGIN_ITEM,
                             NULL,
                             NULL,
                             NULL,
                             p->item_index,
                             line_no) != 0) {
            return siml_fail(p, line_no,
                             "%s:%ld: out of memory",
                             p->filename, line_no);
        }
        p->state = ST_ITEM;
        rc = siml_parse_field(p, line + body_offset, line_no);
        return rc;
    }

    /* ST_ITEM */
    {
        int rc;
        int body_offset = 0;
        int indent = (int) (p_line - line);

        if (!siml_is_field_line(p, line, indent, &body_offset)) {
            return siml_fail(p, line_no,
                             "%s:%ld: expected field at column 0 or with two-space indent",
                             p->filename, line_no);
        }

        rc = siml_parse_field(p, line + body_offset, line_no);
        return rc;
    }
}

int
siml_next(struct siml_iter *p, struct siml_event *ev)
{
    if (!p || !ev) {
        return 1;
    }

    siml_event_cleanup(ev);

    while (1) {
        if (p->pending_count > 0) {
            *ev = p->pending[0];
            if (p->pending_count > 1) {
                memmove(p->pending,
                        p->pending + 1,
                        (p->pending_count - 1) * sizeof(*p->pending));
            }
            p->pending_count--;
            return 0;
        }

        if (p->failed) {
            ev->type = SIML_EVENT_EOF;
            p->finished = 1;
            return 0;
        }

        if (p->finished) {
            ev->type = SIML_EVENT_EOF;
            return 0;
        }

        if (!fgets(p->line, sizeof(p->line), p->fp)) {
            p->line_no++;
            if (siml_finish_literal_if_needed(p, p->line_no) != 0) {
                continue;
            }
            if (siml_emit_pending_block_list_if_needed(p, p->line_no) != 0) {
                continue;
            }
            p->state = ST_OUTSIDE;
            p->finished = 1;
            continue;
        }

        p->line_no++;
        siml_trim_newline(p->line);

        if (siml_process_line(p, p->line) != 0) {
            continue;
        }
    }
}

#endif /* SIML_IMPLEMENTATION */

#endif /* SIML_H_INCLUDED */
