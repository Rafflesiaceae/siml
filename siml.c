#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIML_IMPLEMENTATION
#include "siml.h"

static void
dump_begin_item(void *user_data, int index)
{
    (void) user_data;

    if (index > 0) {
        printf("\n");
    }
    printf("item %d\n", index);
}

static void
dump_scalar(void *user_data,
            const char *key,
            const char *value)
{
    (void) user_data;

    printf("  %s = '%s'\n", key, value);
}

static void
dump_list_element(void *user_data,
                  const char *key,
                  const char *element)
{
    (void) user_data;

    printf("  %s[] = '%s'\n", key, element);
}

static void
dump_list_empty(void *user_data,
                const char *key)
{
    (void) user_data;

    printf("  %s[] = ''\n", key);
}

static void
dump_begin_literal(void *user_data,
                   const char *key)
{
    (void) user_data;

    printf("  %s = '''\n", key);
}

static void
dump_literal_line(void *user_data,
                  const char *line)
{
    (void) user_data;

    printf("%s\n", line);
}

static void
dump_end_literal(void *user_data,
                 const char *key)
{
    (void) user_data;
    (void) key;

    printf("  '''\n");
}

int
main(int argc, char **argv)
{
    const char *filename;
    FILE *fp;
    struct siml_callbacks cb;
    int debug_enabled = 0;
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

    cb.begin_item = dump_begin_item;
    cb.scalar = dump_scalar;
    cb.list_element = dump_list_element;
    cb.list_empty = dump_list_empty;
    cb.begin_literal = dump_begin_literal;
    cb.literal_line = dump_literal_line;
    cb.end_literal = dump_end_literal;

    rc = siml_parse_file(fp, filename, &cb, NULL, debug_enabled);

    if (fp != stdin) {
        fclose(fp);
    }

    return rc;
}
