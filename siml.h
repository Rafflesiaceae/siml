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

struct siml_callbacks {
    void (*begin_item)(void *user_data, int index);
    void (*scalar)(void *user_data,
                   const char *key,
                   const char *value);
    void (*list_element)(void *user_data,
                         const char *key,
                         const char *element);
    void (*list_empty)(void *user_data,
                       const char *key);
    void (*begin_literal)(void *user_data,
                          const char *key);
    void (*literal_line)(void *user_data,
                         const char *line);
    void (*end_literal)(void *user_data,
                        const char *key);
};

int siml_parse_file(FILE *fp,
                    const char *filename,
                    const struct siml_callbacks *cb,
                    void *user_data,
                    int debug_enabled);

#ifdef __cplusplus
}
#endif

#ifdef SIML_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

enum siml_state {
    ST_OUTSIDE = 0,
    ST_ITEM,
    ST_BLOCK_LITERAL,
    ST_BLOCK_LIST
};

struct siml_parser {
    enum siml_state state;
    int item_index;
    int block_pending_blank;
    char block_list_key[SIML_MAX_KEY];
    int block_list_has_items;
    int block_list_pending_scalar;
    int singleton_mode;
    int mode_determined;
    char literal_key[SIML_MAX_KEY];
    int debug_enabled;
    const struct siml_callbacks *cb;
    void *user_data;
};

static void
siml_debugf(int enabled, const char *fmt, ...)
{
    va_list ap;

    if (!enabled) {
        return;
    }
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
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
siml_emit_begin_item(struct siml_parser *p, int index)
{
    if (p->cb && p->cb->begin_item) {
        p->cb->begin_item(p->user_data, index);
    }
}

static void
siml_emit_scalar(struct siml_parser *p,
                 const char *key,
                 const char *value)
{
    if (p->cb && p->cb->scalar) {
        p->cb->scalar(p->user_data, key, value);
    }
}

static void
siml_emit_list_element(struct siml_parser *p,
                       const char *key,
                       const char *element)
{
    if (p->cb && p->cb->list_element) {
        p->cb->list_element(p->user_data, key, element);
    }
}

static void
siml_emit_list_empty(struct siml_parser *p,
                     const char *key)
{
    if (p->cb && p->cb->list_empty) {
        p->cb->list_empty(p->user_data, key);
    }
}

static void
siml_emit_begin_literal(struct siml_parser *p,
                        const char *key)
{
    if (p->cb && p->cb->begin_literal) {
        p->cb->begin_literal(p->user_data, key);
    }
}

static void
siml_emit_literal_line(struct siml_parser *p,
                       const char *line)
{
    if (p->cb && p->cb->literal_line) {
        p->cb->literal_line(p->user_data, line);
    }
}

static void
siml_emit_end_literal(struct siml_parser *p,
                      const char *key)
{
    if (p->cb && p->cb->end_literal) {
        p->cb->end_literal(p->user_data, key);
    }
}

static int
siml_is_field_line(struct siml_parser *p,
                   const char *line,
                   int indent,
                   int *body_offset)
{
    const char *body;
    const char *colon;
    const char *key_end;

    if (p->singleton_mode) {
        if (!(indent == 0 || indent == 2)) {
            return 0;
        }
    } else {
        if (indent != 2) {
            return 0;
        }
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
siml_parse_field(struct siml_parser *p,
                 char *body,
                 const char *filename,
                 long line_no)
{
    char *colon;
    char *key;
    char *value;

    siml_rstrip_spaces(body);

    colon = strchr(body, ':');
    if (!colon) {
        fprintf(stderr, "%s:%ld: missing ':' in field\n",
                filename, line_no);
        return 1;
    }

    *colon = '\0';
    key = body;
    siml_rstrip_spaces(key);

    if (!siml_is_valid_key(key)) {
        fprintf(stderr, "%s:%ld: invalid key '%s'\n",
                filename, line_no, key);
        return 1;
    }

    value = colon + 1;
    if (*value == ' ') {
        value++;
    } else if (*value != '\0') {
        fprintf(stderr,
                "%s:%ld: expected space after ':' in field '%s'\n",
                filename, line_no, key);
        return 1;
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
                fprintf(stderr,
                        "%s:%ld: block list key too long '%s'\n",
                        filename, line_no, key);
                return 1;
            }
            strcpy(p->block_list_key, key);
            p->block_list_has_items = 0;
            p->block_list_pending_scalar = 1;
            p->state = ST_BLOCK_LIST;
            siml_debugf(p->debug_enabled,
                        "[dbg] %s:%ld: key '%s' starts block list\n",
                        filename, line_no, key);
            return 0;
        }
    }

    if (value[0] == '|') {
        size_t key_len = strlen(key);

        if (key_len + 1 > sizeof(p->literal_key)) {
            fprintf(stderr,
                    "%s:%ld: literal block key too long '%s'\n",
                    filename, line_no, key);
            return 1;
        }
        strcpy(p->literal_key, key);
        siml_debugf(p->debug_enabled,
                    "[dbg] %s:%ld: key '%s' starts literal block\n",
                    filename, line_no, key);
        siml_emit_begin_literal(p, key);
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
            fprintf(stderr,
                    "%s:%ld: list for key '%s' missing ']'\n",
                    filename, line_no, key);
            return 1;
        }
        p_char = closing + 1;
        while (*p_char == ' ' || *p_char == '\t') {
            p_char++;
        }
        if (*p_char != '\0' && *p_char != '#') {
            fprintf(stderr,
                    "%s:%ld: unexpected text after list for key '%s'\n",
                    filename, line_no, key);
            return 1;
        }
        /* terminate at ']' */
        closing[1] = '\0';
        siml_rstrip_spaces(value);

        /* now parse inside [ ... ] */
        inside = value + 1;
        end = strchr(inside, ']');
        if (!end) {
            fprintf(stderr,
                    "%s:%ld: internal error parsing list '%s'\n",
                    filename, line_no, key);
            return 1;
        }
        *end = '\0';

        elem = siml_lskip_spaces(inside);
        if (*elem == '\0') {
            siml_emit_list_empty(p, key);
            siml_debugf(p->debug_enabled,
                        "[dbg] %s:%ld: key '%s' has empty list\n",
                        filename, line_no, key);
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
                fprintf(stderr,
                        "%s:%ld: empty list element in key '%s'\n",
                        filename, line_no, key);
                return 1;
            }

            siml_emit_list_element(p, key, elem);

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
        siml_emit_scalar(p, key, value);
        return 0;
    }
}

int
siml_parse_file(FILE *fp,
                const char *filename,
                const struct siml_callbacks *cb,
                void *user_data,
                int debug_enabled)
{
    char line[SIML_MAX_LINE];
    long line_no = 0;
    struct siml_parser parser;
    char *p_line;

    parser.state = ST_OUTSIDE;
    parser.item_index = -1;
    parser.block_pending_blank = 0;
    parser.block_list_key[0] = '\0';
    parser.block_list_has_items = 0;
    parser.block_list_pending_scalar = 0;
    parser.singleton_mode = 0;
    parser.mode_determined = 0;
    parser.literal_key[0] = '\0';
    parser.debug_enabled = debug_enabled;
    parser.cb = cb;
    parser.user_data = user_data;

    while (fgets(line, sizeof(line), fp)) {
        line_no++;
        siml_trim_newline(line);
        siml_debugf(parser.debug_enabled,
                    "[dbg] %s:%ld: line='%s'\n",
                    filename, line_no, line);

        if (parser.state == ST_BLOCK_LITERAL) {
            if (line[0] == '-' && line[1] == ' ') {
                if (parser.singleton_mode) {
                    fprintf(stderr,
                            "%s:%ld: unexpected item start in "
                            "single-object document\n",
                            filename, line_no);
                    return 1;
                }
                /* end block, handle this line as new item */
                siml_emit_end_literal(&parser, parser.literal_key);
                parser.state = ST_ITEM;
                parser.block_pending_blank = 0;
                /* fall through: process as normal below */
            } else {
                int indent = 0;
                char *bp = line;
                int body_offset = 0;
                char *s;

                while (*bp == ' ') {
                    indent++;
                    bp++;
                }
                if (indent == 0 && *bp == '#') {
                    /* comment at column 0 ends the literal block */
                    siml_emit_end_literal(&parser,
                                          parser.literal_key);
                    parser.state = ST_OUTSIDE;
                    parser.block_pending_blank = 0;
                    continue;
                }
                if (siml_is_field_line(&parser, line, indent, &body_offset)) {
                    int rc;

                    siml_emit_end_literal(&parser, parser.literal_key);
                    parser.state = ST_ITEM;
                    parser.block_pending_blank = 0;
                    rc = siml_parse_field(&parser,
                                          line + body_offset,
                                          filename, line_no);
                    if (rc != 0) {
                        return rc;
                    }
                    if (parser.state == ST_BLOCK_LITERAL) {
                        parser.block_pending_blank = 0;
                    }
                    continue;
                }
                s = line;
                if (s[0] == ' ') {
                    s++;
                }
                if (siml_is_blank(s)) {
                    parser.block_pending_blank = 1;
                    continue;
                }
                if (parser.block_pending_blank) {
                    siml_emit_literal_line(&parser, "");
                    parser.block_pending_blank = 0;
                }
                siml_emit_literal_line(&parser, s);
                continue;
            }
        }

        if (parser.state == ST_BLOCK_LIST) {
            char *bp = siml_lskip_spaces(line);
            int indent = (int) (bp - line);

            if (siml_is_blank(bp) || siml_is_comment(bp)) {
                continue;
            }

            if (line[0] == '-' && line[1] == ' ') {
                if (parser.singleton_mode) {
                    fprintf(stderr,
                            "%s:%ld: unexpected item start in "
                            "single-object document\n",
                            filename, line_no);
                    return 1;
                }
                /* new item starts, close empty list if needed */
                if (!parser.block_list_has_items) {
                    if (parser.block_list_pending_scalar) {
                        siml_emit_scalar(&parser,
                                         parser.block_list_key,
                                         "");
                    } else {
                        siml_emit_list_empty(&parser,
                                             parser.block_list_key);
                    }
                }
                parser.block_list_pending_scalar = 0;
                parser.state = ST_OUTSIDE;
                /* fall through to normal item handling below */
            } else if (indent >= 2 &&
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
                    fprintf(stderr,
                            "%s:%ld: empty block list element for "
                            "key '%s'\n",
                            filename, line_no,
                            parser.block_list_key);
                    return 1;
                }
                siml_emit_list_element(&parser,
                                       parser.block_list_key,
                                       elem);
                parser.block_list_has_items = 1;
                parser.block_list_pending_scalar = 0;
                continue;
            } else if (indent == 2) {
                int rc;
                char *body = line + 2;

                if (!parser.block_list_has_items) {
                    if (parser.block_list_pending_scalar) {
                        siml_emit_scalar(&parser,
                                         parser.block_list_key,
                                         "");
                        parser.block_list_pending_scalar = 0;
                        parser.state = ST_ITEM;
                        rc = siml_parse_field(&parser, body,
                                              filename, line_no);
                        if (rc != 0) {
                            return rc;
                        }
                        if (parser.state == ST_BLOCK_LITERAL) {
                            parser.block_pending_blank = 0;
                        }
                        continue;
                    }
                    siml_emit_list_empty(&parser,
                                         parser.block_list_key);
                }
                parser.block_list_pending_scalar = 0;
                parser.state = ST_ITEM;
                rc = siml_parse_field(&parser, body,
                                      filename, line_no);
                if (rc != 0) {
                    return rc;
                }
                if (parser.state == ST_BLOCK_LITERAL) {
                    parser.block_pending_blank = 0;
                }
                continue;
            } else {
                fprintf(stderr,
                        "%s:%ld: expected block list item or new "
                        "field/item\n",
                        filename, line_no);
                return 1;
            }
        }

        p_line = siml_lskip_spaces(line);
        if (siml_is_blank(p_line)) {
            continue;
        }
        if (siml_is_comment(p_line)) {
            continue;
        }

        if (line[0] == '-' && line[1] == ' ') {
            int rc;
            char *body = line + 2;

            if (parser.singleton_mode) {
                fprintf(stderr,
                        "%s:%ld: unexpected item start in "
                        "single-object document\n",
                        filename, line_no);
                return 1;
            }
            parser.mode_determined = 1;
            parser.item_index++;
            siml_emit_begin_item(&parser, parser.item_index);
            parser.state = ST_ITEM;

            rc = siml_parse_field(&parser, body,
                                  filename, line_no);
            if (rc != 0) {
                return rc;
            }
            if (parser.state == ST_BLOCK_LITERAL) {
                parser.block_pending_blank = 0;
            }
            continue;
        }

        if (parser.state == ST_OUTSIDE) {
            int rc;

            if (!parser.mode_determined) {
                /* single-object form */
                parser.mode_determined = 1;
                parser.singleton_mode = 1;
                parser.item_index = 0;
                siml_emit_begin_item(&parser, parser.item_index);
                parser.state = ST_ITEM;
                rc = siml_parse_field(&parser, line,
                                      filename, line_no);
                if (rc != 0) {
                    return rc;
                }
                if (parser.state == ST_BLOCK_LITERAL) {
                    parser.block_pending_blank = 0;
                }
                continue;
            }

            fprintf(stderr,
                    "%s:%ld: expected item start ('- key: value')\n",
                    filename, line_no);
            return 1;
        }

        /* ST_ITEM: expect fields */
        {
            int rc;
            char *body = NULL;

            if (parser.singleton_mode) {
                if (line[0] == ' ' && line[1] == ' ') {
                    body = line + 2;
                } else if (line[0] == ' ') {
                    fprintf(stderr,
                            "%s:%ld: expected field at column 0 or "
                            "with two-space indent\n",
                            filename, line_no);
                    return 1;
                } else {
                    body = line;
                }
            } else {
                if (line[0] == ' ' && line[1] == ' ') {
                    body = line + 2;
                } else {
                    fprintf(stderr,
                            "%s:%ld: expected 2-space-indented field "
                            "or new item\n",
                            filename, line_no);
                    return 1;
                }
            }

            rc = siml_parse_field(&parser, body,
                                  filename, line_no);
            if (rc != 0) {
                return rc;
            }
            if (parser.state == ST_BLOCK_LITERAL) {
                parser.block_pending_blank = 0;
            }
            continue;
        }
    }

    if (parser.state == ST_BLOCK_LITERAL) {
        siml_emit_end_literal(&parser, parser.literal_key);
    }
    if (parser.state == ST_BLOCK_LIST &&
        !parser.block_list_has_items) {
        if (parser.block_list_pending_scalar) {
            siml_emit_scalar(&parser,
                             parser.block_list_key,
                             "");
        } else {
            siml_emit_list_empty(&parser,
                                 parser.block_list_key);
        }
    }

    return 0;
}

#endif /* SIML_IMPLEMENTATION */

#endif /* SIML_H_INCLUDED */
