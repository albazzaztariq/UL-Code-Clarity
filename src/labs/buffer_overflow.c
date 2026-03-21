#include <stdio.h>
#include <string.h>

// VULNERABLE: buffer is only 8 bytes but gets() reads unlimited input
// This is the classic stack buffer overflow — the extra bytes overwrite
// the return address, letting an attacker redirect execution.

int main(void) {
    char buf[8];
    printf("Enter input: ");
    fflush(stdout);
    gets(buf);   // UNSAFE: no length check
    printf("You said: %s\n", buf);
    printf("Program finished normally.\n");
    return 0;
}
