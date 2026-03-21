#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// VULNERABLE: Use-After-Free
// The pointer is freed but the program still reads through it.
// The freed memory may have been reused — reading it returns
// attacker-controlled data or causes a crash.

int main(void) {
    char* secret = (char*)malloc(32);
    strcpy(secret, "sensitive data: password=hunter2");
    printf("[alloc]  ptr=%p  value=\"%s\"\n", (void*)secret, secret);

    free(secret);
    printf("[free]   ptr=%p  (memory returned to allocator)\n", (void*)secret);

    // Simulate another allocation reusing the same memory
    char* reuse = (char*)malloc(32);
    strcpy(reuse, "ATTACKER_CONTROLLED_DATA_XYZ!!!!!");

    // BUG: still reading through the freed pointer
    printf("[use]    ptr=%p  value=\"%s\"\n", (void*)secret, secret);
    printf("\nNOTICE: the freed pointer now shows the attacker's data!\n");

    free(reuse);
    return 0;
}
