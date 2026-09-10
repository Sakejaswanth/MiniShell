/**
 * @file platform_win32.c
 * @brief Windows-native process management, anonymous pipes, and console configuration.
 */

#if defined(_WIN32) || defined(_WIN64)

#include "platform.h"
#include "cshell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <direct.h>

static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            /* Ignore Ctrl+C in shell so it doesn't kill cshell REPL */
            printf("\n");
            cshell_update_prompt();
            printf("%s", g_shell.prompt_str);
            fflush(stdout);
            return TRUE;
        default:
            return FALSE;
    }
}

void platform_init(void) {
    /* Enable Virtual Terminal Processing for ANSI colors */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
}

void platform_setup_signals(void) {
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
}

char *platform_get_cwd(char *buf, size_t size) {
    return _getcwd(buf, (int)size);
}

int platform_change_dir(const char *path) {
    return _chdir(path);
}

char *platform_get_username(char *buf, size_t size) {
    DWORD dwSize = (DWORD)size;
    if (GetUserNameA(buf, &dwSize)) {
        return buf;
    }
    const char *env_user = getenv("USERNAME");
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
    DWORD dwSize = (DWORD)size;
    if (GetComputerNameA(buf, &dwSize)) {
        return buf;
    }
    const char *env_comp = getenv("COMPUTERNAME");
    if (env_comp) {
        strncpy(buf, env_comp, size - 1);
        buf[size - 1] = '\0';
        return buf;
    }
    strncpy(buf, "cshell-host", size - 1);
    buf[size - 1] = '\0';
    return buf;
}

char *platform_find_in_path(const char *cmd) {
    if (!cmd) return NULL;
    char buffer[MAX_PATH];
    char *file_part = NULL;

    /* Check directly */
    DWORD res = SearchPathA(NULL, cmd, ".exe", MAX_PATH, buffer, &file_part);
    if (res > 0 && res < MAX_PATH) {
        return _strdup(buffer);
    }

    res = SearchPathA(NULL, cmd, ".cmd", MAX_PATH, buffer, &file_part);
    if (res > 0 && res < MAX_PATH) {
        return _strdup(buffer);
    }

    res = SearchPathA(NULL, cmd, ".bat", MAX_PATH, buffer, &file_part);
    if (res > 0 && res < MAX_PATH) {
        return _strdup(buffer);
    }

    return NULL;
}

int platform_setenv(const char *name, const char *value) {
    if (!name) return -1;
    if (!value) value = "";
    SetEnvironmentVariableA(name, value);
    return _putenv_s(name, value);
}

int platform_unsetenv(const char *name) {
    if (!name) return -1;
    SetEnvironmentVariableA(name, NULL);
    return _putenv_s(name, "");
}

bool platform_pipe_create(platform_pipe_t *p) {
    if (!p) return false;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&p->read_handle, &p->write_handle, &sa, 0)) {
        return false;
    }
    return true;
}

void platform_pipe_close_read(platform_pipe_t *p) {
    if (p && p->read_handle != INVALID_HANDLE_VALUE && p->read_handle != NULL) {
        CloseHandle(p->read_handle);
        p->read_handle = INVALID_HANDLE_VALUE;
    }
}

void platform_pipe_close_write(platform_pipe_t *p) {
    if (p && p->write_handle != INVALID_HANDLE_VALUE && p->write_handle != NULL) {
        CloseHandle(p->write_handle);
        p->write_handle = INVALID_HANDLE_VALUE;
    }
}

platform_file_handle_t platform_open_read(const char *path) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE h = CreateFileA(path,
                           GENERIC_READ,
                           FILE_SHARE_READ,
                           &sa,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL);
    return h;
}

platform_file_handle_t platform_open_write(const char *path, bool append) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE h = CreateFileA(path,
                           GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           &sa,
                           append ? OPEN_ALWAYS : CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL);

    if (h != INVALID_HANDLE_VALUE && append) {
        SetFilePointer(h, 0, NULL, FILE_END);
    }
    return h;
}

void platform_close_handle(platform_file_handle_t handle) {
    if (handle != INVALID_HANDLE_VALUE && handle != NULL) {
        CloseHandle(handle);
    }
}

/* Construct command-line string with Windows escaping */
static void build_windows_command_line(command_t *cmd, char *out_cmdline, size_t max_len) {
    out_cmdline[0] = '\0';
    for (int i = 0; i < cmd->argc; i++) {
        if (i > 0) {
            strncat(out_cmdline, " ", max_len - strlen(out_cmdline) - 1);
        }
        const char *arg = cmd->argv[i];
        bool needs_quotes = (strchr(arg, ' ') != NULL || strchr(arg, '\t') != NULL || arg[0] == '\0');

        if (needs_quotes) {
            strncat(out_cmdline, "\"", max_len - strlen(out_cmdline) - 1);
        }
        strncat(out_cmdline, arg, max_len - strlen(out_cmdline) - 1);
        if (needs_quotes) {
            strncat(out_cmdline, "\"", max_len - strlen(out_cmdline) - 1);
        }
    }
}

int platform_spawn(command_t *cmd,
                   platform_file_handle_t in_handle,
                   platform_file_handle_t out_handle,
                   bool is_background,
                   platform_proc_t *out_proc) {
    if (!cmd || cmd->argc == 0) return CSHELL_ERR_GENERIC;

    char cmdline[4096];
    build_windows_command_line(cmd, cmdline, sizeof(cmdline));

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    /* Direct or inherited std handles */
    si.hStdInput = (in_handle != INVALID_HANDLE_VALUE) ? in_handle : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = (out_handle != INVALID_HANDLE_VALUE) ? out_handle : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    /* Ensure handles are inheritable */
    if (si.hStdInput != INVALID_HANDLE_VALUE) SetHandleInformation(si.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    if (si.hStdOutput != INVALID_HANDLE_VALUE) SetHandleInformation(si.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    if (si.hStdError != INVALID_HANDLE_VALUE) SetHandleInformation(si.hStdError, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    ZeroMemory(&pi, sizeof(pi));

    DWORD creation_flags = 0;
    if (is_background) {
        /* Runs without stealing focus */
        creation_flags |= CREATE_NEW_PROCESS_GROUP;
    }

    /* Try spawning directly */
    BOOL success = CreateProcessA(NULL,
                                  cmdline,
                                  NULL,
                                  NULL,
                                  TRUE,   /* inherit handles */
                                  creation_flags,
                                  NULL,
                                  NULL,
                                  &si,
                                  &pi);

    /* If direct execution fails, try fallback through cmd.exe /c for shell built-ins like dir, copy */
    if (!success) {
        char fallback[4096 + 64];
        snprintf(fallback, sizeof(fallback), "cmd.exe /c %s", cmdline);
        success = CreateProcessA(NULL,
                                 fallback,
                                 NULL,
                                 NULL,
                                 TRUE,
                                 creation_flags,
                                 NULL,
                                 NULL,
                                 &si,
                                 &pi);
    }

    if (!success) {
        fprintf(stderr, "cshell: command not found or execution failed: %s\n", cmd->argv[0]);
        return CSHELL_ERR_NOT_FOUND;
    }

    /* Close unused thread handle */
    CloseHandle(pi.hThread);

    if (out_proc) {
        out_proc->process_handle = pi.hProcess;
        out_proc->process_id = pi.dwProcessId;
    } else {
        CloseHandle(pi.hProcess);
    }

    return CSHELL_SUCCESS;
}

int platform_wait(platform_proc_t *proc) {
    if (!proc || proc->process_handle == INVALID_HANDLE_VALUE || proc->process_handle == NULL) {
        return CSHELL_ERR_GENERIC;
    }

    WaitForSingleObject(proc->process_handle, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(proc->process_handle, &exit_code);
    CloseHandle(proc->process_handle);
    proc->process_handle = INVALID_HANDLE_VALUE;
    return (int)exit_code;
}

bool platform_is_process_running(platform_proc_t *proc) {
    if (!proc || proc->process_handle == INVALID_HANDLE_VALUE || proc->process_handle == NULL) {
        return false;
    }
    DWORD res = WaitForSingleObject(proc->process_handle, 0);
    return (res == WAIT_TIMEOUT);
}

#endif /* _WIN32 */
