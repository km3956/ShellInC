#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int doCd(char *path) {
    if (path == NULL || strcmp(path, "~") == 0) {
        path = getenv("HOME");
    }
    if (chdir(path) == -1) {
        perror("cd");
        return -1;
    }
    return 0;
}

void execute_single_command(char *cmd) {
    char fnInput[128];
    char fnOutput[128];
    char fnAppend[128];
    char *args[256];
    char *token;
    int position;
    int pid;

    fnInput[0] = '\0';
    fnOutput[0] = '\0';
    fnAppend[0] = '\0';
    position = 0;

    token = strtok(cmd, " \t\r\n");
    while (token != NULL && position < 255) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token != NULL) {
                strncpy(fnInput, token, sizeof(fnInput) - 1);
                fnInput[sizeof(fnInput) - 1] = '\0';
            }
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token != NULL) {
                strncpy(fnOutput, token, sizeof(fnOutput) - 1);
                fnOutput[sizeof(fnOutput) - 1] = '\0';
            }
        } else if (strcmp(token, ">>") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token != NULL) {
                strncpy(fnAppend, token, sizeof(fnAppend) - 1);
                fnAppend[sizeof(fnAppend) - 1] = '\0';
            }
        } else {
            args[position] = token;
            position++;
            token = strtok(NULL, " \t\r\n");
        }
    }

    args[position] = NULL;

    if (args[0] == NULL) {
        return;
    }

    if (strcmp(args[0], "cd") == 0) {
        doCd(args[1]);
        return;
    }

    pid = fork();
    if (pid == 0) {
        if (fnInput[0] != '\0') {
            int fd = open(fnInput, O_RDONLY);
            if (fd < 0) {
                perror("open");
                exit(1);
            }
            dup2(fd, 0);
            close(fd);
        }

        if (fnOutput[0] != '\0') {
            int fd = open(fnOutput, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open");
                exit(1);
            }
            dup2(fd, 1);
            close(fd);
        }
        
        if (fnAppend[0] != '\0') {
            int fd = open(fnAppend, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) {
                perror("open");
                exit(1);
            }
            dup2(fd, 1);
            close(fd);
        }

        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "Command not found: %s\n", args[0]);
            exit(1);
        }
    } else if (pid < 0) {
        perror("fork");
    } else {
        wait(NULL);
    }
}

void execute_command(char *command) {
    char *commands[10];
    int cmd_count;
    char *token;
    int pipefd[2];
    int i;

    cmd_count = 0;

    token = strtok(command, "|");
    while (token != NULL && cmd_count < 10) {
        commands[cmd_count++] = token;
        token = strtok(NULL, "|");
    }
    commands[cmd_count] = NULL;

    for (i = 0; i < cmd_count; i++) {
        if (i < cmd_count - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                exit(1);
            }
        }

        if (fork() == 0) {
            if (i > 0) {
                dup2(pipefd[0], 0);
                close(pipefd[0]);
                close(pipefd[1]);
            }
            
            if (i < cmd_count - 1) {
                dup2(pipefd[1], 1);
                close(pipefd[0]);
                close(pipefd[1]);
            }

            execute_single_command(commands[i]);
            exit(0);
        } else {
            if (i > 0) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            wait(NULL);
        }
    }
}

int main() {
    char line[1024];
    while (1) {
        printf(">$ ");
        if (fgets(line, sizeof(line), stdin) == NULL) {
            exit(1);
        }
        line[strcspn(line, "\n")] = '\0';
        execute_command(line);
    }
    return 0;
}
