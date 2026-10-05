#include <stdio.h>

int main(void) {
    char buf[1024];
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        int i = 0;
        while (buf[i]) {
            if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\r' ||
                buf[i] == '\v' || buf[i] == '\f') {
                buf[i] = '_';
            }
            i++;
        }
        fputs(buf, stdout);
        fflush(stdout);
    }
    return 0;
}