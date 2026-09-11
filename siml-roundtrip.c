#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIML_IMPLEMENTATION
#include "siml.h"

struct mem_reader {
    const char *data;
    size_t len;
    size_t pos;
};

struct buffer {
    char *data;
    size_t len;
    size_t cap;
};

static int siml_mem_read_line(void *userdata,
                              const char **out_line,
                              size_t *out_len) {
    struct mem_reader *r;
    size_t start;
    size_t i;
    int saw_lf;

    r = (struct mem_reader *)userdata;
    if (!r || !out_line || !out_len) {
        return -1;
    }
    if (r->pos >= r->len) {
        return 0;
    }

    start = r->pos;
    i = start;
    saw_lf = 0;
    while (i < r->len && r->data[i] != '\n') {
        i += 1;
    }

    if (i < r->len && r->data[i] == '\n') {
        saw_lf = 1;
    }
    *out_line = r->data + start;
    *out_len = i - start;
    r->pos = (i < r->len) ? (i + 1) : i;
    if (!saw_lf && r->pos >= r->len && *out_len > 0) {
        return 2;
    }
    return 1;
}

static int buf_reserve(struct buffer *b, size_t extra) {
    size_t needed;
    size_t new_cap;
    char *new_data;

    if (!b) return 0;
    needed = b->len + extra;
    if (needed <= b->cap) return 1;
    new_cap = b->cap ? b->cap : 256;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    new_data = (char *)realloc(b->data, new_cap);
    if (!new_data) return 0;
    b->data = new_data;
    b->cap = new_cap;
    return 1;
}

static int buf_append(struct buffer *b, const char *s, size_t len) {
    if (!buf_reserve(b, len)) return 0;
    memcpy(b->data + b->len, s, len);
    b->len += len;
    return 1;
}

static int buf_append_char(struct buffer *b, char c) {
    if (!buf_reserve(b, 1)) return 0;
    b->data[b->len++] = c;
    return 1;
}

static int buf_append_spaces(struct buffer *b, size_t count) {
    size_t i;
    if (!buf_reserve(b, count)) return 0;
    for (i = 0; i < count; ++i) {
        b->data[b->len++] = ' ';
    }
    return 1;
}

static int emit_inline_comment(struct buffer *b,
                               unsigned int spaces,
                               const siml_slice *comment) {
    if (!comment || comment->len == 0) return 1;
    if (!buf_append_spaces(b, (size_t)spaces)) return 0;
    if (!buf_append(b, "# ", 2)) return 0;
    if (!buf_append(b, comment->ptr, comment->len)) return 0;
    return 1;
}

static int emit_line_end(struct buffer *b) {
    return buf_append_char(b, '\n');
}

static int emit_prefix(struct buffer *b, size_t indent,
                       const char *key, size_t key_len,
                       int in_sequence, int has_inline_value) {
    if (!buf_append_spaces(b, indent)) return 0;
    if (in_sequence) {
        if (!buf_append(b, "-", 1)) return 0;
        if (has_inline_value) {
            if (!buf_append(b, " ", 1)) return 0;
        }
        return 1;
    }
    if (!buf_append(b, key, key_len)) return 0;
    if (has_inline_value) {
        if (!buf_append(b, ": ", 2)) return 0;
    } else {
        if (!buf_append(b, ":", 1)) return 0;
    }
    return 1;
}

static int build_flow_sequence(siml_parser *parser,
                               struct buffer *b,
                               siml_event *ev) {
    int first = 1;

    if (!buf_append_char(b, '[')) return 0;

    for (;;) {
        siml_event_type t = siml_next(parser, ev);
        if (t == SIML_EVENT_ERROR) {
            return 0;
        }
        if (t == SIML_EVENT_SEQUENCE_END) {
            if (!buf_append_char(b, ']')) return 0;
            break;
        }
        if (t == SIML_EVENT_SEQUENCE_START) {
            if (!first) {
                if (!buf_append_char(b, ',')) return 0;
            }
            first = 0;
            if (!build_flow_sequence(parser, b, ev)) return 0;
            continue;
        }
        if (t == SIML_EVENT_SCALAR) {
            if (!first) {
                if (!buf_append_char(b, ',')) return 0;
            }
            first = 0;
            if (!buf_append(b, ev->value.ptr, ev->value.len)) return 0;
            continue;
        }
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    const char *filename;
    FILE *fp;
    long file_size;
    size_t read_size;
    char *file_data;
    struct mem_reader reader;
    struct buffer out;
    siml_parser parser;
    siml_event ev;
    int rc;
    size_t stack_indent[SIML_MAX_NESTING];
    siml_container_type stack_type[SIML_MAX_NESTING];
    size_t depth;
    size_t cur_indent;
    int in_sequence;

    if (argc != 2) {
        (void)fprintf(stderr, "Usage: %s <file.siml>\n", argv[0]);
        return 1;
    }

    filename = argv[1];
    fp = fopen(filename, "rb");
    if (!fp) {
        perror(filename);
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        perror(filename);
        (void)fclose(fp);
        return 1;
    }
    file_size = ftell(fp);
    if (file_size < 0) {
        perror(filename);
        (void)fclose(fp);
        return 1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        perror(filename);
        (void)fclose(fp);
        return 1;
    }

    file_data = (char *)malloc((size_t)file_size);
    if (!file_data) {
        (void)fclose(fp);
        return 1;
    }
    read_size = fread(file_data, 1, (size_t)file_size, fp);
    (void)fclose(fp);
    if (read_size != (size_t)file_size) {
        free(file_data);
        return 1;
    }

    reader.data = file_data;
    reader.len = read_size;
    reader.pos = 0;

    out.data = NULL;
    out.len = 0;
    out.cap = 0;

    depth = 0;

    siml_parser_init(&parser, siml_mem_read_line, &reader);

    rc = 0;
    for (;;) {
        siml_event_type t = siml_next(&parser, &ev);
        if (t == SIML_EVENT_ERROR) {
            (void)fprintf(stderr, "SIML error at line %ld: %s\n",
                          ev.line,
                          ev.error_message ? ev.error_message : "parse error");
            rc = 1;
            break;
        }
        if (t == SIML_EVENT_STREAM_END) {
            break;
        }

        in_sequence = (depth > 0 && stack_type[depth - 1] == SIML_CONTAINER_SEQ);
        cur_indent = (depth > 0) ? stack_indent[depth - 1] : 0;

        switch (t) {
        case SIML_EVENT_STREAM_START:
            break;
        case SIML_EVENT_DOCUMENT_START:
            break;
        case SIML_EVENT_DOCUMENT_END:
            if (parser.awaiting_document) {
                if (!buf_append(&out, "---", 3) || !emit_line_end(&out)) {
                    rc = 1;
                }
            }
            break;
        case SIML_EVENT_COMMENT:
            if (!buf_append(&out, ev.value.ptr, ev.value.len) ||
                !emit_line_end(&out)) {
                rc = 1;
            }
            break;
        case SIML_EVENT_MAPPING_START:
            if (ev.key.len > 0 || in_sequence) {
                if (!emit_prefix(&out, cur_indent,
                                 ev.key.ptr, ev.key.len,
                                 in_sequence, 0) ||
                    !emit_line_end(&out)) {
                    rc = 1;
                }
            }
            if (depth >= SIML_MAX_NESTING) {
                rc = 1;
                break;
            }
            stack_type[depth] = SIML_CONTAINER_MAP;
            stack_indent[depth] = (depth == 0) ? 0 : (cur_indent + 2);
            depth += 1;
            break;
        case SIML_EVENT_SEQUENCE_START:
            if (ev.seq_style == SIML_SEQ_STYLE_FLOW) {
                struct buffer flow;
                siml_slice key;
                siml_slice comment;
                unsigned int comment_spaces;
                char key_buf[SIML_MAX_KEY_LEN + 1];
                char comment_buf[SIML_MAX_INLINE_COMMENT_TEXT_LEN + 1];

                key.len = ev.key.len;
                if (key.len > SIML_MAX_KEY_LEN) key.len = SIML_MAX_KEY_LEN;
                if (key.len > 0) {
                    memcpy(key_buf, ev.key.ptr, key.len);
                }
                key.ptr = key_buf;
                key_buf[key.len] = '\0';

                comment.len = ev.inline_comment.len;
                if (comment.len > SIML_MAX_INLINE_COMMENT_TEXT_LEN) {
                    comment.len = SIML_MAX_INLINE_COMMENT_TEXT_LEN;
                }
                if (comment.len > 0) {
                    memcpy(comment_buf, ev.inline_comment.ptr, comment.len);
                }
                comment.ptr = comment_buf;
                comment_buf[comment.len] = '\0';
                comment_spaces = ev.inline_comment_spaces;

                flow.data = NULL;
                flow.len = 0;
                flow.cap = 0;
                if (!build_flow_sequence(&parser, &flow, &ev)) {
                    free(flow.data);
                    rc = 1;
                    break;
                }
                in_sequence = (depth > 0 &&
                               stack_type[depth - 1] == SIML_CONTAINER_SEQ);
                cur_indent = (depth > 0) ? stack_indent[depth - 1] : 0;
                if (!emit_prefix(&out, cur_indent,
                                 key.ptr, key.len,
                                 in_sequence, 1) ||
                    !buf_append(&out, flow.data, flow.len) ||
                    !emit_inline_comment(&out, comment_spaces,
                                         &comment) ||
                    !emit_line_end(&out)) {
                    free(flow.data);
                    rc = 1;
                    break;
                }
                free(flow.data);
                break;
            }

            if (ev.key.len > 0 || in_sequence) {
                if (!emit_prefix(&out, cur_indent,
                                 ev.key.ptr, ev.key.len,
                                 in_sequence, 0) ||
                    !emit_line_end(&out)) {
                    rc = 1;
                }
            }
            if (depth >= SIML_MAX_NESTING) {
                rc = 1;
                break;
            }
            stack_type[depth] = SIML_CONTAINER_SEQ;
            stack_indent[depth] = (depth == 0) ? 0 : (cur_indent + 2);
            depth += 1;
            break;
        case SIML_EVENT_MAPPING_END:
        case SIML_EVENT_SEQUENCE_END:
            if (depth > 0) depth -= 1;
            break;
        case SIML_EVENT_SCALAR:
            if (!emit_prefix(&out, cur_indent,
                             ev.key.ptr, ev.key.len,
                             in_sequence, 1) ||
                !buf_append(&out, ev.value.ptr, ev.value.len) ||
                !emit_inline_comment(&out, ev.inline_comment_spaces,
                                     &ev.inline_comment) ||
                !emit_line_end(&out)) {
                rc = 1;
            }
            break;
        case SIML_EVENT_BLOCK_SCALAR_START:
            if (!emit_prefix(&out, cur_indent,
                             ev.key.ptr, ev.key.len,
                             in_sequence, 1) ||
                !buf_append(&out, "|", 1) ||
                !emit_inline_comment(&out, ev.inline_comment_spaces,
                                     &ev.inline_comment) ||
                !emit_line_end(&out)) {
                rc = 1;
            }
            break;
        case SIML_EVENT_BLOCK_SCALAR_LINE:
            if (ev.value.len == 0) {
                if (!emit_line_end(&out)) {
                    rc = 1;
                }
            } else {
                if (!buf_append_spaces(&out, cur_indent + 2) ||
                    !buf_append(&out, ev.value.ptr, ev.value.len) ||
                    !emit_line_end(&out)) {
                    rc = 1;
                }
            }
            break;
        case SIML_EVENT_BLOCK_SCALAR_END:
            break;
        case SIML_EVENT_NONE:
        case SIML_EVENT_ERROR:
        default:
            rc = 1;
            break;
        }

        if (rc != 0) break;
    }

    if (rc == 0) {
        if (read_size == 0) {
            if (out.len != 0) rc = 1;
        } else if (file_data[read_size - 1] != '\n' && out.len > 0 &&
                   out.data[out.len - 1] == '\n') {
            out.len -= 1;
        }
        if (out.len != read_size ||
            (out.len > 0 && memcmp(out.data, file_data, out.len) != 0)) {
            (void)fprintf(stderr, "roundtrip mismatch: %s\n", filename);
            rc = 1;
        }
    }

    free(out.data);
    free(file_data);
    return rc;
}
