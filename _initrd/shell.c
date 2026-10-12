#include "stdlib.h"
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <dirent.h>

int list_dir(int argc, char* argv[]){
    char* path = ".";
    if (argc > 1) {
        path = argv[1];
    }

    DIR* dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return 1;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            printf("\e[93m%s/\e[0m\n", entry->d_name);
        } else {
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dir);
    return 0;
}

int cat_file(int argc, char* argv[]){
    if (argc < 2) {
        fprintf(stderr, "cat <file>\n");
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("fopen");
        return 1;
    }

    char buffer[1024];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        fwrite(buffer, 1, bytesRead, stdout);
    }

    fclose(file);
    return 0;
}

int main(int argc, char* argv[], char* envp[])
{
    printf(" welcome to CreeperOS :^)");
    char buffer[256];

    while (true) {
        int pos = 0;

        write(STDOUT_FILENO, "> ", 2);

        while (pos < sizeof(buffer) - 1) {
            char c;

            // Read exactly one character from the keyboard
            if (read(STDIN_FILENO, &c, 1) <= 0)
                break;

            // Enter
            if (c == '\n' || c == '\r') {
                write(STDOUT_FILENO, "\n", 1);
                break;
            }

            // Backspace
            if (c == '\b' || c == 127) {
                if (pos > 0) {
                    pos--;

                    write(STDOUT_FILENO, "\b \b", 3);
                }
                continue;
            }

            // Store character
            buffer[pos++] = c;

            // Immediately send character to screen
            write(STDOUT_FILENO, &c, 1);
        }

        buffer[pos] = '\0';

        if (pos == 0)
            continue;

        char* new_argv[128];
        int new_argc = 0;

        char* token = strtok(buffer, " \t");

        while (token != NULL &&
               new_argc < 127) {

            new_argv[new_argc++] = token;
            token = strtok(NULL, " \t");
        }

        new_argv[new_argc] = NULL;

        if (new_argc == 0)
            continue;








        if (strcmp(new_argv[0], "ls") == 0) {
            list_dir(new_argc, new_argv);
            continue;
        }
        else if (strcmp(new_argv[0], "cat") == 0) {
            cat_file(new_argc, new_argv);
            continue;
        }








        pid_t pid = fork();

        if (pid == 0) {
            execve(new_argv[0], new_argv, envp);
            perror("exec");
            exit(1);
        }

        if (pid > 0) {
            waitpid(pid, NULL, 0);
        } else {
            perror("fork");
        }
    }

    return 0;
}