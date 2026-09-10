@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo             Building C-Shell (cshell)
echo ===================================================

if not exist bin mkdir bin

REM Check local zig install
if exist "%LOCALAPPDATA%\Programs\zig\zig-x86_64-windows-0.16.0\zig.exe" (
    set "PATH=%LOCALAPPDATA%\Programs\zig\zig-x86_64-windows-0.16.0;%PATH%"
)

REM Check for Zig cc
where zig >nul 2>nul
if %errorlevel% equ 0 (
    goto compile_zig
)

REM Check for LLVM / Clang
where clang >nul 2>nul
if %errorlevel% equ 0 (
    set CC=clang
    goto compile_cc
)

if exist "C:\Program Files\LLVM\bin\clang.exe" (
    set "CC=C:\Program Files\LLVM\bin\clang.exe"
    goto compile_cc
)

REM Check for GCC
where gcc >nul 2>nul
if %errorlevel% equ 0 (
    set CC=gcc
    goto compile_cc
)

REM Check for MSVC cl.exe
where cl >nul 2>nul
if %errorlevel% equ 0 (
    goto compile_msvc
)

REM Check for CMake
where cmake >nul 2>nul
if %errorlevel% equ 0 (
    goto compile_cmake
)

echo ERROR: No C compiler detected.
echo Please install Zig, Clang/LLVM, MinGW GCC, or Visual Studio Build Tools.
exit /b 1

:compile_zig
echo Compiling using zig cc...
zig cc -Wall -Wextra -Iinclude -std=c99 -O2 src\main.c src\cshell.c src\lexer.c src\parser.c src\builtin.c src\executor.c src\history.c src\platform_win32.c -o bin\cshell.exe
if %errorlevel% neq 0 (
    echo Build failed for cshell.exe!
    exit /b 1
)

zig cc -Wall -Wextra -Iinclude -std=c99 -O2 tests\test_runner.c src\cshell.c src\lexer.c src\parser.c src\builtin.c src\executor.c src\history.c src\platform_win32.c -o bin\cshell_tests.exe
if %errorlevel% neq 0 (
    echo Build failed for cshell_tests.exe!
    exit /b 1
)
goto finished

:compile_cc
echo Compiling using %CC%...
"%CC%" -Wall -Wextra -Iinclude -std=c99 -O2 src\main.c src\cshell.c src\lexer.c src\parser.c src\builtin.c src\executor.c src\history.c src\platform_win32.c -o bin\cshell.exe
if %errorlevel% neq 0 (
    echo Build failed for cshell.exe!
    exit /b 1
)

"%CC%" -Wall -Wextra -Iinclude -std=c99 -O2 tests\test_runner.c src\cshell.c src\lexer.c src\parser.c src\builtin.c src\executor.c src\history.c src\platform_win32.c -o bin\cshell_tests.exe
if %errorlevel% neq 0 (
    echo Build failed for cshell_tests.exe!
    exit /b 1
)
goto finished

:compile_msvc
echo Compiling with Microsoft Visual C++...
cl /nologo /O2 /Iinclude src\main.c src\cshell.c src\lexer.c src\parser.c src\builtin.c src\executor.c src\history.c src\platform_win32.c /Fe:bin\cshell.exe
cl /nologo /O2 /Iinclude tests\test_runner.c src\cshell.c src\lexer.c src\parser.c src\builtin.c src\executor.c src\history.c src\platform_win32.c /Fe:bin\cshell_tests.exe
del *.obj >nul 2>nul
goto finished

:compile_cmake
echo Building with CMake...
cmake -B build
if %errorlevel% neq 0 (
    echo CMake configure failed.
    exit /b 1
)
cmake --build build --config Release
if %errorlevel% neq 0 (
    echo CMake build failed.
    exit /b 1
)
goto finished

:finished
echo.
echo ===================================================
echo            Build Succeeded!
echo  Binaries created:
echo    - bin\cshell.exe       (Shell executable)
echo    - bin\cshell_tests.exe (Test runner)
echo ===================================================
echo.
