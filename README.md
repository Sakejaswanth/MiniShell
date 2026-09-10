# C-Shell (`cshell`)

![C99](https://img.shields.io/badge/Language-C99-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-brightgreen.svg)
![Build](https://img.shields.io/badge/Build-CMake%20%7C%20Make%20%7C%20batch-orange.svg)
![Tests](https://img.shields.io/badge/Tests-Passing-success.svg)

A modular, cross-platform Unix-compatible command-line interpreter (Shell) written in **C**. Designed to demonstrate fundamental Operating Systems principles including tokenization, AST parsing, process control, Inter-Process Communication (IPC via anonymous pipes), I/O redirection, and background job management.

---

## Key Features

- **Modular Architecture**: Separate subsystems for Lexical Analysis, AST Parsing, Built-in Command Handling, Process Execution, and History Tracking.
- **Cross-Platform Compatibility**:
  - **Windows**: Implemented using the Win32 API (`CreateProcess`, `CreatePipe`, `DuplicateHandle`, console virtual terminal processing).
  - **POSIX (Linux, macOS, WSL)**: Implemented using standard Unix system calls (`fork`, `execvp`, `pipe`, `dup2`, `waitpid`, `sigaction`).
- **Interactive REPL**:
  - Colored prompt with `username@hostname:cwd$`.
  - Automatic `~` home directory path substitution.
  - Signal handling (`Ctrl+C` cancels current input line without terminating the shell).
- **Pipelines (`|`)**: Arbitrary multi-stage piping (`cmd1 | cmd2 | cmd3 | ...`).
- **I/O Redirection**:
  - Standard Input redirection: `cmd < input.txt`
  - Standard Output redirection (overwrite): `cmd > output.txt`
  - Standard Output redirection (append): `cmd >> append.txt`
- **Background Processes (`&`)**:
  - Asynchronous background execution: `long_process &`
  - Built-in `jobs` tracker monitoring active and completed jobs (`[1]+ Done`).
- **Command Sequencing (`;`)**: Run multiple commands sequentially: `cmd1 ; cmd2 ; cmd3`.
- **Command History**:
  - In-memory ring buffer with duplicate suppression.
  - Persistent history across sessions saved to `~/.cshell_history`.
  - Event expansion: `!!` (re-run last command) and `!n` (re-run command number `n`).
- **Rich Built-in Commands**:
  - `cd [dir | ~ | -]`: Directory navigation with home and previous directory tracking.
  - `pwd`: Print current working directory.
  - `echo [-n] [args...]`: Text printing with escape sequence support (`\n`, `\t`).
  - `history [n]`: View recent command history.
  - `type [cmd]`: Determine whether command is built-in or locate path in `$PATH`.
  - `export KEY=VAL` / `setenv KEY [VAL]`: Environment variable management.
  - `unsetenv KEY`: Remove environment variable.
  - `jobs`: List background jobs and status.
  - `clear`: Terminal screen clearing.
  - `help`: Interactive manual of all built-in commands.
  - `exit [code]`: Exit shell with status code.
- **Batch Script Runner**: Run scripts with `./cshell script.csh` (supports comments with `#`).

---

## Directory Structure

```
C-shell/
├── include/                 # Header declarations
│   ├── cshell.h             # Core types, state, and constants
│   ├── lexer.h              # Tokenizer interface
│   ├── parser.h             # AST and grammar definitions
│   ├── builtin.h            # Shell built-in declarations
│   ├── executor.h           # Pipeline execution engine
│   ├── history.h            # History tracking and expansion
│   └── platform.h           # Cross-platform OS abstraction layer
├── src/                     # Implementation files
│   ├── main.c               # REPL loop, argument parsing, banner
│   ├── lexer.c              # Lexical scanner with quotes & escape handling
│   ├── parser.c             # Command AST generator
│   ├── builtin.c            # Implementations of all built-in commands
│   ├── executor.c           # Pipeline orchestrator and handle manager
│   ├── history.c            # History persistence and ring buffer
│   ├── platform_win32.c     # Windows Win32 API process and pipe layer
│   └── platform_posix.c     # POSIX fork/exec/pipe/signal layer
├── tests/                   # Test suite
│   ├── test_runner.c        # Automated unit and integration test runner
│   └── sample_script.csh    # Sample batch script for verification
├── CMakeLists.txt           # Modern CMake configuration
├── Makefile                 # Portable Makefile for GCC/Clang/MinGW
├── build.bat                # One-click Windows build script
├── ARCHITECTURE.md          # In-depth OS design & architecture documentation
└── README.md                # Project documentation
```

---

## Building and Compiling

### Windows (Quick Build)

Double-click `build.bat` or run in Command Prompt / PowerShell:
```cmd
.\build.bat
```
The script will automatically detect Clang, GCC, MSVC (`cl.exe`), or CMake and produce `bin\cshell.exe` and `bin\cshell_tests.exe`.

### Using CMake (Universal)

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```
Binaries will be placed in `bin/`.

### Using Make (Linux / macOS / MinGW)

```bash
make
```
To run the automated test suite:
```bash
make test
```

---

## Usage Guide

### 1. Interactive Mode
Run the executable directly:
```bash
./bin/cshell
```
You will be greeted with the C-Shell banner and the interactive prompt:
```
  ____       ____  _          _ _ 
 / ___|     / ___|| |__   ___| | |
| |   _____ \___ \| '_ \ / _ \ | |
| |__|_____| ___) | | | |  __/ | |
 \____|     |____/|_| |_|\___|_|_|
 C-Shell v1.0.0 - Modular Operating Systems Shell
 Type 'help' for built-in commands or 'exit' to quit.

user@host:~/C-shell$ 
```

### 2. Running Commands & Pipelines
```bash
# Basic commands
cshell$ echo "Hello from C-Shell!"
cshell$ pwd
cshell$ cd .. ; pwd

# Multi-stage pipelines
cshell$ echo "zebra\napple\nbanana" | sort

# I/O Redirection
cshell$ echo "Logged line" > output.txt
cshell$ cat < output.txt
cshell$ echo "Appended line" >> output.txt

# Background execution
cshell$ ping 127.0.0.1 &
[1] 14208
cshell$ jobs
[1]  PID: 14208     Status: Running     Command: ping

# Command history and event expansion
cshell$ history 5
cshell$ !!
cshell$ !1
```

### 3. One-off Command Execution
```bash
./bin/cshell -c "echo 'Running directly via -c flag'"
```

### 4. Running Shell Scripts
```bash
./bin/cshell tests/sample_script.csh
```

---

## Running the Automated Tests

Run the built-in test runner:
```bash
./bin/cshell_tests
```
Example output:
```
=========================================
     C-Shell Automated Test Suite        
=========================================

--- Running Lexer Tests ---
  [PASS] Tokenize basic words
  [PASS] Token count for 'ls -la /tmp'
  [PASS] First word is 'ls'
  [PASS] Tokenize pipeline and redirects
  [PASS] Tokenize quoted strings
  [PASS] Double quoted preserved spaces
  [PASS] Single quoted preserved spaces
  [PASS] Tokenize semicolon

--- Running Parser Tests ---
  [PASS] Parse basic command
  [PASS] Pipeline count is 1
  [PASS] Parse pipeline with redirection
  [PASS] Background flag is set

--- Running Builtin Command Tests ---
  [PASS] 'cd' recognized as built-in
  [PASS] 'pwd' recognized as built-in
  [PASS] 'echo' recognized as built-in
  [PASS] setenv succeeds
  [PASS] Environment variable was stored
  [PASS] unsetenv succeeds

--- Running History Tests ---
  [PASS] History starts empty
  [PASS] History count is 3
  [PASS] Expansion of '!!' returns last command
  [PASS] Expansion of '!1' returns first command

=========================================
 Test Results: 20/20 Passed (0 Failed)
=========================================
```

---

## License
MIT License. Open-source educational and portfolio software.
