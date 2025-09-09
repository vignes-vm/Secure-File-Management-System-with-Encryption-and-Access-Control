#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "securefs.h"

/* Simple XOR in-place transformation:
   We read the original file, write XOR'ed bytes to a temp file,
   then replace the original content with the encrypted bytes,
   and finally set permissions to 000. */
int do_lock_file(const char *path, const unsigned char *key, unsigned long klen) {
    if (!path || !*path) {
        log_event("LOCK failed: empty path");
        return -1;
    }

    /* 1) stat: for basic existence check */
    struct stat st;
    if (stat(path, &st) != 0) {
        log_event("LOCK failed: stat('%s') errno=%d", path, errno);
        perror("stat");
        return -1;
    }

    /* 2) Open input for reading */
    int fd_in = open(path, O_RDONLY);
    if (fd_in < 0) {
        log_event("LOCK failed: open('%s', RDONLY) errno=%d", path, errno);
        perror("open input");
        return -1;
    }

    /* 3) Open a temp file for encrypted output */
    char tmp_path[1200];
    snprintf(tmp_path, sizeof(tmp_path), "%s.enc.tmp", path);
    int fd_out = open(tmp_path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd_out < 0) {
        log_event("LOCK failed: open('%s', WRONLY) errno=%d", tmp_path, errno);
        perror("open temp");
        close(fd_in);
        return -1;
    }

    /* 4) XOR transform in chunks */
    unsigned char buf[8192];
    ssize_t n;
    unsigned long i = 0;
    unsigned char k = (klen > 0) ? key[0] : 0xAA; /* Step 3: 1-byte XOR; default 0xAA */
    while ((n = read(fd_in, buf, sizeof(buf))) > 0) {
        for (ssize_t j = 0; j < n; ++j) {
            buf[j] ^= k;
            (void)i; /* placeholder for future multi-byte key */
        }
        if (write(fd_out, buf, (size_t)n) != n) {
            log_event("LOCK failed: write temp errno=%d", errno);
            perror("write temp");
            close(fd_in);
            close(fd_out);
            unlink(tmp_path);
            return -1;
        }
    }
    if (n < 0) {
        log_event("LOCK failed: read input errno=%d", errno);
        perror("read input");
        close(fd_in);
        close(fd_out);
        unlink(tmp_path);
        return -1;
    }

    close(fd_in);
    if (close(fd_out) != 0) {
        log_event("LOCK warn: close temp errno=%d", errno);
    }

    /* 5) Overwrite original file with encrypted content (truncate) */
    int fd_final = open(path, O_WRONLY | O_TRUNC);
    if (fd_final < 0) {
        log_event("LOCK failed: open('%s', WRONLY|TRUNC) errno=%d", path, errno);
        perror("open final");
        unlink(tmp_path);
        return -1;
    }

    int fd_tmp = open(tmp_path, O_RDONLY);
    if (fd_tmp < 0) {
        log_event("LOCK failed: open tmp for read errno=%d", errno);
        perror("open tmp");
        close(fd_final);
        unlink(tmp_path);
        return -1;
    }

    while ((n = read(fd_tmp, buf, sizeof(buf))) > 0) {
        if (write(fd_final, buf, (size_t)n) != n) {
            log_event("LOCK failed: write final errno=%d", errno);
            perror("write final");
            close(fd_tmp);
            close(fd_final);
            unlink(tmp_path);
            return -1;
        }
    }
    if (n < 0) {
        log_event("LOCK failed: read tmp errno=%d", errno);
        perror("read tmp");
        close(fd_tmp);
        close(fd_final);
        unlink(tmp_path);
        return -1;
    }
    close(fd_tmp);
    close(fd_final);
    unlink(tmp_path); /* remove temp */

    /* 6) Restrict permissions: chmod 000 */
    if (chmod(path, 0000) != 0) {
        log_event("LOCK warn: chmod('%s',000) errno=%d", path, errno);
        perror("chmod 000");
        /* Even if chmod fails, content is encrypted; continue. */
    }

    log_event("LOCK ok: path='%s'", path);
    return 0;

    /* 7) Add to metadata DB */
    if (meta_add_locked(path) != 0) {
        log_event("LOCK warn: meta_add_locked failed for '%s'", path);
    }

    log_event("LOCK ok: path='%s'", path);
    return 0;
}
