/* src/main.c
   Step 1: Basic entry program that confirms project init.
*/

#include <stdio.h>
#include "../include/securefs.h"

int main(void) {
    printf("%s v%s — SecureFS initialized\n", PROJECT_NAME, PROJECT_VERSION);
    return 0;
}
