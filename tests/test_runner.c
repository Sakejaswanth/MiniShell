/**
 * @file test_runner.c
 * @brief Automated unit and integration test suite for C-Shell.
 */

#include "cshell.h"
#include "lexer.h"
#include "parser.h"
#include "builtin.h"
#include "history.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(expr, msg) do { \
    g_tests_run++; \
    if (expr) { \
        g_tests_passed++; \
        printf("  [\033[1;32mPASS\033[0m] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("  [\033[1;31mFAIL\033[0m] %s (line %d)\n", msg, __LINE__); \
    } \
} while (0)

static void test_lexer_basic(void) {
    printf("\n--- Running Lexer Tests ---\n");

    /* 1. Simple command */
    token_list_t *t1 = lexer_tokenize("ls -la /tmp");
    TEST_ASSERT(t1 != NULL, "Tokenize basic words");
    TEST_ASSERT(t1->count == 4, "Token count for 'ls -la /tmp'");
    TEST_ASSERT(t1->tokens[0].type == TOKEN_WORD && strcmp(t1->tokens[0].value, "ls") == 0, "First word is 'ls'");
    TEST_ASSERT(t1->tokens[1].type == TOKEN_WORD && strcmp(t1->tokens[1].value, "-la") == 0, "Second word is '-la'");
    TEST_ASSERT(t1->tokens[2].type == TOKEN_WORD && strcmp(t1->tokens[2].value, "/tmp") == 0, "Third word is '/tmp'");
    TEST_ASSERT(t1->tokens[3].type == TOKEN_EOF, "Fourth token is EOF");
    token_list_free(t1);

    /* 2. Pipelines and Redirections */
    token_list_t *t2 = lexer_tokenize("cat < input.txt | grep error >> out.log &");
    TEST_ASSERT(t2 != NULL, "Tokenize pipeline and redirects");
    TEST_ASSERT(t2->tokens[0].type == TOKEN_WORD && strcmp(t2->tokens[0].value, "cat") == 0, "Token 'cat'");
    TEST_ASSERT(t2->tokens[1].type == TOKEN_REDIRECT_IN, "Token '<'");
    TEST_ASSERT(t2->tokens[2].type == TOKEN_WORD && strcmp(t2->tokens[2].value, "input.txt") == 0, "Token 'input.txt'");
    TEST_ASSERT(t2->tokens[3].type == TOKEN_PIPE, "Token '|'");
    TEST_ASSERT(t2->tokens[4].type == TOKEN_WORD && strcmp(t2->tokens[4].value, "grep") == 0, "Token 'grep'");
    TEST_ASSERT(t2->tokens[5].type == TOKEN_WORD && strcmp(t2->tokens[5].value, "error") == 0, "Token 'error'");
    TEST_ASSERT(t2->tokens[6].type == TOKEN_REDIRECT_APPEND, "Token '>>'");
    TEST_ASSERT(t2->tokens[7].type == TOKEN_WORD && strcmp(t2->tokens[7].value, "out.log") == 0, "Token 'out.log'");
    TEST_ASSERT(t2->tokens[8].type == TOKEN_BACKGROUND, "Token '&'");
    token_list_free(t2);

    /* 3. Quotes with spaces */
    token_list_t *t3 = lexer_tokenize("echo \"hello world\" 'single quote'");
    TEST_ASSERT(t3 != NULL, "Tokenize quoted strings");
    TEST_ASSERT(t3->tokens[0].type == TOKEN_WORD && strcmp(t3->tokens[0].value, "echo") == 0, "Token 'echo'");
    TEST_ASSERT(t3->tokens[1].type == TOKEN_WORD && strcmp(t3->tokens[1].value, "hello world") == 0, "Double quoted preserved spaces");
    TEST_ASSERT(t3->tokens[2].type == TOKEN_WORD && strcmp(t3->tokens[2].value, "single quote") == 0, "Single quoted preserved spaces");
    token_list_free(t3);

    /* 4. Semicolon commands */
    token_list_t *t4 = lexer_tokenize("cd .. ; pwd");
    TEST_ASSERT(t4 != NULL, "Tokenize semicolon");
    TEST_ASSERT(t4->tokens[2].type == TOKEN_SEMICOLON, "Token ';'");
    token_list_free(t4);
}

static void test_parser_basic(void) {
    printf("\n--- Running Parser Tests ---\n");

    /* 1. Simple command AST */
    token_list_t *t1 = lexer_tokenize("echo hello world");
    command_line_t *cl1 = parser_parse(t1);
    TEST_ASSERT(cl1 != NULL, "Parse basic command");
    TEST_ASSERT(cl1->pipeline_count == 1, "Pipeline count is 1");
    TEST_ASSERT(cl1->pipelines[0]->command_count == 1, "Command count is 1");
    TEST_ASSERT(cl1->pipelines[0]->commands[0]->argc == 3, "Argc is 3");
    TEST_ASSERT(strcmp(cl1->pipelines[0]->commands[0]->argv[0], "echo") == 0, "Argv[0] is echo");
    TEST_ASSERT(strcmp(cl1->pipelines[0]->commands[0]->argv[1], "hello") == 0, "Argv[1] is hello");
    TEST_ASSERT(strcmp(cl1->pipelines[0]->commands[0]->argv[2], "world") == 0, "Argv[2] is world");
    command_line_free(cl1);
    token_list_free(t1);

    /* 2. Pipeline and redirection AST */
    token_list_t *t2 = lexer_tokenize("cat < in.txt | grep foo > out.txt &");
    command_line_t *cl2 = parser_parse(t2);
    TEST_ASSERT(cl2 != NULL, "Parse pipeline with redirection");
    TEST_ASSERT(cl2->pipelines[0]->command_count == 2, "Pipeline has 2 commands");
    TEST_ASSERT(cl2->pipelines[0]->is_background == true, "Background flag is set");

    command_t *c1 = cl2->pipelines[0]->commands[0];
    TEST_ASSERT(c1->argc == 1 && strcmp(c1->argv[0], "cat") == 0, "First command is 'cat'");
    TEST_ASSERT(c1->input_file && strcmp(c1->input_file, "in.txt") == 0, "Input file is 'in.txt'");

    command_t *c2 = cl2->pipelines[0]->commands[1];
    TEST_ASSERT(c2->argc == 2 && strcmp(c2->argv[0], "grep") == 0, "Second command is 'grep'");
    TEST_ASSERT(c2->output_file && strcmp(c2->output_file, "out.txt") == 0, "Output file is 'out.txt'");
    TEST_ASSERT(c2->append_output == false, "Output is not append mode");

    command_line_free(cl2);
    token_list_free(t2);

    /* 3. Multiple sequential pipelines */
    token_list_t *t3 = lexer_tokenize("pwd ; ls ; whoami");
    command_line_t *cl3 = parser_parse(t3);
    TEST_ASSERT(cl3 != NULL, "Parse sequential pipelines");
    TEST_ASSERT(cl3->pipeline_count == 3, "Sequential pipeline count is 3");
    command_line_free(cl3);
    token_list_free(t3);
}

static void test_builtins(void) {
    printf("\n--- Running Builtin Command Tests ---\n");

    TEST_ASSERT(builtin_is_builtin("cd"), "'cd' recognized as built-in");
    TEST_ASSERT(builtin_is_builtin("pwd"), "'pwd' recognized as built-in");
    TEST_ASSERT(builtin_is_builtin("echo"), "'echo' recognized as built-in");
    TEST_ASSERT(builtin_is_builtin("history"), "'history' recognized as built-in");
    TEST_ASSERT(builtin_is_builtin("export"), "'export' recognized as built-in");
    TEST_ASSERT(builtin_is_builtin("setenv"), "'setenv' recognized as built-in");
    TEST_ASSERT(builtin_is_builtin("unsetenv"), "'unsetenv' recognized as built-in");
    TEST_ASSERT(builtin_is_builtin("exit"), "'exit' recognized as built-in");
    TEST_ASSERT(!builtin_is_builtin("not_a_builtin_command_123"), "Unknown command is not built-in");

    /* Test setenv / getenv */
    char *argv_set[] = {"setenv", "CSHELL_TEST_VAR", "TEST_VALUE_42", NULL};
    int ret_set = builtin_setenv(3, argv_set);
    TEST_ASSERT(ret_set == CSHELL_SUCCESS, "setenv succeeds");
    const char *val = getenv("CSHELL_TEST_VAR");
    TEST_ASSERT(val != NULL && strcmp(val, "TEST_VALUE_42") == 0, "Environment variable was stored");

    /* Test unsetenv */
    char *argv_unset[] = {"unsetenv", "CSHELL_TEST_VAR", NULL};
    int ret_unset = builtin_unsetenv(2, argv_unset);
    TEST_ASSERT(ret_unset == CSHELL_SUCCESS, "unsetenv succeeds");
}

static void test_history(void) {
    printf("\n--- Running History Tests ---\n");

    history_init(10);
    TEST_ASSERT(history_count() == 0, "History starts empty");

    history_add("echo first");
    history_add("ls -l");
    history_add("echo third");
    TEST_ASSERT(history_count() == 3, "History count is 3");

    TEST_ASSERT(strcmp(history_get(1), "echo first") == 0, "Item 1 is 'echo first'");
    TEST_ASSERT(strcmp(history_get(2), "ls -l") == 0, "Item 2 is 'ls -l'");
    TEST_ASSERT(strcmp(history_get(3), "echo third") == 0, "Item 3 is 'echo third'");

    /* Expansion of !! */
    char exp_buf[256];
    bool res1 = history_expand("!!", exp_buf, sizeof(exp_buf));
    TEST_ASSERT(res1 && strcmp(exp_buf, "echo third") == 0, "Expansion of '!!' returns last command");

    /* Expansion of !1 */
    bool res2 = history_expand("!1", exp_buf, sizeof(exp_buf));
    TEST_ASSERT(res2 && strcmp(exp_buf, "echo first") == 0, "Expansion of '!1' returns first command");

    history_cleanup();
}

int main(void) {
    printf("=========================================\n");
    printf("     C-Shell Automated Test Suite        \n");
    printf("=========================================\n");

    cshell_init(false);

    test_lexer_basic();
    test_parser_basic();
    test_builtins();
    test_history();

    printf("\n=========================================\n");
    printf(" Test Results: %d/%d Passed (%d Failed)\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("=========================================\n");

    cshell_cleanup();
    return (g_tests_failed == 0) ? 0 : 1;
}
