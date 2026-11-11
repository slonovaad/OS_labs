#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>

struct Shared {
    char buf[1024];
    int  len;
    int  quit;
    sem_t sem_p_c1;
    sem_t sem_c1_c2;
    sem_t sem_c2_p;
};

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return 1;
    }

    int fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        die("open");
    }
    struct Shared *sh = mmap(NULL, sizeof(*sh), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sh == MAP_FAILED) {
        die("mmap");
    }
    close(fd);

    
    for (;;) {
        if (sem_wait(&sh->sem_c1_c2) < 0) {
            die("sem_wait c1_c2");
        }

        if (sh->quit) {
            if (sem_post(&sh->sem_c2_p) < 0) {
                die("sem_post c2_p");
            }
            break;
        }

        for (int i = 0; i < sh->len; i++) {
            if (sh->buf[i] == ' ' || sh->buf[i] == '\t' || sh->buf[i] == '\r' ||
                sh->buf[i] == '\v' || sh->buf[i] == '\f') {
                sh->buf[i] = '_';
            }
        }
        if (sem_post(&sh->sem_c2_p) < 0) {
            die("sem_post c2_p");
        }
    }
    munmap(sh, sizeof(*sh));
    
    return 0;
}