#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FIXED: Set the pointer to NULL immediately after free.
// Any subsequent dereference will crash immediately (null dereference)
// rather than silently returning garbage — making the bug easy to detect.

int main(void) {
    char* secret = (char*)malloc(32);
    strcpy(secret, "sensitive data: password=hunter2");
    printf("[alloc]  ptr=%p  value=\"%s\"\n", (void*)secret, secret);

    free(secret);
    secret = NULL;  // FIX: nullify immediately
    printf("[free+null]  ptr is now NULL\n");

    // Now any use of secret will be caught at compile-time (nullptr warning)
    // or crash immediately on dereference — not silently corrupt data.
    if (secret != NULL) {
        printf("[use]  %s\n", secret);
    } else {
        printf("[guard]  pointer is NULL — skipping use (bug prevented)\n");
    }

    return 0;
}
