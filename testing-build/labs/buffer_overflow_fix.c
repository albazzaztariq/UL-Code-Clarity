#include <stdio.h>
#include <string.h>

// FIXED: use fgets() with an explicit size limit — buf cannot overflow.
// fgets reads at most sizeof(buf)-1 characters, leaving room for '\0'.

int main(void) {
    char buf[8];
    printf("Enter input: ");
    fflush(stdout);
    fgets(buf, sizeof(buf), stdin);  // SAFE: length-bounded read
    // Strip trailing newline if present
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
    printf("You said: %s\n", buf);
    printf("Program finished normally.\n");
    return 0;
}
