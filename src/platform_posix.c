/**
 * @file platform_posix.c
 * @brief POSIX-native process management, pipes, signals, and redirection.
 */

#if !defined(_WIN32) && !defined(_WIN64)

#include "platform.h"
#include "cshell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

static void sigint_handler(int sig) {
    (void)sig;
    printf("\n");
    cshell_update_prompt();
    printf("%s", g_shell.prompt_str);
    fflush(stdout);
}

void platform_init(void) {
    /* No special terminal init required on standard POSIX */
}

void platform_setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    /* Ignore SIGTSTP (Ctrl+Z) in shell prompt */
    signal(SIGTSTP, SIG_IGN);
}

char *platform_get_cwd(char *buf, size_t size) {
    return getcwd(buf, size);
}

int platform_change_dir(const char *path) {
    return chdir(path);
}

char *platform_get_username(char *buf, size_t size) {
    char *user = getlogin();
    if (user) {
        strncpy(buf, user, size - 1);
        buf[size - 1] = '\0';
        return buf;
    }
    const char *env_user = getenv("USER");
    if (env_user) {
        strncpy(buf, env_user, size - 1);
        buf[size - 1] = '\0';
        return buf;
    }
    strncpy(buf, "user", size - 1);
    buf[size - 1] = '\0';
    return buf;
}

char *platform_get_hostname(char *buf, size_t size) {
    if (gethostname(buf, size) == 0) {
        return buf;
    }
    strncpy(buf, "cshell-host", size - 1);
    buf[size - 1] = '\0';
    return buf;
}

char *platform_find_in_path(const char *cmd) {
    if (!cmd) return NULL;
    if (strchr(cmd, '/')) {
        if (access(cmd, X_OK) == 0) {
            return strdup(cmd);
        }
        return NULL;
    }

    const char *path_env = getenv("PATH");
    if (!path_env) return NULL;

    char *path_copy = strdup(path_env);
    if (!path_copy) return NULL;

    char *saveptr = NULL;
    char *dir = strtok_r(path_copy, ":", &saveptr);
    char full_path[1024];

    while (dir != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return strdup(full_path);
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
    return NULL;
}

int platform_setenv(const char *name, const char *value) {
    if (!name) return -1;
    return setenv(name, value ? value : "", 1);
}

int platform_unsetenv(const char *name) {
    if (!name) return -1;
    return unsetenv(name);
}

bool platform_pipe_create(platform_pipe_t *p) {
    if (!p) return false;
    int fds[2];
    if (pipe(fds) < 0) {
        return false;
    }
    p->read_fd = fds[0];
    p->write_fd = fds[1];
    return true;
}

void platform_pipe_close_read(platform_pipe_t *p) {
    if (p && p->read_fd >= 0) {
        close(p->read_fd);
        p->read_fd = -1;
    }
}

void platform_pipe_close_write(platform_pipe_t *p) {
    if (p && p->write_fd >= 0) {
        close(p->write_fd);
        p->write_fd = -1;
    }
}

platform_file_handle_t platform_open_read(const char *path) {
    return open(path, O_RDONLY);
}

platform_file_handle_t platform_open_write(const char *path, bool append) {
    int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
    return open(path, flags, 0644);
}

void platform_close_handle(platform_file_handle_t handle) {
    if (handle >= 0) {
        close(handle);
    }
}

int platform_spawn(command_t *cmd,
                   platform_file_handle_t in_handle,
                   platform_file_handle_t out_handle,
                   bool is_background,
                   platform_proc_t *out_proc) {
    if (!cmd || cmd->argc == 0) return CSHELL_ERR_GENERIC;

    pid_t pid = fork();
    if (pid < 0) {
        perror("cshell: fork failed");
        return CSHELL_ERR_GENERIC;
    }

    if (pid == 0) {
        /* Child process */
        if (in_handle >= 0) {
            dup2(in_handle, STDIN_FILENO);
            close(in_handle);
        }
        if (out_handle >= 0) {
            dup2(out_handle, STDOUT_FILENO);
            close(out_handle);
        }

        /* Reset signal handlers to default */
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        execvp(cmd->argv[0], cmd->argv);
        fprintf(stderr, "cshell: %s: command not found\n", cmd->argv[0]);
        exit(CSHELL_ERR_NOT_FOUND);
    }

    /* Parent process */
    if (out_proc) {
        out_proc->pid = pid;
    }

    return CSHELL_SUCCESS;
}

int platform_wait(platform_proc_t *proc) {
    if (!proc || proc->pid <= 0) return CSHELL_ERR_GENERIC;

    int status = 0;
    while (waitpid(proc->pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return CSHELL_ERR_GENERIC;
        }
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return CSHELL_SUCCESS;
}

bool platform_is_process_running(platform_proc_t *proc) {
    if (!proc || proc->pid <= 0) return false;
    int status;
    pid_t res = waitpid(proc->pid, &status, WNOHANG);
    return (res == 0);
}

#endif /* !defined(_WIN32) && !defined(_WIN64) */
