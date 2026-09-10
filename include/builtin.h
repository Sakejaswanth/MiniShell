/**
 * @file builtin.h
 * @brief Shell built-in command declarations and dispatcher.
 */

#ifndef CSHELL_BUILTIN_H
#define CSHELL_BUILTIN_H

#include <stdbool.h>

/* Built-in handler function signature */
typedef int (*builtin_func_t)(int argc, char **argv);

typedef struct {
    const char *name;
    builtin_func_t func;
    const char *description;
    const char *usage;
} builtin_desc_t;

/* Dispatcher interface */
bool builtin_is_builtin(const char *cmd);
int  builtin_execute(int argc, char **argv);
void builtin_print_help(void);

/* Individual built-in handlers */
int builtin_cd(int argc, char **argv);
int builtin_pwd(int argc, char **argv);
int builtin_echo(int argc, char **argv);
int builtin_history(int argc, char **argv);
int builtin_type(int argc, char **argv);
int builtin_export(int argc, char **argv);
int builtin_setenv(int argc, char **argv);
int builtin_unsetenv(int argc, char **argv);
int builtin_jobs(int argc, char **argv);
int builtin_clear(int argc, char **argv);
int builtin_help(int argc, char **argv);
int builtin_exit(int argc, char **argv);

#endif /* CSHELL_BUILTIN_H */
