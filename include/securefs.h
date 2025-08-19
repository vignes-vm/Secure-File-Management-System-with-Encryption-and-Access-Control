#ifndef SECUREFS_H
#define SECUREFS_H

#define PROJECT_NAME "SecureFS"
#define PROJECT_VERSION "0.1"

/* Step 2: Logging API */
int ensure_logs_dir(char *out_dir, unsigned long cap);
/* printf-like logger, appends to logs/securefs.log using open/write/close */
int log_event(const char *fmt, ...);

#endif /* SECUREFS_H */
