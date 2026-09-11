#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIML_IMPLEMENTATION
#include "siml.h"

/* Simple utilities ------------------------------------------------------- */

struct string_list {
    char  **items;
    size_t  count;
    size_t  cap;
};

struct cvar_entry {
    char             *id;
    char             *default_value;
    int               has_min;
    double            min_value;
    int               has_max;
    double            max_value;
    struct string_list flags;
    struct string_list description_lines;
    long              start_line;
};

struct file_reader {
    FILE   *fp;
    char   *buf;
    size_t  cap;
};

static void
free_string_list(struct string_list *list)
{
    size_t i;
    if (!list || !list->items) return;
    for (i = 0; i < list->count; ++i) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap   = 0;
}

static int
string_list_append_owned(struct string_list *list, char *text)
{
    char **new_items;
    size_t new_cap;

    if (!list || !text) return 0;
    if (list->count + 1 > list->cap) {
        new_cap = (list->cap == 0) ? 4 : (list->cap * 2);
        new_items = (char **)realloc(list->items, new_cap * sizeof(char *));
        if (!new_items) {
            return 0;
        }
        list->items = new_items;
        list->cap   = new_cap;
    }
    list->items[list->count++] = text;
    return 1;
}

static char *
dup_slice(const siml_slice *s)
{
    char *p;
    if (!s || !s->ptr) return NULL;
    p = (char *)malloc(s->len + 1);
    if (!p) return NULL;
    if (s->len > 0) {
        memcpy(p, s->ptr, s->len);
    }
    p[s->len] = '\0';
    return p;
}

static int
slice_equals(const siml_slice *s, const char *text)
{
    size_t len;
    if (!s || !text) return 0;
    len = strlen(text);
    return (s->len == len && strncmp(s->ptr, text, len) == 0);
}

static void
clear_entry(struct cvar_entry *e)
{
    if (!e) return;
    free(e->id);
    free(e->default_value);
    free_string_list(&e->flags);
    free_string_list(&e->description_lines);
    e->id = NULL;
    e->default_value = NULL;
    e->has_min = 0;
    e->min_value = 0.0;
    e->has_max = 0;
    e->max_value = 0.0;
    e->start_line = 0;
}

/* Input handling --------------------------------------------------------- */

static int
read_line_from_file(void *userdata, const char **out_line, size_t *out_len)
{
    struct file_reader *r;
    int ch;
    size_t len;
    size_t new_cap;
    char *new_buf;
    int saw_lf;

    r = (struct file_reader *)userdata;
    if (!r || !out_line || !out_len || !r->fp) {
        return -1;
    }

    if (r->cap == 0) {
        r->cap = 256;
        r->buf = (char *)malloc(r->cap);
        if (!r->buf) return -1;
    }

    len = 0;
    ch = EOF;
    saw_lf = 0;
    while ((ch = fgetc(r->fp)) != EOF) {
        if (ch == '\n') {
            saw_lf = 1;
            break;
        }
        if (len + 1 >= r->cap) {
            new_cap = r->cap * 2;
            new_buf = (char *)realloc(r->buf, new_cap);
            if (!new_buf) return -1;
            r->buf = new_buf;
            r->cap = new_cap;
        }
        r->buf[len++] = (char)ch;
    }

    if (ferror(r->fp)) {
        return -1;
    }
    if (ch == EOF && len == 0) {
        return 0; /* EOF */
    }
    if (ch == EOF && len > 0 && !saw_lf) {
        r->buf[len] = '\0';
        *out_line = r->buf;
        *out_len  = len;
        return 2;
    }

    r->buf[len] = '\0';
    *out_line = r->buf;
    *out_len  = len;
    return 1;
}

/* Formatting helpers ---------------------------------------------------- */

static void
emit_escaped(FILE *out, const char *text)
{
    size_t i;
    int c;
    if (!out || !text) return;
    for (i = 0; text[i] != '\0'; ++i) {
        c = (unsigned char)text[i];
        switch (c) {
        case '\\':
            fputs("\\\\", out);
            break;
        case '"':
            fputs("\\\"", out);
            break;
        case '\t':
            fputs("\\t", out);
            break;
        case '\n':
            fputs("\\n", out);
            break;
        default:
            if (c < 32 || c > 126) {
                fprintf(out, "\\x%02X", (unsigned int)c);
            } else {
                fputc(c, out);
            }
            break;
        }
    }
}

static int
format_float(double value, char *out, size_t out_size)
{
    int n;
    if (!out || out_size == 0) return 0;
    n = sprintf(out, "%.9gf", (float)value);
    return (n > 0 && (size_t)n < out_size);
}

static void
emit_description(FILE *out, const struct string_list *desc)
{
    size_t i;
    if (!out || !desc) return;
    if (desc->count == 0) {
        fprintf(out, "\t\"\"\n");
        return;
    }
    for (i = 0; i < desc->count; ++i) {
        fprintf(out, "\t\"");
        emit_escaped(out, desc->items[i]);
        fprintf(out, "\\n\"\n");
    }
}

static int
parse_double_field(const siml_slice *s, double *out_value)
{
    char *tmp;
    char *endp;
    if (!s || !out_value) return 0;
    tmp = dup_slice(s);
    if (!tmp) return 0;
    *out_value = strtod(tmp, &endp);
    if (endp == tmp || *endp != '\0') {
        free(tmp);
        return 0;
    }
    free(tmp);
    return 1;
}

/* Stanza handling ------------------------------------------------------- */

static char *
read_entire_file(const char *path, size_t *out_len)
{
    FILE *fp;
    long size;
    size_t nread;
    char *buf;

    if (out_len) *out_len = 0;
    if (!path) return NULL;

    fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buf = (char *)malloc((size_t)size + 2);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    nread = fread(buf, 1, (size_t)size, fp);
    if (ferror(fp)) {
        fclose(fp);
        free(buf);
        return NULL;
    }
    fclose(fp);
    buf[nread] = '\0';
    if (out_len) *out_len = nread;
    return buf;
}

/* Output ---------------------------------------------------------------- */

static void
emit_entry(FILE *out, const struct cvar_entry *e)
{
    char min_buf[64];
    char max_buf[64];
    size_t i;

    if (!format_float(e->min_value, min_buf, sizeof(min_buf))) return;
    if (!format_float(e->max_value, max_buf, sizeof(max_buf))) return;

    fprintf(out, "static const cvar_t %s = {\n", e->id);

    fprintf(out, "\t\"");
    emit_escaped(out, e->id);
    fprintf(out, "\",\n");

    fprintf(out, "\t\"");
    emit_escaped(out, e->default_value);
    fprintf(out, "\",\n");

    fprintf(out, "\t%s,\n", min_buf);
    fprintf(out, "\t%s,\n", max_buf);

    fprintf(out, "\t");
    if (e->flags.count == 0) {
        fprintf(out, "NULL,\n");
    } else {
        for (i = 0; i < e->flags.count; ++i) {
            if (i > 0) fprintf(out, " | ");
            fputs(e->flags.items[i], out);
        }
        fprintf(out, ",\n");
    }

    emit_description(out, &e->description_lines);
    fprintf(out, "};\n\n");
}

/* Path handling --------------------------------------------------------- */

static int
make_output_path(const char *input, char *out, size_t out_size)
{
    size_t len;
    size_t i;
    if (!input || !out || out_size == 0) return 0;
    len = strlen(input);
    if (len >= 5 && strcmp(input + len - 5, ".siml") == 0) {
        if (len - 5 + 2 >= out_size) return 0;
        for (i = 0; i < len - 5; ++i) out[i] = input[i];
        out[len - 5] = '.';
        out[len - 4] = 'c';
        out[len - 3] = '\0';
    } else {
        if (len + 3 >= out_size) return 0;
        strcpy(out, input);
        strcat(out, ".c");
    }
    return 1;
}

/* Parsing --------------------------------------------------------------- */

static int
process_file(const char *path, const char *stanza)
{
    struct file_reader reader;
    siml_parser parser;
    siml_event ev;
    FILE *in;
    FILE *out;
    char out_path[4096];
    struct cvar_entry *entries;
    size_t entry_count;
    size_t entry_cap;
    enum { LIST_NONE = 0, LIST_FLAGS } list_state;
    int in_block;
    struct cvar_entry current;
    int rc;

    in = fopen(path, "r");
    if (!in) {
        fprintf(stderr, "codegen: cannot open %s: %s\n", path, strerror(errno));
        return 1;
    }

    if (!make_output_path(path, out_path, sizeof(out_path))) {
        fprintf(stderr, "codegen: output path too long for %s\n", path);
        fclose(in);
        return 1;
    }

    reader.fp = in;
    reader.buf = NULL;
    reader.cap = 0;

    siml_parser_init(&parser, read_line_from_file, &reader);

    entries = NULL;
    entry_count = 0;
    entry_cap = 0;
    list_state = LIST_NONE;
    in_block = 0;
    memset(&current, 0, sizeof(current));

    rc = 0;
    while (1) {
        siml_event_type t;
        t = siml_next(&parser, &ev);
        if (t == SIML_EVENT_ERROR) {
            fprintf(stderr, "%s:%ld: %s\n", path,
                    ev.line, ev.error_message ? ev.error_message : "parse error");
            rc = 1;
            break;
        }
        if (t == SIML_EVENT_STREAM_END) {
            break;
        }

        switch (t) {
        case SIML_EVENT_ITEM_START:
            clear_entry(&current);
            memset(&current, 0, sizeof(current));
            current.start_line = ev.line;
            list_state = LIST_NONE;
            in_block = 0;
            break;

        case SIML_EVENT_FIELD_SCALAR:
            if (slice_equals(&ev.key, "id")) {
                if (current.id) {
                    fprintf(stderr, "%s:%ld: duplicate id\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
                current.id = dup_slice(&ev.value);
                if (!current.id) {
                    fprintf(stderr, "%s:%ld: out of memory\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
            } else if (slice_equals(&ev.key, "default")) {
                if (current.default_value) {
                    fprintf(stderr, "%s:%ld: duplicate default\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
                current.default_value = dup_slice(&ev.value);
                if (!current.default_value) {
                    fprintf(stderr, "%s:%ld: out of memory\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
            } else if (slice_equals(&ev.key, "min")) {
                if (current.has_min) {
                    fprintf(stderr, "%s:%ld: duplicate min\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
                if (!parse_double_field(&ev.value, &current.min_value)) {
                    fprintf(stderr, "%s:%ld: invalid min value\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
                current.has_min = 1;
            } else if (slice_equals(&ev.key, "max")) {
                if (current.has_max) {
                    fprintf(stderr, "%s:%ld: duplicate max\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
                if (!parse_double_field(&ev.value, &current.max_value)) {
                    fprintf(stderr, "%s:%ld: invalid max value\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
                current.has_max = 1;
            } else {
                fprintf(stderr, "%s:%ld: unknown scalar field '", path, ev.line);
                fwrite(ev.key.ptr, 1, ev.key.len, stderr);
                fprintf(stderr, "'\n");
                rc = 1;
                goto parse_done;
            }
            break;

        case SIML_EVENT_FIELD_LIST_BEGIN:
            if (list_state != LIST_NONE) {
                fprintf(stderr, "%s:%ld: nested lists are not allowed\n", path, ev.line);
                rc = 1;
                goto parse_done;
            }
            if (slice_equals(&ev.key, "flags")) {
                list_state = LIST_FLAGS;
            } else {
                fprintf(stderr, "%s:%ld: unknown list field '", path, ev.line);
                fwrite(ev.key.ptr, 1, ev.key.len, stderr);
                fprintf(stderr, "'\n");
                rc = 1;
                goto parse_done;
            }
            break;

        case SIML_EVENT_FIELD_LIST_ITEM:
            if (list_state != LIST_FLAGS) {
                fprintf(stderr, "%s:%ld: stray list item\n", path, ev.line);
                rc = 1;
                goto parse_done;
            } else {
                char *flag = dup_slice(&ev.value);
                if (!flag) {
                    fprintf(stderr, "%s:%ld: out of memory\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
                if (!string_list_append_owned(&current.flags, flag)) {
                    free(flag);
                    fprintf(stderr, "%s:%ld: out of memory\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
            }
            break;

        case SIML_EVENT_FIELD_LIST_END:
            list_state = LIST_NONE;
            break;

        case SIML_EVENT_FIELD_BLOCK_BEGIN:
            if (in_block) {
                fprintf(stderr, "%s:%ld: nested block\n", path, ev.line);
                rc = 1;
                goto parse_done;
            }
            if (!slice_equals(&ev.key, "description")) {
                fprintf(stderr, "%s:%ld: unknown block field '", path, ev.line);
                fwrite(ev.key.ptr, 1, ev.key.len, stderr);
                fprintf(stderr, "'\n");
                rc = 1;
                goto parse_done;
            }
            in_block = 1;
            break;

        case SIML_EVENT_FIELD_BLOCK_LINE:
            if (!in_block) {
                fprintf(stderr, "%s:%ld: stray block line\n", path, ev.line);
                rc = 1;
                goto parse_done;
            } else {
                char *line = dup_slice(&ev.value);
                if (!line) {
                    fprintf(stderr, "%s:%ld: out of memory\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
                if (!string_list_append_owned(&current.description_lines, line)) {
                    free(line);
                    fprintf(stderr, "%s:%ld: out of memory\n", path, ev.line);
                    rc = 1;
                    goto parse_done;
                }
            }
            break;

        case SIML_EVENT_FIELD_BLOCK_END:
            in_block = 0;
            break;

        case SIML_EVENT_ITEM_END:
            if (!current.id || !current.default_value || !current.has_min || !current.has_max) {
                fprintf(stderr, "%s:%ld: missing required fields (need id, default, min, max)\n",
                        path, current.start_line);
                rc = 1;
                goto parse_done;
            }
            if (entry_count + 1 > entry_cap) {
                size_t new_cap = (entry_cap == 0) ? 4 : (entry_cap * 2);
                struct cvar_entry *new_entries =
                    (struct cvar_entry *)realloc(entries, new_cap * sizeof(struct cvar_entry));
                if (!new_entries) {
                    fprintf(stderr, "%s:%ld: out of memory\n", path, current.start_line);
                    rc = 1;
                    goto parse_done;
                }
                entries = new_entries;
                entry_cap = new_cap;
            }
            entries[entry_count] = current;
            entry_count += 1;
            memset(&current, 0, sizeof(current));
            break;

        default:
            break;
        }
    }

parse_done:
    free(reader.buf);
    fclose(in);

    if (rc == 0) {
        out = fopen(out_path, "w");
        if (!out) {
            fprintf(stderr, "codegen: cannot open %s for write: %s\n",
                    out_path, strerror(errno));
            rc = 1;
        } else {
            size_t i;
            if (stanza && stanza[0] != '\0') {
                fputs(stanza, out);
                if (stanza[strlen(stanza) - 1] != '\n') {
                    fputc('\n', out);
                }
                fputc('\n', out);
            }
            for (i = 0; i < entry_count; ++i) {
                emit_entry(out, &entries[i]);
            }
            fclose(out);
        }
    }

    {
        size_t i;
        for (i = 0; i < entry_count; ++i) {
            clear_entry(&entries[i]);
        }
        free(entries);
    }
    clear_entry(&current);

    return rc;
}

/* Main ------------------------------------------------------------------ */

int
main(int argc, char **argv)
{
    int i;
    const char *stanza_path = NULL;
    char *stanza_content = NULL;
    size_t stanza_len = 0;
    int have_input = 0;
    int rc = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s [--stanza=FILE] <file.siml> [file2.siml ...]\n",
                argv[0]);
        return 1;
    }

    for (i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--stanza=", 9) == 0) {
            stanza_path = argv[i] + 9;
        }
    }

    if (stanza_path) {
        stanza_content = read_entire_file(stanza_path, &stanza_len);
        if (!stanza_content) {
            fprintf(stderr, "codegen: failed to read stanza file %s\n", stanza_path);
            rc = 1;
            goto done;
        }
    }

    for (i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--stanza=", 9) == 0) {
            continue;
        }
        have_input = 1;
        if (process_file(argv[i], stanza_content) != 0) {
            rc = 1;
            break;
        }
    }

    if (!have_input) {
        fprintf(stderr, "codegen: no input files provided\n");
        rc = 1;
    }

done:
    free(stanza_content);
    return rc;
}
