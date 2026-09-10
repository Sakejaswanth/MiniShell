/**
 * @file cshell.h
 * @brief Core definitions, constants, and state structures for C-Shell.
 */

#ifndef CSHELL_H
#define CSHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define CSHELL_VERSION "1.0.0"
#define CSHELL_NAME    "cshell"

/* Configuration limits */
#define CSHELL_MAX_INPUT       4096
#define CSHELL_MAX_ARGS        128
#define CSHELL_MAX_PIPES       32
#define CSHELL_MAX_JOBS        64
#define CSHELL_HISTORY_DEFAULT 100
#define CSHELL_PATH_MAX        1024

/* Return status codes */
#define CSHELL_SUCCESS         0
#define CSHELL_ERR_GENERIC     1
#define CSHELL_ERR_NOT_FOUND   127
#define CSHELL_ERR_SYNTAX      2
#define CSHELL_EXIT_SIGNAL     -999

/* Background Job Structure */
typedef struct {
    int id;                 /* Job number (1, 2, ...) */
    unsigned long pid;      /* Process ID */
    char command[256];      /* Command representation */
    bool is_running;        /* Current execution state */
} job_t;

/* Global Shell State */
typedef struct {
    bool is_interactive;              /* True if connected to a TTY/terminal */
    bool should_exit;                 /* Shell termination flag */
    int last_exit_code;               /* Return code of last executed command ($?) */
    char current_dir[CSHELL_PATH_MAX];/* Cached working directory */
    char prompt_str[CSHELL_PATH_MAX]; /* Formatted shell prompt */
    job_t jobs[CSHELL_MAX_JOBS];      /* Background jobs table */
    int job_count;                    /* Current active jobs */
    int next_job_id;                  /* Monotonic job counter */
} cshell_state_t;

extern cshell_state_t g_shell;

/* Shell lifecycle */
void cshell_init(bool interactive);
void cshell_cleanup(void);
void cshell_update_prompt(void);
int  cshell_run_repl(void);
int  cshell_execute_string(const char *cmd_str);
int  cshell_execute_script(const char *filepath);

/* Job management */
int  cshell_add_job(unsigned long pid, const char *cmd);
void cshell_check_jobs(void);
void cshell_remove_job(int job_id);

#endif /* CSHELL_H */
