#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
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

int main(void) {
    char path[] = "/tmp/lab4_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        die("mkstemp");
    }
    if (ftruncate(fd, sizeof(struct Shared)) < 0) {
        die("ftruncate");
    }

    struct Shared *sh = mmap(NULL, sizeof(*sh), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sh == MAP_FAILED) {
        die("mmap");
    }
    close(fd);

    if (sem_init(&sh->sem_p_c1, 1, 0) < 0) {
        die("sem_init p_c1");
    }
    if (sem_init(&sh->sem_c1_c2, 1, 0) < 0) {
        die("sem_init c1_c2");
    }
    if (sem_init(&sh->sem_c2_p, 1, 0) < 0) {
        die("sem_init c2_p");
    }
    sh->quit = 0;
    sh->len = 0;

    pid_t pid1 = fork();
    if (pid1 == -1) {
        die("fork1");
    }

    if (pid1 == 0) {
        execl("./child1", "./child1", path, (char *)NULL);
        die("execl1");
    }

    pid_t pid2 = fork();
    if (pid2 == -1) {
        die("fork2");
    }

    if (pid2 == 0) {
        execl("./child2", "./child2", path, (char *)NULL);
        die("execl2");
    }


    char line[1024];
    printf("Enter lines\n");
    while (fgets(line, sizeof(line), stdin) != NULL) {
        int len = (int)strlen(line);
        memcpy(sh->buf, line, len);
        sh->len = len;

        if (sem_post(&sh->sem_p_c1) < 0) {
            die("sem_post p_c1");
        }
        if (sem_wait(&sh->sem_c2_p) < 0) {
            die("sem_wait c2_p");
        }

        fwrite(sh->buf, 1, sh->len, stdout);
    }

    sh->quit = 1;
    if (sem_post(&sh->sem_p_c1) < 0) {
        die("sem_post quit");
    }
    if (sem_wait(&sh->sem_c2_p) < 0) {
        die("sem_wait quit");
    }

    int status;
    if (waitpid(pid1, &status, 0) == -1) {
        die("waitpid 1");
    }    
    if (WIFEXITED(status)) {
        printf("\nchild1 end with %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("\nchild1 killed by %d\n", WTERMSIG(status));
    }

    if (waitpid(pid2, &status, 0) == -1) {
        die("waitpid 2");
    }
    if (WIFEXITED(status)) {
        printf("\nchild2 end with %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("\nchild2 killed by %d\n", WTERMSIG(status));
    }

    sem_destroy(&sh->sem_p_c1);
    sem_destroy(&sh->sem_c1_c2);
    sem_destroy(&sh->sem_c2_p);
    munmap(sh, sizeof(*sh));
    unlink(path);

    return 0;
}