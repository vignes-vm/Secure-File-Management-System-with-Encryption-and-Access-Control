#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "securefs.h"

/* Step 4: Unlock = chmod 600 -> decrypt via XOR -> write back -> chmod 644 */
int do_unlock_file(const char *path, const unsigned char *key, unsigned long klen) {
    if (!path || !*path) {
        log_event("UNLOCK failed: empty path");
        return -1;
    }

    /* 0) Ensure file exists (optional, but helpful) */
    struct stat st;
    if (stat(path, &st) != 0) {
        log_event("UNLOCK failed: stat('%s') errno=%d", path, errno);
        perror("stat");
        return -1;
    }

    /* 1) Temporarily allow owner access to open & modify */
    if (chmod(path, 0600) != 0) {
        log_event("UNLOCK warn: chmod('%s',0600) errno=%d", path, errno);
        perror("chmod 600");
        /* Continue; if we can't open, we'll fail below. */
    }

    /* 2) Open original (currently encrypted) and temp for decrypted output */
    int fd_in = open(path, O_RDONLY);
    if (fd_in < 0) {
        log_event("UNLOCK failed: open('%s', RDONLY) errno=%d", path, errno);
        perror("open input");
        return -1;
    }

    char tmp_path[1200];
    snprintf(tmp_path, sizeof(tmp_path), "%s.dec.tmp", path);
    int fd_out = open(tmp_path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd_out < 0) {
        log_event("UNLOCK failed: open('%s', WRONLY) errno=%d", tmp_path, errno);
        perror("open temp");
        close(fd_in);
        return -1;
    }

    /* 3) XOR decrypt in chunks (same 1-byte XOR scheme as lock) */
    unsigned char buf[8192];
    ssize_t n;
    unsigned char k = (klen > 0) ? key[0] : 0xAA; /* default 0xAA, symmetric */
    while ((n = read(fd_in, buf, sizeof(buf))) > 0) {
        for (ssize_t j = 0; j < n; ++j) buf[j] ^= k;
        if (write(fd_out, buf, (size_t)n) != n) {
            log_event("UNLOCK failed: write temp errno=%d", errno);
            perror("write temp");
            close(fd_in);
            close(fd_out);
            unlink(tmp_path);
            return -1;
        }
    }
    if (n < 0) {
        log_event("UNLOCK failed: read input errno=%d", errno);
        perror("read input");
        close(fd_in);
        close(fd_out);
        unlink(tmp_path);
        return -1;
    }

    close(fd_in);
    if (close(fd_out) != 0) {
        log_event("UNLOCK warn: close temp errno=%d", errno);
    }

    /* 4) Overwrite the original with decrypted content */
    int fd_final = open(path, O_WRONLY | O_TRUNC);
    if (fd_final < 0) {
        log_event("UNLOCK failed: open('%s', WRONLY|TRUNC) errno=%d", path, errno);
        perror("open final");
        unlink(tmp_path);
        return -1;
    }

    int fd_tmp = open(tmp_path, O_RDONLY);
    if (fd_tmp < 0) {
        log_event("UNLOCK failed: open tmp for read errno=%d", errno);
        perror("open tmp");
        close(fd_final);
        unlink(tmp_path);
        return -1;
    }

    while ((n = read(fd_tmp, buf, sizeof(buf))) > 0) {
        if (write(fd_final, buf, (size_t)n) != n) {
            log_event("UNLOCK failed: write final errno=%d", errno);
            perror("write final");
            close(fd_tmp);
            close(fd_final);
            unlink(tmp_path);
            return -1;
        }
    }
    if (n < 0) {
        log_event("UNLOCK failed: read tmp errno=%d", errno);
        perror("read tmp");
        close(fd_tmp);
        close(fd_final);
        unlink(tmp_path);
        return -1;
    }
    close(fd_tmp);
    close(fd_final);
    unlink(tmp_path);

    /* 5) Restore normal, user-readable permissions: 0644 */
    if (chmod(path, 0644) != 0) {
        log_event("UNLOCK warn: chmod('%s',0644) errno=%d", path, errno);
        perror("chmod 644");
        /* Continue even if this fails; contents are decrypted. */
    }

    log_event("UNLOCK ok: path='%s'", path);
    return 0;
}
