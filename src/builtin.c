/**
 * @file builtin.c
 * @brief Shell built-in commands implementation.
 */

#include "builtin.h"
#include "cshell.h"
#include "history.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_old_pwd[CSHELL_PATH_MAX] = {0};

/* Table of built-in commands */
static const builtin_desc_t BUILTIN_COMMANDS[] = {
    {"cd",       builtin_cd,       "Change the current working directory", "cd [dir | ~ | -]"},
    {"pwd",      builtin_pwd,      "Print the current working directory", "pwd"},
    {"echo",     builtin_echo,     "Print arguments to standard output", "echo [-n] [string ...]"},
    {"history",  builtin_history,  "Display command history", "history [n]"},
    {"type",     builtin_type,     "Display information about command type", "type command"},
    {"export",   builtin_export,   "Set an environment variable", "export KEY=VALUE"},
    {"setenv",   builtin_setenv,   "Set an environment variable (C-shell style)", "setenv KEY VALUE"},
    {"unsetenv", builtin_unsetenv, "Remove an environment variable", "unsetenv KEY"},
    {"jobs",     builtin_jobs,     "List active background jobs", "jobs"},
    {"clear",    builtin_clear,    "Clear the terminal screen", "clear"},
    {"help",     builtin_help,     "Display help for built-in commands", "help"},
    {"exit",     builtin_exit,     "Exit C-Shell", "exit [status]"},
    {NULL,       NULL,             NULL, NULL}
};

bool builtin_is_builtin(const char *cmd) {
    if (!cmd) return false;
    for (int i = 0; BUILTIN_COMMANDS[i].name != NULL; i++) {
        if (strcmp(cmd, BUILTIN_COMMANDS[i].name) == 0) {
            return true;
        }
    }
    return false;
}

int builtin_execute(int argc, char **argv) {
    if (argc == 0 || !argv || !argv[0]) return CSHELL_SUCCESS;

    for (int i = 0; BUILTIN_COMMANDS[i].name != NULL; i++) {
        if (strcmp(argv[0], BUILTIN_COMMANDS[i].name) == 0) {
            return BUILTIN_COMMANDS[i].func(argc, argv);
        }
    }
    return CSHELL_ERR_NOT_FOUND;
}

void builtin_print_help(void) {
    printf("\n=================================================================\n");
    printf("   C-Shell (cshell) Built-in Commands Reference - v%s\n", CSHELL_VERSION);
    printf("=================================================================\n\n");
    for (int i = 0; BUILTIN_COMMANDS[i].name != NULL; i++) {
        printf("  %-12s %-25s - %s\n",
               BUILTIN_COMMANDS[i].name,
               BUILTIN_COMMANDS[i].usage,
               BUILTIN_COMMANDS[i].description);
    }
    printf("\nFeatures Supported:\n");
    printf("  * Pipelines:         cmd1 | cmd2 | cmd3\n");
    printf("  * I/O Redirection:   cmd < in.txt > out.txt (or >> append.txt)\n");
    printf("  * Background Jobs:   cmd &\n");
    printf("  * Command Chains:    cmd1 ; cmd2 ; cmd3\n");
    printf("  * History Expansion: !! (repeat last) or !n (repeat command n)\n");
    printf("  * Script Execution:  cshell script.csh\n");
    printf("=================================================================\n\n");
}

int builtin_cd(int argc, char **argv) {
    const char *target = NULL;
    char current_dir[CSHELL_PATH_MAX];
    platform_get_cwd(current_dir, sizeof(current_dir));

    if (argc == 1 || strcmp(argv[1], "~") == 0) {
        target = getenv("HOME");
#if defined(_WIN32) || defined(_WIN64)
        if (!target) target = getenv("USERPROFILE");
#endif
        if (!target) {
            fprintf(stderr, "cshell: cd: HOME directory not set\n");
            return CSHELL_ERR_GENERIC;
        }
    } else if (strcmp(argv[1], "-") == 0) {
        if (g_old_pwd[0] == '\0') {
            fprintf(stderr, "cshell: cd: OLDPWD not set\n");
            return CSHELL_ERR_GENERIC;
        }
        target = g_old_pwd;
        printf("%s\n", target);
    } else {
        target = argv[1];
    }

    if (platform_change_dir(target) != 0) {
        fprintf(stderr, "cshell: cd: cannot change directory to '%s'\n", target);
        return CSHELL_ERR_GENERIC;
    }

    /* Update OLDPWD and current state */
    strncpy(g_old_pwd, current_dir, sizeof(g_old_pwd) - 1);
    g_old_pwd[sizeof(g_old_pwd) - 1] = '\0';
    platform_setenv("OLDPWD", g_old_pwd);

    cshell_update_prompt();
    return CSHELL_SUCCESS;
}

int builtin_pwd(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char buf[CSHELL_PATH_MAX];
    if (platform_get_cwd(buf, sizeof(buf))) {
        printf("%s\n", buf);
        return CSHELL_SUCCESS;
    }
    fprintf(stderr, "cshell: pwd: error retrieving current directory\n");
    return CSHELL_ERR_GENERIC;
}

int builtin_echo(int argc, char **argv) {
    bool newline = true;
    int start = 1;

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        newline = false;
        start = 2;
    }

    for (int i = start; i < argc; i++) {
        const char *arg = argv[i];
        for (size_t j = 0; arg[j] != '\0'; j++) {
            if (arg[j] == '\\' && arg[j + 1] != '\0') {
                j++;
                switch (arg[j]) {
                    case 'n': putchar('\n'); break;
                    case 't': putchar('\t'); break;
                    case 'r': putchar('\r'); break;
                    case '\\': putchar('\\'); break;
                    default:
                        putchar('\\');
                        putchar(arg[j]);
                        break;
                }
            } else {
                putchar(arg[j]);
            }
        }
        if (i < argc - 1) {
            putchar(' ');
        }
    }

    if (newline) {
        putchar('\n');
    }
    fflush(stdout);
    return CSHELL_SUCCESS;
}

int builtin_history(int argc, char **argv) {
    int limit = 0;
    if (argc > 1) {
        limit = atoi(argv[1]);
    }
    history_print(limit);
    return CSHELL_SUCCESS;
}

int builtin_type(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "cshell: type: missing argument\n");
        return CSHELL_ERR_GENERIC;
    }

    for (int i = 1; i < argc; i++) {
        if (builtin_is_builtin(argv[i])) {
            printf("%s is a shell built-in command\n", argv[i]);
        } else {
            char *path = platform_find_in_path(argv[i]);
            if (path) {
                printf("%s is %s\n", argv[i], path);
                free(path);
            } else {
                fprintf(stderr, "cshell: type: %s: not found\n", argv[i]);
            }
        }
    }
    return CSHELL_SUCCESS;
}

int builtin_export(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "cshell: export: usage: export KEY=VALUE\n");
        return CSHELL_ERR_GENERIC;
    }

    char *eq = strchr(argv[1], '=');
    if (!eq) {
        fprintf(stderr, "cshell: export: invalid format, expected KEY=VALUE\n");
        return CSHELL_ERR_GENERIC;
    }

    *eq = '\0';
    const char *key = argv[1];
    const char *val = eq + 1;
    int res = platform_setenv(key, val);
    *eq = '='; /* restore */
    return res == 0 ? CSHELL_SUCCESS : CSHELL_ERR_GENERIC;
}

int builtin_setenv(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "cshell: setenv: usage: setenv KEY [VALUE]\n");
        return CSHELL_ERR_GENERIC;
    }
    const char *key = argv[1];
    const char *val = (argc >= 3) ? argv[2] : "";
    return platform_setenv(key, val) == 0 ? CSHELL_SUCCESS : CSHELL_ERR_GENERIC;
}

int builtin_unsetenv(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "cshell: unsetenv: usage: unsetenv KEY\n");
        return CSHELL_ERR_GENERIC;
    }
    return platform_unsetenv(argv[1]) == 0 ? CSHELL_SUCCESS : CSHELL_ERR_GENERIC;
}

int builtin_jobs(int argc, char **argv) {
    (void)argc;
    (void)argv;
    cshell_check_jobs();

    if (g_shell.job_count == 0) {
        printf("No active background jobs.\n");
        return CSHELL_SUCCESS;
    }

    for (int i = 0; i < CSHELL_MAX_JOBS; i++) {
        if (g_shell.jobs[i].id > 0) {
            printf("[%d]  PID: %-8lu  Status: %-10s  Command: %s\n",
                   g_shell.jobs[i].id,
                   g_shell.jobs[i].pid,
                   g_shell.jobs[i].is_running ? "Running" : "Done",
                   g_shell.jobs[i].command);
        }
    }
    return CSHELL_SUCCESS;
}

int builtin_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    /* Use ANSI escape sequences to clear screen and reposition cursor */
    printf("\033[2J\033[H");
    fflush(stdout);
    return CSHELL_SUCCESS;
}

int builtin_help(int argc, char **argv) {
    (void)argc;
    (void)argv;
    builtin_print_help();
    return CSHELL_SUCCESS;
}

int builtin_exit(int argc, char **argv) {
    int code = g_shell.last_exit_code;
    if (argc > 1) {
        code = atoi(argv[1]);
    }
    g_shell.should_exit = true;
    g_shell.last_exit_code = code;
    return code;
}
