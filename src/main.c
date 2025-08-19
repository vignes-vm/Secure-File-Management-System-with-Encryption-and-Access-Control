/* src/main.c
   Step 4: Add "unlock" CLI alongside "lock" and keep logging.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/securefs.h"

/* parser for optional --key HEX (1-byte, as in Step 3) */
static int parse_hex_key(int argc, char **argv, unsigned char *out_key, unsigned long *out_klen) {
    for (int i = 0; i < argc - 1; ++i) {
        if (strcmp(argv[i], "--key") == 0) {
            const char *hex = argv[i+1];
            unsigned int byte = 0;
            if (sscanf(hex, "%2x", &byte) == 1) {
                out_key[0] = (unsigned char)byte;
                *out_klen = 1;
                return 0;
            }
            return -1;
        }
    }
    *out_klen = 0; /* default key 0xAA if not provided */
    return 0;
}

static void print_usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s                                 # startup message\n", prog);
    printf("  %s lock   <path> [--key HEX]       # lock file with XOR + chmod 000\n", prog);
    printf("  %s unlock <path> [--key HEX]       # unlock (decrypt) + chmod 644\n", prog);
}

int main(int argc, char **argv) {
    printf("%s v%s — SecureFS initialized\n", PROJECT_NAME, PROJECT_VERSION);
    if (log_event("Program started") != 0) {
        printf("WARN: logging failed\n");
    } else {
        printf("Log entry appended to logs/securefs.log\n");
    }

    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    /* LOCK command */
    if (strcmp(argv[1], "lock") == 0) {
        if (argc < 3) {
            fprintf(stderr, "ERROR: 'lock' requires a <path>\n");
            print_usage(argv[0]);
            return 2;
        }
        const char *path = argv[2];
        unsigned char key[1] = {0};
        unsigned long klen = 0;
        if (parse_hex_key(argc, argv, key, &klen) != 0) {
            fprintf(stderr, "ERROR: invalid --key (use one byte hex like 'AA' or '7F')\n");
            return 2;
        }
        int rc = do_lock_file(path, key, klen);
        if (rc == 0) {
            printf("LOCK: success -> %s\n", path);
            return 0;
        } else {
            printf("LOCK: failed -> %s (see logs/securefs.log)\n", path);
            return 1;
        }
    }

    /* UNLOCK command */
    if (strcmp(argv[1], "unlock") == 0) {
        if (argc < 3) {
            fprintf(stderr, "ERROR: 'unlock' requires a <path>\n");
            print_usage(argv[0]);
            return 2;
        }
        const char *path = argv[2];
        unsigned char key[1] = {0};
        unsigned long klen = 0;
        if (parse_hex_key(argc, argv, key, &klen) != 0) {
            fprintf(stderr, "ERROR: invalid --key (use one byte hex like 'AA' or '7F')\n");
            return 2;
        }
        int rc = do_unlock_file(path, key, klen);
        if (rc == 0) {
            printf("UNLOCK: success -> %s\n", path);
            return 0;
        } else {
            printf("UNLOCK: failed -> %s (see logs/securefs.log)\n", path);
            return 1;
        }
    }

    /* Unknown subcommand */
    print_usage(argv[0]);
    return 0;
}
