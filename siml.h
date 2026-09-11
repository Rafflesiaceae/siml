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

#ifndef SIML_MAX_VALUE
#define SIML_MAX_VALUE 4096
#endif

#ifndef SIML_MAX_ERROR
#define SIML_MAX_ERROR 256
#endif

#ifndef SIML_MAX_LIST
#define SIML_MAX_LIST 4096
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
    const char *key;
    const char *value;
    const char *message;
};

/*
 * The siml_iter object owns all storage for keys, values, and error messages
 * referenced by siml_event. Pointers in siml_event are only valid until the
 * next call to siml_next() with the same iterator, or until the iterator is
 * destroyed.
 */
struct siml_iter;

int siml_iter_init(struct siml_iter *it, FILE *fp, const char *filename);
void siml_iter_destroy(struct siml_iter *it);

/*
 * Advance the iterator by one event.
 *
 * On success, returns 0 and fills *ev. ev->type is SIML_EVENT_EOF when the
 * stream is exhausted. On a parse error, the next event will be
 * SIML_EVENT_ERROR with a human-readable message, followed by SIML_EVENT_EOF.
 *
 * On API misuse (NULL arguments), returns non-zero and does not modify *ev.
 */
int siml_next(struct siml_iter *it, struct siml_event *ev);

/* Provided for backwards compatibility; no dynamic memory is owned by ev. */
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
    ST_INLINE_LIST
};

struct siml_iter {
    FILE *fp;
    const char *filename;
    char line[SIML_MAX_LINE];
    long line_no;
    enum siml_state state;
    int item_index;

    /* pending item/field */
    int pending_begin_item;
    int pending_has_line;
    long pending_line_no;
    char pending_line[SIML_MAX_LINE];

    /* list state */
    char list_key[SIML_MAX_KEY];
    char list_buffer[SIML_MAX_LIST];
    size_t list_buffer_len;
    long list_start_line;
    const char *list_cursor;
    int list_emitting;

    /* literal block state */
    char literal_key[SIML_MAX_KEY];

    /* output buffers (owned by iterator) */
    char key_buf[SIML_MAX_KEY];
    char value_buf[SIML_MAX_VALUE];
    char err_buf[SIML_MAX_ERROR];

    /* error / eof state */
    int have_error;
    long error_line_no;
    int finished;
};

/* Backwards-compatible no-op: events own no dynamic memory. */
void
siml_event_cleanup(struct siml_event *ev)
{
    if (!ev) {
        return;
    }
    memset(ev, 0, sizeof(*ev));
}

static int
siml_debug_enabled(void)
{
    static int initialized = 0;
    static int enabled = 0;

    if (!initialized) {
        const char *env = getenv("DEBUG");
        enabled = (env && env[0] != '\0');
        initialized = 1;
    }
    return enabled;
}

static void
siml_debugf(const char *fmt, ...)
{
    va_list ap;

    if (!siml_debug_enabled()) {
        return;
    }

    fputs("[siml] ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void
siml_trim_newline(char *s)
{
    size_t len;

    if (!s) {
        return;
    }

    len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

static void
siml_rstrip_spaces(char *s)
{
    size_t len;

    if (!s) {
        return;
    }

    len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

static char *
siml_lskip_spaces(char *s)
{
    if (!s) {
        return s;
    }

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

static int
siml_is_blank(const char *s)
{
    const char *p = s;

    if (!p) {
        return 1;
    }

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return *p == '\0';
}

static int
siml_is_comment(const char *s)
{
    const char *p = s;

    if (!p) {
        return 0;
    }

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return *p == '#';
}

static int
siml_is_doc_separator(const char *line)
{
    const char *p;

    if (!line) {
        return 0;
    }

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

    if (!p || *p == '\0') {
        return 0;
    }
    if (!isalpha(*p) && *p != '_') {
        return 0;
    }
    p++;
    while (*p) {
        if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.') {
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

    if (!start || len == 0) {
        return 0;
    }
    if (!isalpha((unsigned char) start[0]) && start[0] != '_') {
        return 0;
    }
    for (i = 1; i < len; i++) {
        unsigned char c = (unsigned char) start[i];

        if (!isalnum(c) && c != '_' && c != '-' && c != '.') {
            return 0;
        }
    }
    return 1;
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

    if (indent != 0) {
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
    while (key_end > body &&
           (key_end[-1] == ' ' || key_end[-1] == '\t')) {
        key_end--;
    }
    if (key_end == body) {
        return 0;
    }

    if (!siml_is_valid_key_span(body, (size_t) (key_end - body))) {
        return 0;
    }

    if (body_offset) {
        *body_offset = indent;
    }
    return 1;
}

static int
siml_fail(struct siml_iter *p, long line_no, const char *fmt, ...)
{
    va_list ap;

    if (!p) {
        return 1;
    }

    p->have_error = 1;
    p->error_line_no = line_no;

    if (fmt && p->err_buf[0] == '\0') {
        va_start(ap, fmt);
        (void) vsnprintf(p->err_buf, sizeof(p->err_buf), fmt, ap);
        va_end(ap);
    }

    siml_debugf("%s", p->err_buf[0] ? p->err_buf : "siml_fail");

    return 0;
}

static void
siml_reset_list_state(struct siml_iter *p)
{
    p->list_buffer_len = 0;
    p->list_buffer[0] = '\0';
    p->list_start_line = 0;
    p->list_cursor = NULL;
    p->list_emitting = 0;
    p->list_key[0] = '\0';
}

static int
siml_list_append(struct siml_iter *p, const char *text)
{
    size_t len;

    if (!text) {
        return 0;
    }

    len = strlen(text);
    if (len == 0) {
        return 0;
    }

    if (p->list_buffer_len + len >= SIML_MAX_LIST) {
        return siml_fail(p, p->list_start_line ? p->list_start_line : p->line_no,
                         "%s:%ld: list for key '%s' too long",
                         p->filename,
                         p->list_start_line ? p->list_start_line : p->line_no,
                         p->list_key[0] ? p->list_key : "<list>");
    }

    memcpy(p->list_buffer + p->list_buffer_len, text, len);
    p->list_buffer_len += len;
    p->list_buffer[p->list_buffer_len] = '\0';
    return 0;
}

static int
siml_init_list_from_string(struct siml_iter *p,
                           const char *key,
                           const char *inside,
                           long line_no)
{
    size_t key_len;
    size_t len;

    if (!p || !key || !inside) {
        return 1;
    }

    key_len = strlen(key);
    if (key_len >= sizeof(p->list_key)) {
        return siml_fail(p, line_no,
                         "%s:%ld: list key too long '%s'",
                         p->filename, line_no, key);
    }
    memcpy(p->list_key, key, key_len + 1);

    len = strlen(inside);
    if (len >= SIML_MAX_LIST) {
        return siml_fail(p, line_no,
                         "%s:%ld: list for key '%s' too long",
                         p->filename, line_no, key);
    }
    memcpy(p->list_buffer, inside, len + 1);
    p->list_buffer_len = len;
    p->list_start_line = line_no;
    p->list_cursor = NULL;
    p->list_emitting = 1;

    siml_debugf("%s:%ld: begin list key='%s'", p->filename, line_no, key);

    return 0;
}

static int
siml_emit_next_list_element(struct siml_iter *p, struct siml_event *ev)
{
    char *elem;

    if (!p || !ev) {
        return 1;
    }

    if (!p->list_emitting) {
        return 1;
    }

    if (!p->list_cursor) {
        p->list_cursor = siml_lskip_spaces(p->list_buffer);
        if (*(p->list_cursor) == '\0') {
            /* empty list */
            ev->type = SIML_EVENT_LIST_EMPTY;
            ev->item_index = p->item_index;
            ev->line_no = p->list_start_line;
            ev->key = p->list_key;
            ev->value = NULL;
            ev->message = NULL;

            siml_debugf("%s:%ld: emit empty list for key='%s'",
                        p->filename,
                        p->list_start_line,
                        p->list_key);

            p->list_emitting = 0;
            p->list_cursor = NULL;
            siml_reset_list_state(p);
            return 0;
        }
    }

    elem = (char *) p->list_cursor;
    if (*elem == '\0') {
        p->list_emitting = 0;
        p->list_cursor = NULL;
        siml_reset_list_state(p);
        return 1;
    }

    {
        char *comma = strchr(elem, ',');
        char *next;

        if (comma) {
            *comma = '\0';
            next = comma + 1;
        } else {
            next = elem + strlen(elem);
        }

        siml_rstrip_spaces(elem);
        if (*elem == '\0') {
            /* empty list element */
            siml_fail(p,
                      p->list_start_line,
                      "%s:%ld: empty list element in key '%s'",
                      p->filename,
                      p->list_start_line,
                      p->list_key[0] ? p->list_key : "<list>");
            p->list_emitting = 0;
            p->list_cursor = NULL;
            siml_reset_list_state(p);
            return -1;
        }

        if (strlen(elem) >= sizeof(p->value_buf)) {
            siml_fail(p,
                      p->list_start_line,
                      "%s:%ld: list element too long in key '%s'",
                      p->filename,
                      p->list_start_line,
                      p->list_key);
            p->list_emitting = 0;
            p->list_cursor = NULL;
            siml_reset_list_state(p);
            return -1;
        }

        memcpy(p->value_buf, elem, strlen(elem) + 1);

        ev->type = SIML_EVENT_LIST_ELEMENT;
        ev->item_index = p->item_index;
        ev->line_no = p->list_start_line;
        ev->key = p->list_key;
        ev->value = p->value_buf;
        ev->message = NULL;

        siml_debugf("%s:%ld: emit list element key='%s' value='%s'",
                    p->filename,
                    p->list_start_line,
                    p->list_key,
                    p->value_buf);

        p->list_cursor = siml_lskip_spaces(next);
        if (*p->list_cursor == '\0') {
            p->list_emitting = 0;
            p->list_cursor = NULL;
            siml_reset_list_state(p);
        }

        return 0;
    }
}

static int
siml_handle_field_line(struct siml_iter *p,
                       char *body,
                       long line_no,
                       struct siml_event *ev)
{
    char *colon;
    char *key;
    char *value;
    size_t key_len;

    siml_rstrip_spaces(body);

    colon = strchr(body, ':');
    if (!colon) {
        return siml_fail(p, line_no,
                         "%s:%ld: missing ':' in field",
                         p->filename, line_no);
    }

    *colon = '\0';
    key = body;
    siml_rstrip_spaces(key);

    if (!siml_is_valid_key(key)) {
        return siml_fail(p, line_no,
                         "%s:%ld: invalid key '%s'",
                         p->filename, line_no, key);
    }

    key_len = strlen(key);
    if (key_len >= sizeof(p->key_buf)) {
        return siml_fail(p, line_no,
                         "%s:%ld: key too long '%s'",
                         p->filename, line_no, key);
    }
    memcpy(p->key_buf, key, key_len + 1);

    value = colon + 1;
    if (*value == ' ') {
        value++;
    } else if (*value != '\0') {
        return siml_fail(p, line_no,
                         "%s:%ld: expected space after ':' in field '%s'",
                         p->filename, line_no, key);
    }

    /* literal block */
    if (value[0] == '|') {
        size_t lit_len = strlen(key);

        if (lit_len >= sizeof(p->literal_key)) {
            return siml_fail(p, line_no,
                             "%s:%ld: literal block key too long '%s'",
                             p->filename, line_no, key);
        }
        memcpy(p->literal_key, key, lit_len + 1);
        p->state = ST_BLOCK_LITERAL;

        ev->type = SIML_EVENT_BEGIN_LITERAL;
        ev->item_index = p->item_index;
        ev->line_no = line_no;
        ev->key = p->literal_key;
        ev->value = NULL;
        ev->message = NULL;

        siml_debugf("%s:%ld: begin literal key='%s'",
                    p->filename, line_no, p->literal_key);

        return 0;
    }

    /* list value */
    if (value[0] == '[') {
        char *closing;
        char *p_char;

        closing = strrchr(value, ']');
        if (closing) {
            p_char = closing + 1;
            while (*p_char == ' ' || *p_char == '\t') {
                p_char++;
            }
            if (*p_char != '\0' && *p_char != '#') {
                return siml_fail(p, line_no,
                                 "%s:%ld: unexpected text after list for key '%s'",
                                 p->filename, line_no, key);
            }
            *closing = '\0';
            siml_rstrip_spaces(value);

            if (siml_init_list_from_string(p,
                                           key,
                                           value + 1,
                                           line_no) != 0) {
                return -1;
            }

            /* emit first list event immediately */
            if (siml_emit_next_list_element(p, ev) == 0) {
                return 0;
            }
            return -1;
        } else {
            /* multi-line list */
            siml_reset_list_state(p);
            p->list_start_line = line_no;

            if (siml_list_append(p, value + 1) != 0) {
                return -1;
            }

            p->state = ST_INLINE_LIST;

            siml_debugf("%s:%ld: begin multiline list key='%s'",
                        p->filename, line_no, key);

            return 0;
        }
    }

    /* scalar */
    {
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

        if (strlen(value) >= sizeof(p->value_buf)) {
            return siml_fail(p, line_no,
                             "%s:%ld: scalar value too long for key '%s'",
                             p->filename, line_no, key);
        }
        memcpy(p->value_buf, value, strlen(value) + 1);

        ev->type = SIML_EVENT_SCALAR;
        ev->item_index = p->item_index;
        ev->line_no = line_no;
        ev->key = p->key_buf;
        ev->value = p->value_buf;
        ev->message = NULL;

        siml_debugf("%s:%ld: scalar key='%s' value='%s'",
                    p->filename, line_no, p->key_buf, p->value_buf);

        return 0;
    }
}

static int
siml_handle_toplevel_line(struct siml_iter *p,
                          char *line,
                          long line_no,
                          struct siml_event *ev)
{
    char *p_line = siml_lskip_spaces(line);
    int indent = (int) (p_line - line);
    int body_offset = 0;

    if (siml_is_blank(p_line) || siml_is_comment(p_line)) {
        return 0;
    }

    if (siml_is_doc_separator(line)) {
        siml_debugf("%s:%ld: doc separator", p->filename, line_no);
        p->state = ST_OUTSIDE;
        return 0;
    }

    if (!siml_is_field_line(p, line, indent, &body_offset)) {
        if (p->state == ST_OUTSIDE) {
            return siml_fail(p, line_no,
                             "%s:%ld: expected field at column 0 to start document (use '---' between documents)",
                             p->filename, line_no);
        } else {
            return siml_fail(p, line_no,
                             "%s:%ld: expected field at column 0",
                             p->filename, line_no);
        }
    }

    if (p->state == ST_OUTSIDE) {
        size_t len;

        p->item_index++;
        p->state = ST_ITEM;
        p->pending_begin_item = 1;

        len = strlen(line + body_offset);
        if (len >= sizeof(p->pending_line)) {
            return siml_fail(p, line_no,
                             "%s:%ld: field line too long",
                             p->filename, line_no);
        }
        memcpy(p->pending_line, line + body_offset, len + 1);
        p->pending_line_no = line_no;
        p->pending_has_line = 1;

        siml_debugf("%s:%ld: begin item %d",
                    p->filename, line_no, p->item_index);

        return 0;
    }

    /* ST_ITEM: parse field immediately */
    return siml_handle_field_line(p, line + body_offset, line_no, ev);
}

static int
siml_handle_literal_line(struct siml_iter *p,
                         char *line,
                         long line_no,
                         struct siml_event *ev)
{
    char *bp = line;
    int indent = 0;
    char *s;

    if (siml_is_doc_separator(line)) {
        p->state = ST_OUTSIDE;

        ev->type = SIML_EVENT_END_LITERAL;
        ev->item_index = p->item_index;
        ev->line_no = line_no;
        ev->key = p->literal_key;
        ev->value = NULL;
        ev->message = NULL;

        siml_debugf("%s:%ld: end literal (doc separator) key='%s'",
                    p->filename, line_no, p->literal_key);

        return 0;
    }

    while (*bp == ' ') {
        indent++;
        bp++;
    }

    if (indent == 0 && *bp == '#') {
        /* top-level comment ends literal; comment itself is ignored */
        p->state = ST_ITEM;

        ev->type = SIML_EVENT_END_LITERAL;
        ev->item_index = p->item_index;
        ev->line_no = line_no;
        ev->key = p->literal_key;
        ev->value = NULL;
        ev->message = NULL;

        siml_debugf("%s:%ld: end literal (comment) key='%s'",
                    p->filename, line_no, p->literal_key);

        return 0;
    }

    {
        int body_offset = 0;

        if (siml_is_field_line(p, line, indent, &body_offset)) {
            size_t len;

            /* new field starts; close literal and re-process field later */
            len = strlen(line + body_offset);
            if (len >= sizeof(p->pending_line)) {
                return siml_fail(p, line_no,
                                 "%s:%ld: field line too long",
                                 p->filename, line_no);
            }
            memcpy(p->pending_line, line + body_offset, len + 1);
            p->pending_line_no = line_no;
            p->pending_has_line = 1;

            p->state = ST_ITEM;

            ev->type = SIML_EVENT_END_LITERAL;
            ev->item_index = p->item_index;
            ev->line_no = line_no;
            ev->key = p->literal_key;
            ev->value = NULL;
            ev->message = NULL;

            siml_debugf("%s:%ld: end literal (next field) key='%s'",
                        p->filename, line_no, p->literal_key);

            return 0;
        }
    }

    s = line;
    while (*s == ' ') {
        s++;
    }
    if (siml_is_blank(s)) {
        /* blank line inside literal block */
        p->value_buf[0] = '\0';

        ev->type = SIML_EVENT_LITERAL_LINE;
        ev->item_index = p->item_index;
        ev->line_no = line_no;
        ev->key = NULL;
        ev->value = p->value_buf;
        ev->message = NULL;

        return 0;
    }

    if (line[0] != ' ' || line[1] != ' ') {
        return siml_fail(p, line_no,
                         "%s:%ld: literal block line must start with two spaces for key '%s'",
                         p->filename, line_no, p->literal_key);
    }

    {
        const char *text = line + 2;
        size_t len = strlen(text);

        if (len >= sizeof(p->value_buf)) {
            return siml_fail(p, line_no,
                             "%s:%ld: literal line too long in key '%s'",
                             p->filename, line_no, p->literal_key);
        }
        memcpy(p->value_buf, text, len + 1);
    }

    ev->type = SIML_EVENT_LITERAL_LINE;
    ev->item_index = p->item_index;
    ev->line_no = line_no;
    ev->key = NULL;
    ev->value = p->value_buf;
    ev->message = NULL;

    return 0;
}

static int
siml_handle_inline_list_line(struct siml_iter *p,
                             char *line,
                             long line_no,
                             struct siml_event *ev)
{
    char *p_line_in_list = siml_lskip_spaces(line);
    int indent = (int) (p_line_in_list - line);
    char *closing;
    char *after;

    (void) ev;

    if (siml_is_doc_separator(line) ||
        siml_is_field_line(p, line, indent, NULL)) {
        siml_fail(p, line_no,
                  "%s:%ld: list for key '%s' missing ']' (started at line %ld)",
                  p->filename,
                  line_no,
                  p->list_key[0] ? p->list_key : "<list>",
                  p->list_start_line ? p->list_start_line : line_no);
        siml_reset_list_state(p);
        p->state = ST_ITEM;
        return -1;
    }

    closing = strrchr(line, ']');

    if (!closing) {
        if (siml_is_blank(p_line_in_list) || siml_is_comment(p_line_in_list)) {
            return 0;
        }
        if (siml_list_append(p, line) != 0) {
            p->state = ST_ITEM;
            return -1;
        }
        return 0;
    }

    after = closing + 1;
    while (*after == ' ' || *after == '\t') {
        after++;
    }
    if (*after != '\0' && *after != '#') {
        siml_fail(p, line_no,
                  "%s:%ld: unexpected text after list for key '%s'",
                  p->filename, line_no, p->list_key);
        siml_reset_list_state(p);
        p->state = ST_ITEM;
        return -1;
    }

    *closing = '\0';
    if (siml_list_append(p, line) != 0) {
        p->state = ST_ITEM;
        return -1;
    }

    /* all list text collected */
    if (siml_init_list_from_string(p,
                                   p->list_key,
                                   p->list_buffer,
                                   p->list_start_line ? p->list_start_line : line_no) != 0) {
        siml_reset_list_state(p);
        p->state = ST_ITEM;
        return -1;
    }

    p->state = ST_ITEM;

    /* emit first element now */
    if (siml_emit_next_list_element(p, ev) == 0) {
        return 0;
    }

    return -1;
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
    p->line_no = 0;

    siml_reset_list_state(p);

    return 0;
}

void
siml_iter_destroy(struct siml_iter *p)
{
    (void) p;
}

int
siml_next(struct siml_iter *p, struct siml_event *ev)
{
    if (!p || !ev) {
        return 1;
    }

    memset(ev, 0, sizeof(*ev));
    ev->type = SIML_EVENT_NONE;
    ev->item_index = p->item_index;
    ev->line_no = p->line_no;
    ev->key = NULL;
    ev->value = NULL;
    ev->message = NULL;

    for (;;) {
        if (p->have_error) {
            ev->type = SIML_EVENT_ERROR;
            ev->item_index = p->item_index;
            ev->line_no = p->error_line_no;
            ev->key = NULL;
            ev->value = NULL;
            ev->message = p->err_buf[0] ? p->err_buf : "parse error";

            siml_debugf("%s:%ld: emit ERROR '%s'",
                        p->filename,
                        p->error_line_no,
                        ev->message);

            p->have_error = 0;
            p->finished = 1;
            return 0;
        }

        if (p->pending_begin_item) {
            ev->type = SIML_EVENT_BEGIN_ITEM;
            ev->item_index = p->item_index;
            ev->line_no = p->pending_line_no;
            ev->key = NULL;
            ev->value = NULL;
            ev->message = NULL;

            siml_debugf("%s:%ld: emit BEGIN_ITEM %d",
                        p->filename,
                        p->pending_line_no,
                        p->item_index);

            p->pending_begin_item = 0;
            return 0;
        }

        if (p->list_emitting) {
            int lrc = siml_emit_next_list_element(p, ev);

            if (lrc == 0) {
                return 0;
            }
            /* error case already recorded via siml_fail */
            continue;
        }

        if (p->finished) {
            ev->type = SIML_EVENT_EOF;
            ev->item_index = p->item_index;
            ev->line_no = p->line_no;
            ev->key = NULL;
            ev->value = NULL;
            ev->message = NULL;
            return 0;
        }

        {
            char *line;
            long line_no;

            if (p->pending_has_line) {
                line = p->pending_line;
                line_no = p->pending_line_no;
                p->pending_has_line = 0;
            } else {
                if (!fgets(p->line, sizeof(p->line), p->fp)) {
                    p->line_no++;

                    if (p->state == ST_BLOCK_LITERAL) {
                        /* EOF closes literal block */
                        p->state = ST_OUTSIDE;

                        ev->type = SIML_EVENT_END_LITERAL;
                        ev->item_index = p->item_index;
                        ev->line_no = p->line_no;
                        ev->key = p->literal_key;
                        ev->value = NULL;
                        ev->message = NULL;

                        siml_debugf("%s:%ld: end literal (EOF) key='%s'",
                                    p->filename,
                                    p->line_no,
                                    p->literal_key);

                        return 0;
                    }

                    if (p->state == ST_INLINE_LIST) {
                        siml_fail(p, p->line_no,
                                  "%s:%ld: list for key '%s' missing ']' (started at line %ld)",
                                  p->filename,
                                  p->line_no,
                                  p->list_key[0] ? p->list_key : "<list>",
                                  p->list_start_line ? p->list_start_line : p->line_no);
                        siml_reset_list_state(p);
                        p->state = ST_ITEM;
                        continue;
                    }

                    p->finished = 1;
                    continue;
                }

                p->line_no++;
                line_no = p->line_no;
                siml_trim_newline(p->line);
                line = p->line;
            }

            if (p->state == ST_BLOCK_LITERAL) {
                if (siml_handle_literal_line(p, line, line_no, ev) == 0 &&
                    ev->type != SIML_EVENT_NONE) {
                    return 0;
                }
                continue;
            }

            if (p->state == ST_INLINE_LIST) {
                if (siml_handle_inline_list_line(p, line, line_no, ev) == 0 &&
                    ev->type != SIML_EVENT_NONE) {
                    return 0;
                }
                continue;
            }

            /* ST_OUTSIDE or ST_ITEM */
            if (siml_handle_toplevel_line(p, line, line_no, ev) == 0 &&
                ev->type != SIML_EVENT_NONE) {
                return 0;
            }
        }
    }
}

#endif /* SIML_IMPLEMENTATION */

#endif /* SIML_H_INCLUDED */
