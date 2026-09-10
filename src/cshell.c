/**
 * @file cshell.c
 * @brief Shell lifecycle, global state management, REPL loop, and job tracking.
 */

#include "cshell.h"
#include "lexer.h"
#include "parser.h"
#include "executor.h"
#include "history.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

cshell_state_t g_shell;

void cshell_init(bool interactive) {
    memset(&g_shell, 0, sizeof(cshell_state_t));
    g_shell.is_interactive = interactive;
    g_shell.should_exit = false;
    g_shell.last_exit_code = 0;
    g_shell.job_count = 0;
    g_shell.next_job_id = 1;

    platform_init();
    platform_setup_signals();
    cshell_update_prompt();

    history_init(CSHELL_HISTORY_DEFAULT);

    /* Load history file from HOME */
    const char *home = getenv("HOME");
#if defined(_WIN32) || defined(_WIN64)
    if (!home) home = getenv("USERPROFILE");
#endif
    if (home) {
        char hist_file[CSHELL_PATH_MAX];
        snprintf(hist_file, sizeof(hist_file), "%s/.cshell_history", home);
        history_load(hist_file);
    }
}

void cshell_cleanup(void) {
    const char *home = getenv("HOME");
#if defined(_WIN32) || defined(_WIN64)
    if (!home) home = getenv("USERPROFILE");
#endif
    if (home) {
        char hist_file[CSHELL_PATH_MAX];
        snprintf(hist_file, sizeof(hist_file), "%s/.cshell_history", home);
        history_save(hist_file);
    }
    history_cleanup();
}

void cshell_update_prompt(void) {
    platform_get_cwd(g_shell.current_dir, sizeof(g_shell.current_dir));

    char username[128];
    char hostname[128];
    platform_get_username(username, sizeof(username));
    platform_get_hostname(hostname, sizeof(hostname));

    /* Replace full HOME path prefix with ~ */
    const char *home = getenv("HOME");
#if defined(_WIN32) || defined(_WIN64)
    if (!home) home = getenv("USERPROFILE");
#endif
    const char *disp_dir = g_shell.current_dir;
    char rel_dir[CSHELL_PATH_MAX];

    if (home && strncmp(g_shell.current_dir, home, strlen(home)) == 0) {
        snprintf(rel_dir, sizeof(rel_dir), "~%s", g_shell.current_dir + strlen(home));
        disp_dir = rel_dir;
    }

    /* Colored prompt: username@host:dir$ */
    snprintf(g_shell.prompt_str, sizeof(g_shell.prompt_str),
             "\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m$ ",
             username, hostname, disp_dir);
}

int cshell_add_job(unsigned long pid, const char *cmd) {
    for (int i = 0; i < CSHELL_MAX_JOBS; i++) {
        if (g_shell.jobs[i].id == 0) {
            g_shell.jobs[i].id = g_shell.next_job_id++;
            g_shell.jobs[i].pid = pid;
            g_shell.jobs[i].is_running = true;
            strncpy(g_shell.jobs[i].command, cmd ? cmd : "unknown", sizeof(g_shell.jobs[i].command) - 1);
            g_shell.job_count++;
            return g_shell.jobs[i].id;
        }
    }
    return -1;
}

void cshell_check_jobs(void) {
    for (int i = 0; i < CSHELL_MAX_JOBS; i++) {
        if (g_shell.jobs[i].id > 0 && g_shell.jobs[i].is_running) {
            platform_proc_t proc;
#if defined(_WIN32) || defined(_WIN64)
            proc.process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, (DWORD)g_shell.jobs[i].pid);
            proc.process_id = (DWORD)g_shell.jobs[i].pid;
            if (!platform_is_process_running(&proc)) {
                g_shell.jobs[i].is_running = false;
                printf("[%d]+  Done                    %s\n", g_shell.jobs[i].id, g_shell.jobs[i].command);
                cshell_remove_job(g_shell.jobs[i].id);
            }
            if (proc.process_handle != NULL && proc.process_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(proc.process_handle);
            }
#else
            proc.pid = (pid_t)g_shell.jobs[i].pid;
            if (!platform_is_process_running(&proc)) {
                g_shell.jobs[i].is_running = false;
                printf("[%d]+  Done                    %s\n", g_shell.jobs[i].id, g_shell.jobs[i].command);
                cshell_remove_job(g_shell.jobs[i].id);
            }
#endif
        }
    }
}

void cshell_remove_job(int job_id) {
    for (int i = 0; i < CSHELL_MAX_JOBS; i++) {
        if (g_shell.jobs[i].id == job_id) {
            g_shell.jobs[i].id = 0;
            g_shell.jobs[i].pid = 0;
            g_shell.jobs[i].is_running = false;
            g_shell.jobs[i].command[0] = '\0';
            g_shell.job_count--;
            break;
        }
    }
}

int cshell_execute_string(const char *cmd_str) {
    if (!cmd_str) return CSHELL_SUCCESS;

    char expanded[CSHELL_MAX_INPUT];
    if (!history_expand(cmd_str, expanded, sizeof(expanded))) {
        return CSHELL_ERR_SYNTAX;
    }

    token_list_t *tokens = lexer_tokenize(expanded);
    if (!tokens) return CSHELL_ERR_SYNTAX;

    command_line_t *cmd_line = parser_parse(tokens);
    token_list_free(tokens);

    if (!cmd_line) {
        return CSHELL_ERR_SYNTAX;
    }

    int status = executor_execute_line(cmd_line);
    command_line_free(cmd_line);
    return status;
}

int cshell_execute_script(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "cshell: cannot open script file '%s'\n", filepath);
        return CSHELL_ERR_NOT_FOUND;
    }

    char line[CSHELL_MAX_INPUT];
    int status = CSHELL_SUCCESS;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        /* Skip leading whitespace */
        char *start = line;
        while (isspace((unsigned char)*start)) start++;

        /* Skip empty lines and comments */
        if (*start == '\0' || *start == '#') continue;

        status = cshell_execute_string(start);
        if (g_shell.should_exit) break;
    }

    fclose(f);
    return status;
}

int cshell_run_repl(void) {
    char line_buf[CSHELL_MAX_INPUT];

    while (!g_shell.should_exit) {
        cshell_check_jobs();
        cshell_update_prompt();
        printf("%s", g_shell.prompt_str);
        fflush(stdout);

        if (!fgets(line_buf, sizeof(line_buf), stdin)) {
            /* EOF reached (Ctrl+D) */
            printf("\nexit\n");
            break;
        }

        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r')) {
            line_buf[--len] = '\0';
        }

        /* Check for empty line */
        char *p = line_buf;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') continue;

        /* Add unexpanded command to history */
        history_add(line_buf);

        /* Execute */
        cshell_execute_string(line_buf);
    }

    return g_shell.last_exit_code;
}
