/**
 * @file executor.c
 * @brief Execution engine for commands, pipelines, I/O redirection, and background jobs.
 */

#include "executor.h"
#include "builtin.h"
#include "cshell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#else
#include <unistd.h>
#endif

/* Execute built-in with temporary handle redirection if required */
static int execute_builtin_with_redirection(command_t *cmd) {
    platform_file_handle_t saved_in = PLATFORM_INVALID_HANDLE;
    platform_file_handle_t saved_out = PLATFORM_INVALID_HANDLE;
    platform_file_handle_t in_h = PLATFORM_INVALID_HANDLE;
    platform_file_handle_t out_h = PLATFORM_INVALID_HANDLE;

#if defined(_WIN32) || defined(_WIN64)
    int saved_stdout_fd = -1;
    int saved_stdin_fd = -1;

    if (cmd->output_file) {
        fflush(stdout);
        out_h = platform_open_write(cmd->output_file, cmd->append_output);
        if (out_h == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "cshell: cannot open output file '%s'\n", cmd->output_file);
            return CSHELL_ERR_GENERIC;
        }
        saved_out = GetStdHandle(STD_OUTPUT_HANDLE);
        SetStdHandle(STD_OUTPUT_HANDLE, out_h);
        saved_stdout_fd = _dup(1);
        int out_fd = _open_osfhandle((intptr_t)out_h, 0);
        if (out_fd >= 0) {
            _dup2(out_fd, 1);
            _close(out_fd);
        }
    }

    if (cmd->input_file) {
        fflush(stdin);
        in_h = platform_open_read(cmd->input_file);
        if (in_h == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "cshell: cannot open input file '%s'\n", cmd->input_file);
            if (saved_stdout_fd >= 0) {
                _dup2(saved_stdout_fd, 1);
                _close(saved_stdout_fd);
                SetStdHandle(STD_OUTPUT_HANDLE, saved_out);
            }
            return CSHELL_ERR_GENERIC;
        }
        saved_in = GetStdHandle(STD_INPUT_HANDLE);
        SetStdHandle(STD_INPUT_HANDLE, in_h);
        saved_stdin_fd = _dup(0);
        int in_fd = _open_osfhandle((intptr_t)in_h, 0);
        if (in_fd >= 0) {
            _dup2(in_fd, 0);
            _close(in_fd);
        }
    }
#else
    if (cmd->input_file) {
        in_h = platform_open_read(cmd->input_file);
        if (in_h < 0) {
            perror("cshell: open input file");
            return CSHELL_ERR_GENERIC;
        }
        saved_in = dup(STDIN_FILENO);
        dup2(in_h, STDIN_FILENO);
        close(in_h);
    }
    if (cmd->output_file) {
        out_h = platform_open_write(cmd->output_file, cmd->append_output);
        if (out_h < 0) {
            perror("cshell: open output file");
            if (saved_in >= 0) {
                dup2(saved_in, STDIN_FILENO);
                close(saved_in);
            }
            return CSHELL_ERR_GENERIC;
        }
        saved_out = dup(STDOUT_FILENO);
        dup2(out_h, STDOUT_FILENO);
        close(out_h);
    }
#endif

    int result = builtin_execute(cmd->argc, cmd->argv);

    /* Restore std handles/descriptors */
#if defined(_WIN32) || defined(_WIN64)
    fflush(stdout);
    if (saved_stdout_fd >= 0) {
        _dup2(saved_stdout_fd, 1);
        _close(saved_stdout_fd);
    }
    if (saved_out != PLATFORM_INVALID_HANDLE) {
        SetStdHandle(STD_OUTPUT_HANDLE, saved_out);
    }

    if (saved_stdin_fd >= 0) {
        _dup2(saved_stdin_fd, 0);
        _close(saved_stdin_fd);
    }
    if (saved_in != PLATFORM_INVALID_HANDLE) {
        SetStdHandle(STD_INPUT_HANDLE, saved_in);
    }
#else
    if (saved_in >= 0) {
        dup2(saved_in, STDIN_FILENO);
        close(saved_in);
    }
    if (saved_out >= 0) {
        dup2(saved_out, STDOUT_FILENO);
        close(saved_out);
    }
#endif

    return result;
}

int executor_execute_single_external(command_t *cmd, bool is_background) {
    platform_file_handle_t in_handle = PLATFORM_INVALID_HANDLE;
    platform_file_handle_t out_handle = PLATFORM_INVALID_HANDLE;

    if (cmd->input_file) {
        in_handle = platform_open_read(cmd->input_file);
        if (in_handle == PLATFORM_INVALID_HANDLE) {
            fprintf(stderr, "cshell: cannot open '%s' for input\n", cmd->input_file);
            return CSHELL_ERR_GENERIC;
        }
    }

    if (cmd->output_file) {
        out_handle = platform_open_write(cmd->output_file, cmd->append_output);
        if (out_handle == PLATFORM_INVALID_HANDLE) {
            fprintf(stderr, "cshell: cannot open '%s' for output\n", cmd->output_file);
            platform_close_handle(in_handle);
            return CSHELL_ERR_GENERIC;
        }
    }

    platform_proc_t proc;
    int status = platform_spawn(cmd, in_handle, out_handle, is_background, &proc);

    /* Close file handles in parent */
    platform_close_handle(in_handle);
    platform_close_handle(out_handle);

    if (status != CSHELL_SUCCESS) {
        return status;
    }

    if (is_background) {
#if defined(_WIN32) || defined(_WIN64)
        unsigned long pid = proc.process_id;
#else
        unsigned long pid = (unsigned long)proc.pid;
#endif
        int job_id = cshell_add_job(pid, cmd->argv[0]);
        printf("[%d] %lu\n", job_id, pid);
        return CSHELL_SUCCESS;
    } else {
        return platform_wait(&proc);
    }
}

int executor_execute_pipeline(pipeline_t *pipeline) {
    if (!pipeline || pipeline->command_count == 0) {
        return CSHELL_SUCCESS;
    }

    /* Single command case */
    if (pipeline->command_count == 1) {
        command_t *cmd = pipeline->commands[0];
        if (cmd->argc == 0) return CSHELL_SUCCESS;

        if (builtin_is_builtin(cmd->argv[0])) {
            return execute_builtin_with_redirection(cmd);
        } else {
            return executor_execute_single_external(cmd, pipeline->is_background);
        }
    }

    /* Multi-command pipeline: cmd0 | cmd1 | ... | cmdN-1 */
    int n = pipeline->command_count;
    platform_pipe_t *pipes = (platform_pipe_t *)malloc((n - 1) * sizeof(platform_pipe_t));
    if (!pipes) {
        fprintf(stderr, "cshell: memory allocation failed for pipes\n");
        return CSHELL_ERR_GENERIC;
    }

    /* Create all IPC pipes */
    for (int i = 0; i < n - 1; i++) {
        if (!platform_pipe_create(&pipes[i])) {
            fprintf(stderr, "cshell: failed to create pipeline\n");
            for (int j = 0; j < i; j++) {
                platform_pipe_close_read(&pipes[j]);
                platform_pipe_close_write(&pipes[j]);
            }
            free(pipes);
            return CSHELL_ERR_GENERIC;
        }
    }

    platform_proc_t *procs = (platform_proc_t *)malloc(n * sizeof(platform_proc_t));
    int final_status = CSHELL_SUCCESS;

    for (int i = 0; i < n; i++) {
        command_t *cmd = pipeline->commands[i];
        platform_file_handle_t in_h = PLATFORM_INVALID_HANDLE;
        platform_file_handle_t out_h = PLATFORM_INVALID_HANDLE;

        /* Input handle */
        if (i == 0) {
            if (cmd->input_file) {
                in_h = platform_open_read(cmd->input_file);
            }
        } else {
#if defined(_WIN32) || defined(_WIN64)
            in_h = pipes[i - 1].read_handle;
#else
            in_h = pipes[i - 1].read_fd;
#endif
        }

        /* Output handle */
        if (i == n - 1) {
            if (cmd->output_file) {
                out_h = platform_open_write(cmd->output_file, cmd->append_output);
            }
        } else {
#if defined(_WIN32) || defined(_WIN64)
            out_h = pipes[i].write_handle;
#else
            out_h = pipes[i].write_fd;
#endif
        }

        platform_spawn(cmd, in_h, out_h, pipeline->is_background, &procs[i]);

        /* Close handles that are now handed off */
        if (i == 0 && cmd->input_file) {
            platform_close_handle(in_h);
        }
        if (i > 0) {
            platform_pipe_close_read(&pipes[i - 1]);
        }
        if (i < n - 1) {
            platform_pipe_close_write(&pipes[i]);
        }
        if (i == n - 1 && cmd->output_file) {
            platform_close_handle(out_h);
        }
    }

    /* Wait for processes if in foreground */
    if (!pipeline->is_background) {
        for (int i = 0; i < n; i++) {
            int ret = platform_wait(&procs[i]);
            if (i == n - 1) {
                final_status = ret;
            }
        }
    } else {
#if defined(_WIN32) || defined(_WIN64)
        unsigned long pid = procs[n - 1].process_id;
#else
        unsigned long pid = (unsigned long)procs[n - 1].pid;
#endif
        int job_id = cshell_add_job(pid, pipeline->commands[0]->argv[0]);
        printf("[%d] %lu\n", job_id, pid);
    }

    free(pipes);
    free(procs);
    return final_status;
}

int executor_execute_line(command_line_t *cmd_line) {
    if (!cmd_line) return CSHELL_SUCCESS;

    int last_status = CSHELL_SUCCESS;
    for (int i = 0; i < cmd_line->pipeline_count; i++) {
        if (g_shell.should_exit) break;
        last_status = executor_execute_pipeline(cmd_line->pipelines[i]);
        g_shell.last_exit_code = last_status;
    }
    return last_status;
}
