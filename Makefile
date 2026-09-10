# C-Shell (cshell) Makefile

CC ?= gcc
CFLAGS ?= -Wall -Wextra -pedantic -std=c99 -Iinclude -O2
BIN_DIR = bin
SRC_DIR = src
TEST_DIR = tests

# Platform detection
ifeq ($(OS),Windows_NT)
    TARGET = $(BIN_DIR)/cshell.exe
    TEST_TARGET = $(BIN_DIR)/cshell_tests.exe
    PLATFORM_SRC = $(SRC_DIR)/platform_win32.c
    MKDIR_P = if not exist $(BIN_DIR) mkdir $(BIN_DIR)
    RM = del /Q /F
else
    TARGET = $(BIN_DIR)/cshell
    TEST_TARGET = $(BIN_DIR)/cshell_tests
    PLATFORM_SRC = $(SRC_DIR)/platform_posix.c
    MKDIR_P = mkdir -p $(BIN_DIR)
    RM = rm -rf
endif

CORE_SRCS = $(SRC_DIR)/cshell.c \
            $(SRC_DIR)/lexer.c \
            $(SRC_DIR)/parser.c \
            $(SRC_DIR)/builtin.c \
            $(SRC_DIR)/executor.c \
            $(SRC_DIR)/history.c \
            $(PLATFORM_SRC)

.PHONY: all clean test help

all: $(TARGET) $(TEST_TARGET)

$(TARGET): $(SRC_DIR)/main.c $(CORE_SRCS)
	@$(MKDIR_P)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_TARGET): $(TEST_DIR)/test_runner.c $(CORE_SRCS)
	@$(MKDIR_P)
	$(CC) $(CFLAGS) -o $@ $^

test: $(TEST_TARGET)
	@echo Running automated test suite...
	@$(TEST_TARGET)

clean:
ifeq ($(OS),Windows_NT)
	@if exist $(BIN_DIR) rmdir /S /Q $(BIN_DIR)
else
	@rm -rf $(BIN_DIR)
endif

help:
	@echo Available targets:
	@echo   all    - Build cshell and test runner
	@echo   test   - Build and run test suite
	@echo   clean  - Remove build artifacts
