#define _XOPEN_SOURCE 700
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "securefs.h"

/* Ensure logs/ exists; returns 0 on success */
int ensure_logs_dir(char *out_dir, unsigned long cap) {
    /* Build path: <project_root>/logs
       We assume current working directory is the project root (Step 1 rule). */
    const char *dir = "logs";
    if (out_dir && cap) {
        /* Copy path to out_dir */
        unsigned long n = (unsigned long)snprintf(out_dir, cap, "%s", dir);
        if (n >= cap) return -1;
    }

    struct stat st;
    if (stat(dir, &st) == 0) {
        /* exists and is directory? */
        if (S_ISDIR(st.st_mode)) return 0;
        /* Exists but not a directory -> error */
        return -1;
    }
    /* Not exists -> create */
    if (mkdir(dir, 0700) != 0) {
        return -1;
    }
    return 0;
}

/* Append one formatted line to logs/securefs.log using system calls */
int log_event(const char *fmt, ...) {
    char logdir[64];
    if (ensure_logs_dir(logdir, sizeof(logdir)) != 0) {
        /* As a fallback, do nothing but signal error */
        return -1;
    }

    const char *logpath = "logs/securefs.log";

    /* Build the final line in memory first */
    char message[1024];

    /* Timestamp */
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);

    /* Variable part from caller */
    char userbuf[768];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(userbuf, sizeof(userbuf), fmt, ap);
    va_end(ap);

    /* Prefix with timestamp + PID + project tag */
    pid_t pid = getpid();
    snprintf(message, sizeof(message), "[%s] PID=%d %s: %s\n",
             ts, (int)pid, PROJECT_NAME, userbuf);

    /* Open (create if needed) and append using system calls */
    int fd = open(logpath, O_CREAT | O_APPEND | O_WRONLY, 0600);
    if (fd < 0) {
        return -1;
    }

    size_t len = strlen(message);
    ssize_t nw = write(fd, message, len);
    int saved_errno = errno; /* capture errno before close */

    /* Close regardless of write result */
    if (close(fd) != 0) {
        return -1;
    }
    if (nw < 0) {
        errno = saved_errno;
        return -1;
    }
    return 0;
}
