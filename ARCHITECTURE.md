# C-Shell Architecture & Operating Systems Design Document

This document outlines the internal architecture, operating systems principles, and low-level system design behind **C-Shell (`cshell`)**.

---

## 1. System Overview & Lifecycle

C-Shell operates on a classic **REPL (Read-Eval-Print Loop)** pattern:

```
+-------------------------------------------------------------------+
|                        1. Read & Expansion                       |
|   Prompt Display -> Input Buffer -> History Expansion (!! / !n)   |
+---------------------------------+---------------------------------+
                                  |
                                  v
+---------------------------------+---------------------------------+
|                       2. Lexical Analysis                         |
|   Character Stream -> Tokenizer -> Dynamic Token Array            |
|   (Identifies words, quotes, escapes, |, <, >, >>, &, ;)          |
+---------------------------------+---------------------------------+
                                  |
                                  v
+---------------------------------+---------------------------------+
|                     3. Abstract Syntax Tree                       |
|   Token Array -> Parser -> Command Line AST                       |
|   (Sequences of Pipelines containing Commands with Redirections)  |
+---------------------------------+---------------------------------+
                                  |
                                  v
+---------------------------------+---------------------------------+
|                       4. Execution Engine                         |
|   Dispatch: Built-in vs External Process                          |
|   IPC Pipe Setup -> File Redirection -> Process Spawning          |
|   Foreground Wait vs Background Job Tracking                      |
+---------------------------------+---------------------------------+
                                  |
                                  v
+---------------------------------+---------------------------------+
|                      5. Memory Deallocation                       |
|   Free AST Nodes, Token Buffers, and Reset Execution State        |
+-------------------------------------------------------------------+
```

---

## 2. Subsystems Detail

### A. Lexer / Tokenizer (`src/lexer.c`)
- **Finite-State Character Scanner**: Reads raw user input character-by-character.
- **Quote Processing**:
  - Double quotes (`"..."`): Groups multi-word arguments while allowing backslash escape sequences.
  - Single quotes (`'...'`): Treats all enclosed characters literally.
- **Operator Separation**: Delimiters (`|`, `<`, `>`, `>>`, `&`, `;`) are separated from neighboring arguments even without spaces (e.g., `cat<in.txt` parses into `["cat", "<", "in.txt"]`).
- **Dynamic Allocation**: Tokens and their inner string buffers are allocated dynamically on the heap and freed in bulk via `token_list_free()`.

### B. Command Parser & AST (`src/parser.c`)
The parser converts flat token arrays into a hierarchical tree:
- **`command_line_t`**: Root node containing an array of `pipeline_t*`.
- **`pipeline_t`**: Represents a pipeline stage `cmd1 | cmd2 | ... | cmdN`, along with an `is_background` flag (`&`).
- **`command_t`**: Holds individual arguments (`argv[]`), `input_file` (`<`), `output_file` (`>` or `>>`), and `append_output` flag.

```
command_line_t
  └── pipelines[0]
        ├── commands[0] ("cat", input_file: "input.txt")
        │     |
        │   [PIPE 0]
        │     v
        └── commands[1] ("grep foo", output_file: "out.log", append: true)
```

### C. Execution Engine & IPC (`src/executor.c`)

#### Built-in vs External Dispatch
- **Parent Process Built-ins**: Commands that modify shell state (such as `cd`, `setenv`, `export`, `exit`) **must** run in the parent process. Spawning a child process for `cd` would change the directory of the child, leaving the shell unchanged.
- **Temporary Built-in Redirection**: When a built-in uses redirection (e.g. `help > manual.txt`), the executor duplicates the standard stream handle, redirects it to the file, runs the built-in, and restores standard I/O.

#### Multi-Stage Pipelines (`|`)
For an $N$-command pipeline:
1. Exactly $N - 1$ anonymous pipes are created.
2. Command $0$'s stdout connects to Pipe $0$'s write end.
3. Command $i$'s stdin connects to Pipe $i-1$'s read end, and stdout connects to Pipe $i$'s write end.
4. Command $N-1$'s stdin connects to Pipe $N-2$'s read end.
5. **Critical OS Rule**: Every pipe write end **must** be closed in the parent shell after handing it to the child; otherwise, readers will never see EOF and the pipeline will hang indefinitely.

---

## 3. Platform Abstraction Layer (`src/platform_*.c`)

To achieve maximum portability and avoid platform-dependent lock-in, all operating system primitives are isolated behind `include/platform.h`:

| Feature | POSIX (`src/platform_posix.c`) | Windows (`src/platform_win32.c`) |
| :--- | :--- | :--- |
| **Process Creation** | `fork()` + `execvp()` | `CreateProcessA()` |
| **Pipes** | `pipe(int fds[2])` | `CreatePipe(HANDLE*, HANDLE*, ...)` |
| **I/O Redirection** | `dup2(fd, STDIN/OUT)` | `STARTUPINFOA.hStdInput/Output` |
| **Process Waiting** | `waitpid(pid, &status, 0)` | `WaitForSingleObject(hProcess, INFINITE)` |
| **Non-blocking Status**| `waitpid(..., WNOHANG)` | `WaitForSingleObject(..., 0) == WAIT_TIMEOUT` |
| **Ctrl+C Handling** | `sigaction(SIGINT, ...)` | `SetConsoleCtrlHandler(...)` |
| **Terminal ANSI Color**| Native support | `ENABLE_VIRTUAL_TERMINAL_PROCESSING` |

---

## 4. Background Job Management

1. When a command ends with `&`, the executor launches the child process without waiting synchronously.
2. The process PID and command line are saved into `g_shell.jobs[]`.
3. Before every prompt in the REPL (`cshell_run_repl`), `cshell_check_jobs()` performs non-blocking polling:
   - On POSIX: `waitpid(pid, &status, WNOHANG)`.
   - On Windows: `WaitForSingleObject(handle, 0) == WAIT_OBJECT_0`.
4. Finished jobs are printed with `[id]+ Done <cmd>` and reaped from the table.

---

## 5. Memory Management & Safety

- No static buffer overflow vulnerabilities: Dynamic allocation is used for all tokens and argument vectors.
- Recursive memory cleanup functions:
  - `token_list_free()`
  - `command_free()`
  - `pipeline_free()`
  - `command_line_free()`
- Guaranteed handle/file descriptor cleanup in all pipeline stages prevents OS descriptor leaks.
