#ifndef SECUREFS_H
#define SECUREFS_H

#define PROJECT_NAME "SecureFS"
#define PROJECT_VERSION "0.1"

/* Step 2: Logging API */
int ensure_logs_dir(char *out_dir, unsigned long cap);
int log_event(const char *fmt, ...);

/* Step 3: Lock API */
int do_lock_file(const char *path, const unsigned char *key, unsigned long klen);

/* Step 4: Unlock API */
int do_unlock_file(const char *path, const unsigned char *key, unsigned long klen);

/* Step 5: Metadata API (locked files DB) */
int meta_add_locked(const char *path);      /* add path to data/locked_files.db */
int meta_remove_locked(const char *path);   /* remove path from DB */
int meta_is_locked(const char *path);       /* 1 if locked, 0 if not, -1 on error */

#endif /* SECUREFS_H */
