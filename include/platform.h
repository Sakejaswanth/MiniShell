/**
 * @file platform.h
 * @brief Cross-platform abstraction interface for process control, pipes, and OS environment.
 */

#ifndef CSHELL_PLATFORM_H
#define CSHELL_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include "parser.h"

#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS 1
#include <windows.h>
#include <process.h>
#include <direct.h>
#include <io.h>

typedef HANDLE platform_file_handle_t;
#define PLATFORM_INVALID_HANDLE INVALID_HANDLE_VALUE

typedef struct {
    HANDLE read_handle;
    HANDLE write_handle;
} platform_pipe_t;

typedef struct {
    HANDLE process_handle;
    DWORD  process_id;
} platform_proc_t;

#else
#define PLATFORM_POSIX 1
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

typedef int platform_file_handle_t;
#define PLATFORM_INVALID_HANDLE -1

typedef struct {
    int read_fd;
    int write_fd;
} platform_pipe_t;

typedef struct {
    pid_t pid;
} platform_proc_t;

#endif

/* Cross-platform API */
void platform_init(void);
void platform_setup_signals(void);

/* Directory & Environment */
char *platform_get_cwd(char *buf, size_t size);
int   platform_change_dir(const char *path);
char *platform_get_username(char *buf, size_t size);
char *platform_get_hostname(char *buf, size_t size);
char *platform_find_in_path(const char *cmd);
int   platform_setenv(const char *name, const char *value);
int   platform_unsetenv(const char *name);

/* IPC Pipes */
bool platform_pipe_create(platform_pipe_t *p);
void platform_pipe_close_read(platform_pipe_t *p);
void platform_pipe_close_write(platform_pipe_t *p);

/* File redirection */
platform_file_handle_t platform_open_read(const char *path);
platform_file_handle_t platform_open_write(const char *path, bool append);
void platform_close_handle(platform_file_handle_t handle);

/* Process execution */
int  platform_spawn(command_t *cmd,
                    platform_file_handle_t in_handle,
                    platform_file_handle_t out_handle,
                    bool is_background,
                    platform_proc_t *out_proc);

int  platform_wait(platform_proc_t *proc);
bool platform_is_process_running(platform_proc_t *proc);

#endif /* CSHELL_PLATFORM_H */
