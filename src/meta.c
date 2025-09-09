#define XOPEN_SOURCE 700
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "securefs.h"

/* Database file path relative to project root */
static const char *DB_PATH = "data/locked_files.db";

/* Helper: ensure data/ exists */
static int ensure_data_dir(void) {
    struct stat st;
    if (stat("data", &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        return -1;
    }
    if (mkdir("data", 0700) != 0) return -1;
    return 0;
}

/* Check if path exists in DB (returns 1 if found, 0 if not, -1 on error) */
int meta_is_locked(const char *path) {
    if (!path) return -1;
    if (ensure_data_dir() != 0) return -1;

    int fd = open(DB_PATH, O_RDONLY | O_CREAT, 0600);
    if (fd < 0) return -1;

    FILE *fp = fdopen(fd, "r");
    if (!fp) { close(fd); return -1; }

    char *line = NULL;
    size_t lena = 0;
    ssize_t n;
    int found = 0;
    while ((n = getline(&line, &lena, fp)) != -1) {
        /* strip newline */
        if (n > 0 && line[n-1] == '\\n') line[n-1] = '\\0';
        if (strcmp(line, path) == 0) { found = 1; break; }
    }
    free(line);
    fclose(fp);
    return found;
}

/* Add path to DB if not present (returns 0 on success) */
int meta_add_locked(const char *path) {
    if (!path) return -1;
    if (ensure_data_dir() != 0) {
        log_event("META add failed: ensure_data_dir errno=%d", errno);
        return -1;
    }

    int already = meta_is_locked(path);
    if (already == 1) return 0; /* already present, nothing to do */
    if (already < 0) {
        /* error checking file; continue and attempt append anyway */
    }

    /* Append line using system calls */
    int fd = open(DB_PATH, O_CREAT | O_APPEND | O_WRONLY, 0600);
    if (fd < 0) {
        log_event("META add failed: open('%s') errno=%d", DB_PATH, errno);
        return -1;
    }

    size_t len = strlen(path);
    /* write path + newline */
    ssize_t nw = write(fd, path, len);
    if (nw != (ssize_t)len) {
        close(fd);
        log_event("META add failed: write path errno=%d", errno);
        return -1;
    }
    nw = write(fd, "\\n", 1);
    if (nw != 1) {
        close(fd);
        log_event("META add failed: write newline errno=%d", errno);
        return -1;
    }
    /* flush to disk */
    fsync(fd);
    close(fd);
    log_event("META add: %s", path);
    return 0;
}

/* Remove path by rewriting DB without that line (returns 0 on success) */
int meta_remove_locked(const char *path) {
    if (!path) return -1;
    if (ensure_data_dir() != 0) return -1;

    int fd = open(DB_PATH, O_RDONLY);
    if (fd < 0) {
        /* Nothing to remove if file missing */
        return 0;
    }
    FILE *in = fdopen(fd, "r");
    if (!in) { close(fd); return -1; }

    /* Write to tmp, then rename */
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", DB_PATH);
    int fd_out = open(tmp, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd_out < 0) { fclose(in); return -1; }
    FILE *out = fdopen(fd_out, "w");
    if (!out) { fclose(in); close(fd_out); unlink(tmp); return -1; }

    char *line = NULL;
    size_t lena = 0;
    ssize_t n;
    int removed = 0;
    while ((n = getline(&line, &lena, in)) != -1) {
        /* remove newline for comparison */
        if (n > 0 && line[n-1] == '\\n') line[n-1] = '\\0';
        if (strcmp(line, path) == 0) {
            removed = 1;
            continue; /* skip writing this line */
        }
        fprintf(out, "%s\\n", line);
    }
    free(line);
    fclose(in);
    fflush(out);
    fsync(fileno(out));
    fclose(out);

    /* Atomically replace */
    if (rename(tmp, DB_PATH) != 0) {
        /* rename failed */
        unlink(tmp);
        log_event("META remove failed: rename errno=%d", errno);
        return -1;
    }

    if (removed) log_event("META remove: %s", path);
    return 0;
}
