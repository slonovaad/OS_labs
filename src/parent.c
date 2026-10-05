#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(void) {
    int pipe_p_c1[2];
    int pipe_c1_c2[2];
    int pipe_c2_p[2];

    if (pipe(pipe_p_c1) == -1) {
        die("pipe_p_c1");
    }
    if (pipe(pipe_c1_c2) == -1) {
        die("pipe_c1_c2");
    }
    if (pipe(pipe_c2_p) == -1) {
        die("pipe_c2_p");
    }

    pid_t pid1 = fork();
    if (pid1 == -1) {
        die("fork1");
    }

    if (pid1 == 0) {
        if (dup2(pipe_p_c1[0], STDIN_FILENO) == -1) {
            die("c1 dup2 stdin");
        }
        if (dup2(pipe_c1_c2[1], STDOUT_FILENO) == -1) {
            die("c1 dup2 stdout");
        }

        close(pipe_p_c1[0]);
        close(pipe_p_c1[1]);
        close(pipe_c1_c2[0]);
        close(pipe_c1_c2[1]);
        close(pipe_c2_p[0]);
        close(pipe_c2_p[1]);

        execl("./child1", "./child1", (char *)NULL);
        die("execl");
    }

    pid_t pid2 = fork();
    if (pid2 == -1) {
        die("fork2");
    }

    close(pipe_p_c1[0]);
    close(pipe_c1_c2[1]);

    if (pid2 == 0) {
        if (dup2(pipe_c1_c2[0], STDIN_FILENO) == -1) {
            die("c2 dup2 stdin");
        }
        if (dup2(pipe_c2_p[1], STDOUT_FILENO) == -1) {
            die("c2 dup2 stdout");
        }

        close(pipe_p_c1[1]);
        close(pipe_c1_c2[0]);
        close(pipe_c2_p[0]);
        close(pipe_c2_p[1]);

        execl("./child2", "./child2", (char *)NULL);
        die("execl");
    }

    close(pipe_c1_c2[0]);
    close(pipe_c2_p[1]);

    char line[1024];
    FILE *up = fdopen(pipe_p_c1[1], "w");
    if (!up) {
        die("fdopen");
    }

    printf("Enter lines\n");
    while (fgets(line, sizeof(line), stdin) != NULL) {
        fputs(line, up);
        fflush(up);
    }

    fclose(up);



    

    FILE *down = fdopen(pipe_c2_p[0], "r");
    if (!down) die("fdopen");

    while (fgets(line, sizeof(line), down) != NULL) {
        fputs(line, stdout);
    }
    fclose(down);

    
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
    return 0;
}