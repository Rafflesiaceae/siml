#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#define SIML_MAX_LINE 4096
#define SIML_MAX_KEY  64

enum siml_state {
    ST_OUTSIDE = 0,
    ST_ITEM,
    ST_BLOCK_LITERAL,
    ST_BLOCK_LIST
};

static int debug_enabled = 0;

static void
debugf(const char *fmt, ...)
{
    va_list ap;

    if (!debug_enabled) {
        return;
    }
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static void
trim_newline(char *s)
{
    size_t len = strlen(s);

    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

static void
rstrip_spaces(char *s)
{
    size_t len = strlen(s);

    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

static char *
lskip_spaces(char *s)
{
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

static int
is_blank(const char *s)
{
    const char *p = s;

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return *p == '\0';
}

static int
is_comment(const char *s)
{
    const char *p = s;

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return *p == '#';
}

static int
is_valid_key(const char *key)
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
parse_field(char *body,
            enum siml_state *state,
            int *last_was_block,
            char *block_list_key,
            size_t block_list_key_size,
            int *block_list_has_items,
            const char *filename,
            long line_no)
{
    char *colon;
    char *key;
    char *value;

    rstrip_spaces(body);

    colon = strchr(body, ':');
    if (!colon) {
        fprintf(stderr, "%s:%ld: missing ':' in field\n",
                filename, line_no);
        return 1;
    }

    *colon = '\0';
    key = body;
    rstrip_spaces(key);

    if (!is_valid_key(key)) {
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

    if (*last_was_block) {
        fprintf(stderr,
                "%s:%ld: fields after literal block are not allowed "
                "(key '%s')\n",
                filename, line_no, key);
        return 1;
    }

    /* detect block list (empty value, ignoring inline comment) */
    {
        int value_is_empty = 1;
        char last = ' ';
        char *scan = value;

        while (*scan) {
            if (*scan == '#' && (last == ' ' || last == '\t')) {
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

            if (key_len + 1 > block_list_key_size) {
                fprintf(stderr,
                        "%s:%ld: block list key too long '%s'\n",
                        filename, line_no, key);
                return 1;
            }
            strcpy(block_list_key, key);
            *block_list_has_items = 0;
            *state = ST_BLOCK_LIST;
            *last_was_block = 0;
            debugf("[dbg] %s:%ld: key '%s' starts block list\n",
                   filename, line_no, key);
            return 0;
        }
    }

    if (value[0] == '|') {
        /* literal block */
        debugf("[dbg] %s:%ld: key '%s' starts literal block\n",
               filename, line_no, key);
        printf("  %s = \"\"\"\n", key);
        *state = ST_BLOCK_LITERAL;
        *last_was_block = 1;
        return 0;
    }

    if (value[0] == '[') {
        /* list value */
        char *closing;
        char *p;
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
        p = closing + 1;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p != '\0' && *p != '#') {
            fprintf(stderr,
                    "%s:%ld: unexpected text after list for key '%s'\n",
                    filename, line_no, key);
            return 1;
        }
        /* terminate at ']' */
        closing[1] = '\0';
        rstrip_spaces(value);

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

        elem = lskip_spaces(inside);
        if (*elem == '\0') {
            printf("  %s[] = (empty)\n", key);
            debugf("[dbg] %s:%ld: key '%s' has empty list\n",
                   filename, line_no, key);
            *last_was_block = 0;
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

            rstrip_spaces(elem);
            if (*elem == '\0') {
                fprintf(stderr,
                        "%s:%ld: empty list element in key '%s'\n",
                        filename, line_no, key);
                return 1;
            }

            printf("  %s[] = %s\n", key, elem);

            elem = lskip_spaces(next);
            if (*elem == '\0') {
                break;
            }
        }

        *last_was_block = 0;
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
        rstrip_spaces(value);
        printf("  %s = %s\n", key, value);
        *last_was_block = 0;
        return 0;
    }
}

static int
parse_file(FILE *fp, const char *filename)
{
    char line[SIML_MAX_LINE];
    long line_no = 0;
    enum siml_state state = ST_OUTSIDE;
    int item_index = -1;
    int last_was_block = 0;
    int block_pending_blank = 0;
    char block_list_key[SIML_MAX_KEY];
    int block_list_has_items = 0;
    int singleton_mode = 0;
    int mode_determined = 0;

    while (fgets(line, sizeof(line), fp)) {
        char *p;

        line_no++;
        trim_newline(line);
        debugf("[dbg] %s:%ld: line='%s'\n",
               filename, line_no, line);

        if (state == ST_BLOCK_LITERAL) {
            if (line[0] == '-' && line[1] == ' ') {
                if (singleton_mode) {
                    fprintf(stderr,
                            "%s:%ld: unexpected item start in single-object "
                            "document\n",
                            filename, line_no);
                    return 1;
                }
                /* end block, handle this line as new item */
                printf("  \"\"\"\n");
                state = ST_ITEM;
                last_was_block = 0;
                block_pending_blank = 0;
                /* fall through: process as normal below */
            } else {
                int indent = 0;
                char *bp = line;

                while (*bp == ' ') {
                    indent++;
                    bp++;
                }
                if (indent == 0 && *bp == '#') {
                    /* comment at column 0 ends the literal block */
                    printf("  \"\"\"\n");
                    state = ST_OUTSIDE;
                    block_pending_blank = 0;
                    continue;
                }
                char *s = line;

                if (s[0] == ' ') {
                    s++;
                }
                if (is_blank(s)) {
                    block_pending_blank = 1;
                    continue;
                }
                if (block_pending_blank) {
                    printf("\n");
                    block_pending_blank = 0;
                }
                printf("%s\n", s);
                continue;
            }
        }

        if (state == ST_BLOCK_LIST) {
            char *bp = lskip_spaces(line);
            int indent = (int)(bp - line);

        if (is_blank(bp) || is_comment(bp)) {
            continue;
        }

        if (line[0] == '-' && line[1] == ' ') {
            if (singleton_mode) {
                fprintf(stderr,
                        "%s:%ld: unexpected item start in single-object "
                        "document\n",
                        filename, line_no);
                return 1;
            }
            /* new item starts, close empty list if needed */
            if (!block_list_has_items) {
                printf("  %s[] = (empty)\n", block_list_key);
            }
            state = ST_OUTSIDE;
                last_was_block = 0;
                /* fall through to normal item handling below */
            } else if (indent >= 2 && bp[0] == '-' && bp[1] == ' ') {
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
                rstrip_spaces(elem);
                if (*elem == '\0') {
                    fprintf(stderr,
                            "%s:%ld: empty block list element for key '%s'\n",
                            filename, line_no, block_list_key);
                    return 1;
                }
                printf("  %s[] = %s\n", block_list_key, elem);
                block_list_has_items = 1;
                continue;
            } else if (indent == 2) {
                int rc;
                char *body = line + 2;

                if (!block_list_has_items) {
                    printf("  %s[] = (empty)\n", block_list_key);
                }
                state = ST_ITEM;
                rc = parse_field(body, &state, &last_was_block,
                                 block_list_key,
                                 sizeof(block_list_key),
                                 &block_list_has_items,
                                 filename, line_no);
                if (rc != 0) {
                    return rc;
                }
                if (state == ST_BLOCK_LITERAL) {
                    block_pending_blank = 0;
                }
                continue;
            } else {
                fprintf(stderr,
                        "%s:%ld: expected block list item or new field/item\n",
                        filename, line_no);
                return 1;
            }
        }

        p = lskip_spaces(line);
        if (is_blank(p)) {
            continue;
        }
        if (is_comment(p)) {
            continue;
        }

        if (line[0] == '-' && line[1] == ' ') {
            int rc;
            char *body = line + 2;

            if (singleton_mode) {
                fprintf(stderr,
                        "%s:%ld: unexpected item start in single-object "
                        "document\n",
                        filename, line_no);
                return 1;
            }
            mode_determined = 1;
            item_index++;
            if (item_index > 0) {
                printf("\n");
            }
            printf("item %d\n", item_index);
            state = ST_ITEM;
            last_was_block = 0;

            rc = parse_field(body, &state, &last_was_block,
                             block_list_key, sizeof(block_list_key),
                             &block_list_has_items,
                             filename, line_no);
            if (rc != 0) {
                return rc;
            }
            if (state == ST_BLOCK_LITERAL) {
                block_pending_blank = 0;
            }
            continue;
        }

        if (state == ST_OUTSIDE) {
            int rc;

            if (!mode_determined) {
                /* single-object form */
                mode_determined = 1;
                singleton_mode = 1;
                item_index = 0;
                printf("item %d\n", item_index);
                state = ST_ITEM;
                last_was_block = 0;
                rc = parse_field(line, &state, &last_was_block,
                                 block_list_key, sizeof(block_list_key),
                                 &block_list_has_items,
                                 filename, line_no);
                if (rc != 0) {
                    return rc;
                }
                if (state == ST_BLOCK_LITERAL) {
                    block_pending_blank = 0;
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

            if (singleton_mode) {
                if (line[0] == ' ' && line[1] == ' ') {
                    body = line + 2;
                } else if (line[0] == ' ') {
                    fprintf(stderr,
                            "%s:%ld: expected field at column 0 or with "
                            "two-space indent\n",
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
                            "%s:%ld: expected 2-space-indented field or "
                            "new item\n",
                            filename, line_no);
                    return 1;
                }
            }

            rc = parse_field(body, &state, &last_was_block,
                             block_list_key, sizeof(block_list_key),
                             &block_list_has_items,
                             filename, line_no);
            if (rc != 0) {
                return rc;
            }
            if (state == ST_BLOCK_LITERAL) {
                block_pending_blank = 0;
            }
            continue;
        }
    }

    if (state == ST_BLOCK_LITERAL) {
        printf("  \"\"\"\n");
    }
    if (state == ST_BLOCK_LIST && !block_list_has_items) {
        printf("  %s[] = (empty)\n", block_list_key);
    }

    return 0;
}

int
main(int argc, char **argv)
{
    const char *filename;
    FILE *fp;
    int rc;

    if (getenv("DEBUG") != NULL) {
        debug_enabled = 1;
    }

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.siml>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-") == 0) {
        fp = stdin;
        filename = "<stdin>";
    } else {
        filename = argv[1];
        fp = fopen(filename, "r");
        if (!fp) {
            perror(filename);
            return 1;
        }
    }

    rc = parse_file(fp, filename);

    if (fp != stdin) {
        fclose(fp);
    }

    return rc;
}
