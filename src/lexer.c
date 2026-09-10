/**
 * @file lexer.c
 * @brief Lexical analyzer and tokenizer for C-Shell.
 */

#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_TOKEN_CAPACITY 16
#define INITIAL_BUFFER_CAPACITY 64

static token_list_t *token_list_create(void) {
    token_list_t *list = (token_list_t *)malloc(sizeof(token_list_t));
    if (!list) return NULL;
    list->capacity = INITIAL_TOKEN_CAPACITY;
    list->count = 0;
    list->tokens = (token_t *)malloc(list->capacity * sizeof(token_t));
    if (!list->tokens) {
        free(list);
        return NULL;
    }
    return list;
}

static char *cshell_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) memcpy(copy, s, len + 1);
    return copy;
}

static bool token_list_add(token_list_t *list, token_type_t type, const char *value) {
    if (!list) return false;
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity * 2;
        token_t *new_tokens = (token_t *)realloc(list->tokens, new_cap * sizeof(token_t));
        if (!new_tokens) return false;
        list->tokens = new_tokens;
        list->capacity = new_cap;
    }

    list->tokens[list->count].type = type;
    if (value) {
        list->tokens[list->count].value = cshell_strdup(value);
        if (!list->tokens[list->count].value) return false;
    } else {
        list->tokens[list->count].value = NULL;
    }

    list->count++;
    return true;
}

void token_list_free(token_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        if (list->tokens[i].value) {
            free(list->tokens[i].value);
            list->tokens[i].value = NULL;
        }
    }
    free(list->tokens);
    free(list);
}

const char *token_type_to_string(token_type_t type) {
    switch (type) {
        case TOKEN_WORD:            return "WORD";
        case TOKEN_PIPE:            return "PIPE (|)";
        case TOKEN_REDIRECT_IN:     return "REDIRECT_IN (<)";
        case TOKEN_REDIRECT_OUT:    return "REDIRECT_OUT (>)";
        case TOKEN_REDIRECT_APPEND: return "REDIRECT_APPEND (>>)";
        case TOKEN_BACKGROUND:      return "BACKGROUND (&)";
        case TOKEN_SEMICOLON:       return "SEMICOLON (;)";
        case TOKEN_EOF:             return "EOF";
        case TOKEN_ERROR:           return "ERROR";
        default:                    return "UNKNOWN";
    }
}

token_list_t *lexer_tokenize(const char *input) {
    if (!input) return NULL;

    token_list_t *list = token_list_create();
    if (!list) return NULL;

    size_t len = strlen(input);
    size_t i = 0;

    /* Word buffer */
    size_t buf_cap = INITIAL_BUFFER_CAPACITY;
    char *buf = (char *)malloc(buf_cap);
    if (!buf) {
        token_list_free(list);
        return NULL;
    }

    while (i < len) {
        /* Skip whitespace */
        if (isspace((unsigned char)input[i])) {
            i++;
            continue;
        }

        /* Comments start with '#' */
        if (input[i] == '#') {
            break;
        }

        /* Check single/double character operators */
        if (input[i] == '|') {
            token_list_add(list, TOKEN_PIPE, "|");
            i++;
            continue;
        }

        if (input[i] == '<') {
            token_list_add(list, TOKEN_REDIRECT_IN, "<");
            i++;
            continue;
        }

        if (input[i] == '>') {
            if (i + 1 < len && input[i + 1] == '>') {
                token_list_add(list, TOKEN_REDIRECT_APPEND, ">>");
                i += 2;
            } else {
                token_list_add(list, TOKEN_REDIRECT_OUT, ">");
                i++;
            }
            continue;
        }

        if (input[i] == '&') {
            token_list_add(list, TOKEN_BACKGROUND, "&");
            i++;
            continue;
        }

        if (input[i] == ';') {
            token_list_add(list, TOKEN_SEMICOLON, ";");
            i++;
            continue;
        }

        /* Parse regular WORD (with quote and escape support) */
        size_t b_idx = 0;
        bool in_single_quote = false;
        bool in_double_quote = false;

        while (i < len) {
            char c = input[i];

            if (!in_single_quote && !in_double_quote) {
                if (isspace((unsigned char)c) || c == '|' || c == '<' || c == '>' ||
                    c == '&' || c == ';' || c == '#') {
                    break;
                }
            }

            if (c == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
                i++;
                continue;
            }

            if (c == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
                i++;
                continue;
            }

            if (c == '\\' && !in_single_quote && i + 1 < len) {
                /* Escaped character */
                i++;
                char next = input[i];
                if (next == 'n') c = '\n';
                else if (next == 't') c = '\t';
                else if (next == 'r') c = '\r';
                else if (next == '\\') c = '\\';
                else if (next == '"') c = '"';
                else if (next == '\'') c = '\'';
                else c = next;
            }

            /* Append char to word buffer */
            if (b_idx + 2 >= buf_cap) {
                buf_cap *= 2;
                char *new_buf = (char *)realloc(buf, buf_cap);
                if (!new_buf) {
                    free(buf);
                    token_list_free(list);
                    return NULL;
                }
                buf = new_buf;
            }

            buf[b_idx++] = c;
            i++;
        }

        buf[b_idx] = '\0';
        if (in_single_quote || in_double_quote) {
            fprintf(stderr, "cshell: syntax error: unclosed quote\n");
            token_list_add(list, TOKEN_ERROR, "unclosed quote");
            free(buf);
            return list;
        }

        token_list_add(list, TOKEN_WORD, buf);
    }

    token_list_add(list, TOKEN_EOF, NULL);
    free(buf);
    return list;
}
