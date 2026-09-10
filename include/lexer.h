/**
 * @file lexer.h
 * @brief Tokenizer and lexical analysis for C-Shell.
 */

#ifndef CSHELL_LEXER_H
#define CSHELL_LEXER_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TOKEN_WORD,              /* Regular argument or command name */
    TOKEN_PIPE,              /* | */
    TOKEN_REDIRECT_IN,       /* < */
    TOKEN_REDIRECT_OUT,      /* > */
    TOKEN_REDIRECT_APPEND,   /* >> */
    TOKEN_BACKGROUND,        /* & */
    TOKEN_SEMICOLON,         /* ; */
    TOKEN_EOF,               /* End of input line */
    TOKEN_ERROR              /* Syntax/lexical error */
} token_type_t;

typedef struct {
    token_type_t type;
    char *value;             /* Null-terminated string for TOKEN_WORD */
} token_t;

typedef struct {
    token_t *tokens;
    size_t count;
    size_t capacity;
} token_list_t;

/* Lexer functions */
token_list_t *lexer_tokenize(const char *input);
void          token_list_free(token_list_t *list);
const char   *token_type_to_string(token_type_t type);

#endif /* CSHELL_LEXER_H */
