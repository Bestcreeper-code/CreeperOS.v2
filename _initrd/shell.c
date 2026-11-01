#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <dirent.h>

#define LINE_MAX_LEN 256
#define MAX_ARGS 128

int list_dir(int argc, char* argv[]) {
    const char* path = ".";
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
            printf("\033[93m%s/\033[0m\n", entry->d_name);
        } else {
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dir);
    return 0;
}

int cat_file(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: cat <file>\n");
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("fopen");
        return 1;
    }

    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        fwrite(buffer, 1, bytes_read, stdout);
    }

    fclose(file);
    return 0;
}

static int read_line(char* buf, size_t size) {
    size_t pos = 0;

    while (pos < size - 1) {
        char c;

        if (read(STDIN_FILENO, &c, 1) <= 0) {
            if (pos == 0)
                return -1;
            break;
        }

        if (c == '\n' || c == '\r') {
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                write(STDOUT_FILENO, "\b \b", 3);
            }
            continue;
        }

        buf[pos++] = c;
        write(STDOUT_FILENO, &c, 1);
    }

    buf[pos] = '\0';
    return (int)pos;
}

int main(int argc, char* argv[], char* envp[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("welcome to CreeperOS :^)\n");

    char buffer[LINE_MAX_LEN];
    char* new_argv[MAX_ARGS];
    int new_argc = 0;

    bool first_cmd = true;
    while (true) {
        if (first_cmd && argc > 1) {
            
            new_argc = 0;
            for (int i = 1; i < argc && new_argc < MAX_ARGS - 1; i++) {
                new_argv[new_argc++] = argv[i];
            }
            new_argv[new_argc] = NULL;
            first_cmd = false;
        } else {
            first_cmd = false;

            write(STDOUT_FILENO, "> ", 2);
            new_argc = 0;
            buffer[0] = '\0';
            int len = read_line(buffer, sizeof(buffer));
            if (len < 0) {
                write(STDOUT_FILENO, "\n", 1);
                exit(0);
            }
            if (len == 0)
                continue;

            new_argc = 0;
            char* token = strtok(buffer, " \t");
            while (token != NULL && new_argc < MAX_ARGS - 1) {
                new_argv[new_argc++] = token;
                token = strtok(NULL, " \t");
            }
            new_argv[new_argc] = NULL;

            if (new_argc == 0)
                continue;
        }

        /* Builtins */
        if (strcmp(new_argv[0], "cd") == 0) {
            if (new_argc < 2) {
                fprintf(stderr, "cd: missing argument\n");
                continue;
            }
            if (chdir(new_argv[1]) < 0) {
                perror("chdir");
            }
            continue;
        }
        else if (strcmp(new_argv[0], "ls") == 0) {
            list_dir(new_argc, new_argv);
            continue;
        }
        else if (strcmp(new_argv[0], "cat") == 0) {
            cat_file(new_argc, new_argv);
            continue;
        }
        else if (strcmp(new_argv[0], "clear") == 0) {
            write(STDOUT_FILENO, "\033[H\033[J", 6);
            continue;
        }
        else if (strcmp(new_argv[0], "exit") == 0) {
            exit(new_argc > 1 ? atoi(new_argv[1]) : 0);
        }

        
        pid_t pid = fork();

        if (pid == 0) {
            execve(new_argv[0], new_argv, envp);
            perror("exec");
            _exit(1);
        } else if (pid > 0) {
            waitpid(pid, NULL, 0);
        } else {
            perror("fork");
        }
    }

    return 0;
}