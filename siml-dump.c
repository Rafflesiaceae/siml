#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIML_IMPLEMENTATION
#include "siml.h"

struct file_reader {
    FILE   *fp;
    char   *buf;
    size_t  cap;
    size_t  lines_read;
    long    fail_after;
};

static int siml_file_read_line(void *userdata,
                               const char **out_line,
                               size_t *out_len) {
    struct file_reader *r;
    size_t len;
    int ch;
    int saw_lf;
    size_t new_cap;
    char *new_buf;

    r = (struct file_reader *)userdata;
    if (!r || !out_line || !out_len) {
        return -1;
    }
    if (!r->fp) {
        return -1;
    }
    if (r->fail_after >= 0 && r->lines_read >= (size_t)r->fail_after) {
        return -1;
    }

    if (r->cap == 0) {
        r->cap = 256;
        r->buf = (char *)malloc(r->cap);
        if (!r->buf) {
            return -1;
        }
    }

    len = 0;
    saw_lf = 0;
    while ((ch = fgetc(r->fp)) != EOF) {
        if (ch == '\n') {
            saw_lf = 1;
            break;
        }
        if (len + 1 >= r->cap) {
            new_cap = r->cap * 2;
            new_buf = (char *)realloc(r->buf, new_cap);
            if (!new_buf) {
                return -1;
            }
            r->buf = new_buf;
            r->cap = new_cap;
        }
        r->buf[len++] = (char)ch;
    }

    if (ferror(r->fp)) {
        return -1;
    }
    if (ch == EOF && len == 0) {
        return 0;
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
    r->lines_read += 1;
    return 1;
}

static void print_slice(const siml_slice *s) {
    if (s && s->ptr && s->len > 0) {
        (void)fwrite(s->ptr, 1, s->len, stdout);
    }
}

static void print_inline_comment(const siml_event *ev) {
    if (ev->inline_comment.ptr && ev->inline_comment.len > 0) {
        (void)printf("  # (spaces=%u) ", ev->inline_comment_spaces);
        print_slice(&ev->inline_comment);
    }
}

int main(int argc, char **argv) {
    const char *filename;
    FILE *fp;
    siml_parser parser;
    siml_event ev;
    struct file_reader reader;
    int rc;

    if (argc != 2) {
        (void)fprintf(stderr, "Usage: %s <file.siml>\n", argv[0]);
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

    reader.fp  = fp;
    reader.buf = NULL;
    reader.cap = 0;
    reader.lines_read = 0;
    reader.fail_after = -1;
    {
        const char *env = getenv("SIML_TEST_READ_ERROR_AFTER");
        if (env && env[0] != '\0') {
            reader.fail_after = strtol(env, NULL, 10);
            if (reader.fail_after < 0) {
                reader.fail_after = -1;
            }
        }
    }

    siml_parser_init(&parser, siml_file_read_line, &reader);

    rc = 0;
    while (1) {
        siml_event_type t;
        t = siml_next(&parser, &ev);
        if (t == SIML_EVENT_ERROR) {
            (void)fprintf(stderr, "SIML error at line %ld: %s\n",
                          ev.line,
                          ev.error_message ? ev.error_message : "parse error");
            rc = 1;
            break;
        }
        if (t == SIML_EVENT_STREAM_END) {
            (void)printf("STREAM_END\n");
            break;
        }

        switch (t) {
        case SIML_EVENT_STREAM_START:
            (void)printf("STREAM_START\n");
            break;
        case SIML_EVENT_DOCUMENT_START:
            (void)printf("DOCUMENT_START\n");
            break;
        case SIML_EVENT_DOCUMENT_END:
            (void)printf("DOCUMENT_END\n");
            break;
        case SIML_EVENT_MAPPING_START:
            (void)printf("MAPPING_START");
            if (ev.key.len > 0) {
                (void)printf(" key=");
                print_slice(&ev.key);
            }
            (void)printf("\n");
            break;
        case SIML_EVENT_MAPPING_END:
            (void)printf("MAPPING_END\n");
            break;
        case SIML_EVENT_SEQUENCE_START:
            (void)printf("SEQUENCE_START");
            if (ev.seq_style == SIML_SEQ_STYLE_FLOW) {
                (void)printf(" style=flow");
            } else {
                (void)printf(" style=block");
            }
            if (ev.key.len > 0) {
                (void)printf(" key=");
                print_slice(&ev.key);
            }
            print_inline_comment(&ev);
            (void)printf("\n");
            break;
        case SIML_EVENT_SEQUENCE_END:
            (void)printf("SEQUENCE_END\n");
            break;
        case SIML_EVENT_SCALAR:
            (void)printf("SCALAR");
            if (ev.key.len > 0) {
                (void)printf(" key=");
                print_slice(&ev.key);
            }
            (void)printf(" value='");
            print_slice(&ev.value);
            (void)printf("'");
            print_inline_comment(&ev);
            (void)printf("\n");
            break;
        case SIML_EVENT_BLOCK_SCALAR_START:
            (void)printf("BLOCK_SCALAR_START");
            if (ev.key.len > 0) {
                (void)printf(" key=");
                print_slice(&ev.key);
            }
            print_inline_comment(&ev);
            (void)printf("\n");
            break;
        case SIML_EVENT_BLOCK_SCALAR_LINE:
            (void)printf("BLOCK_SCALAR_LINE '");
            print_slice(&ev.value);
            (void)printf("'\n");
            break;
        case SIML_EVENT_BLOCK_SCALAR_END:
            (void)printf("BLOCK_SCALAR_END\n");
            break;
        case SIML_EVENT_COMMENT:
            (void)printf("COMMENT ");
            print_slice(&ev.value);
            (void)printf("\n");
            break;
        default:
            break;
        }
    }

    free(reader.buf);
    if (fp != stdin) {
        fclose(fp);
    }

    return rc;
}
