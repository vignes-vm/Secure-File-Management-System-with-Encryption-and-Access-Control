/* src/main.c
   Step 2: Call logger to verify logs/securefs.log is written.
*/

#include <stdio.h>
#include "../include/securefs.h"

int main(void) {
    printf("%s v%s — SecureFS initialized\n", PROJECT_NAME, PROJECT_VERSION);

    /* Test the logger */
    if (log_event("Program started") != 0) {
        printf("WARN: logging failed\n");
    } else {
        printf("Log entry appended to logs/securefs.log\n");
    }

    return 0;
}
