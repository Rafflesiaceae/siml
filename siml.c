#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIML_IMPLEMENTATION
#include "siml.h"

int
main(int argc, char **argv)
{
    const char *filename;
    FILE *fp;
    struct siml_iter it;
    struct siml_event ev;
    int rc;

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

    if (siml_iter_init(&it, fp, filename) != 0) {
        fprintf(stderr, "Failed to initialize parser\n");
        rc = 1;
        goto done;
    }

    memset(&ev, 0, sizeof(ev));
    rc = 0;
    while (1) {
        if (siml_next(&it, &ev) != 0) {
            fprintf(stderr, "Failed to read next event\n");
            rc = 1;
            break;
        }
        if (ev.type == SIML_EVENT_EOF) {
            siml_event_cleanup(&ev);
            break;
        }
        if (ev.type == SIML_EVENT_ERROR) {
            fprintf(stderr, "%s\n",
                    ev.message ? ev.message : "parse error");
            siml_event_cleanup(&ev);
            rc = 1;
            break;
        }

        switch (ev.type) {
        case SIML_EVENT_BEGIN_ITEM:
            if (ev.item_index > 0) {
                printf("\n");
            }
            printf("item %d\n", ev.item_index);
            break;
        case SIML_EVENT_SCALAR:
            printf("  %s = '%s'\n", ev.key, ev.value);
            break;
        case SIML_EVENT_LIST_ELEMENT:
            printf("  %s[] = '%s'\n", ev.key, ev.value);
            break;
        case SIML_EVENT_LIST_EMPTY:
            printf("  %s[] = ''\n", ev.key);
            break;
        case SIML_EVENT_BEGIN_LITERAL:
            printf("  %s = '''\n", ev.key);
            break;
        case SIML_EVENT_LITERAL_LINE:
            printf("%s\n", ev.value ? ev.value : "");
            break;
        case SIML_EVENT_END_LITERAL:
            printf("'''\n");
            break;
        default:
            break;
        }
        siml_event_cleanup(&ev);
    }

    siml_iter_destroy(&it);

done:
    if (fp != stdin) {
        fclose(fp);
    }

    return rc;
}
