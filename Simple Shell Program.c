#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#define MAX_LINE 80 /* The maximum length command */

char command[MAX_LINE]; // the command that was entered
char histbuf[MAX_LINE] = { 0 };

int main(void)
{
    /* command line (of 80) has max of 40 arguments */
    char* args[MAX_LINE / 2 + 1];

    /* flag to determine when to exit program */
    int should_run = 1;

    while (should_run) {
        printf("$"); /* prompt */
        fflush(stdout);

        /* read a command from the keyboard */
        fgets(command, MAX_LINE, stdin);

        for (int i = 0; i < MAX_LINE; i++) {
            if (command[i] == '\n' || command[i] == '\r') {
                command[i] = '\0';
            }
        }

        if (strcmp(command, "!!") == 0) {
            if (*histbuf == '\0') { // no recent command in the history
                printf("No commands in history buffer.\n");
                continue;
            }
            else {
                // use the history command as the current command
                strcpy(command, histbuf);
                printf("%s\n", command);
            }
        }
        else {
            // save the current command as the history command
            strcpy(histbuf, command);
        }

        /* parse the command */
        int num = 0;
        int reco = 0;
        int flag = 0;
        int redir = 0;
        int mark = 0;
        int nonempty = 0;
        char filename[MAX_LINE] = { 0 };
        char* subcmd[MAX_LINE / 2 + 1];

        for (int i = 0; i < MAX_LINE; i++) {
            if (command[i] == '\0') { // At the end of command
                if (!nonempty) { // input nothing
                    break;
                }
                if (reco != i) {
                    args[num++] = &command[reco];
                }
                if (strcmp(args[0], "exit") == 0) { // input exit
                    should_run = 0;
                    exit(1);
                }
                args[num] = NULL;
                break;
            }
            else if (command[i] == ' ') { // separate parts of command
                command[i] = '\0';
                if (i > 0 && command[i - 1] != '\0') {
                    args[num++] = &command[reco];
                }
                reco = i + 1;
            }
            else if (command[i] == '>' || command[i] == '<' || command[i] == '|') {
                switch (command[i]) {
                case '>': redir = 1;
                    break;
                case '<': redir = 2;
                    break;
                case '|': redir = 3;
                    break;
                }
                command[i] = '\0';
                if (i > 0 && command[i - 1] != '\0') {
                    args[num++] = &command[reco]; // save command before '>','<','|'
                }
                mark = num++; // mark the position of '>','<','|'
                reco = i + 1;
            }
            else {
                nonempty = 1;
            }
        }

        if (!nonempty) {
            continue;
        }

        if (num > 0) {
            // the parent process is not to wait for the child to exit
            if (strcmp(args[num - 1], "&") == 0) {
                flag = 1;
                args[num - 1] = NULL;
            }
        }

        if (redir == 1 || redir == 2) {
            strcpy(filename, args[mark + 1]); // save the name of the file
            args[mark] = NULL;
        }

        if (redir == 3) {
            int index = mark + 1;
            for (int i = 0; i < num - mark - 1; i++) {
                subcmd[i] = args[index++]; // save command after '|'
            }
            subcmd[index] = NULL;
            args[mark] = NULL;
        }

        // Execute the command typed by the user:
        pid_t pid; // PID of child
        int status; // Exit status of child
        int fd;
        int fd2[2];

        pid = fork(); // New child process starts executing code from here

        if (pid < 0) { // No child process created
            write(1, "Parent: fork failed\n", 20);
            return 1; // Parent process dies
        }
        else if (pid == 0) {
            // Code only executed by the new child process
            if (redir == 1) {
                fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0666);
                dup2(fd, STDOUT_FILENO); // duplicate fd to standard output
                close(fd);
            }

            if (redir == 2) {
                fd = open(filename, O_CREAT | O_RDWR, 0666);
                dup2(fd, STDIN_FILENO); // duplicate fd to standard input
                close(fd);
            }

            if (redir == 3) {
                // establish a pipe between child and the child process it creates
                if (pipe(fd2) == -1) {
                    perror("pipe fd2");
                    return 1;
                }

                pid = fork(); // child creates another child process

                if (pid < 0) {
                    write(1, "Parent: fork failed\n", 20);
                    return 1;
                }
                else if (pid == 0) {
                    close(fd2[0]);
                    // duplicate write end of pipe to standard output
                    dup2(fd2[1], STDOUT_FILENO);
                    // execute command before '|' and write the output to pipe
                    execvp(args[0], args);

                    write(1, "Child: exec failed, die\n", 24);
                    return 1; // Child process dies after failed exec
                }
                else {
                    close(fd2[1]);
                    // duplicate read end of pipe to standard input
                    dup2(fd2[0], STDIN_FILENO);
                    // execute command after '|' by reading the input of pipe
                    execvp(subcmd[0], subcmd);

                    write(1, "Child: exec failed, die\n", 24);
                    return 1; // Child process dies after failed exec
                }
            }

            execvp(args[0], args);

            write(1, "Child: exec failed, die\n", 24);
            return 1; // Child process dies after failed exec
        }
        else {
            // Code only executed by the parent process
            if (flag == 0) {
                wait(&status);
            }
        }
    }
    return 0;
}
