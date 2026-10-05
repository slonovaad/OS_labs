#include <stdio.h>

int main(void) {
    char buf[1024];
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        int i = 0;
        while (buf[i]) {
            if (buf[i] >= 'A' && buf[i] <= 'Z') {
                buf[i] += 'a' - 'A';
            }
            i++;
        }
        fputs(buf, stdout);
        fflush(stdout);
    }
    return 0;
}