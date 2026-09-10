/**
 * @file parser.h
 * @brief Grammar and AST parsing for C-Shell command lines.
 */

#ifndef CSHELL_PARSER_H
#define CSHELL_PARSER_H

#include "lexer.h"
#include <stdbool.h>

/* Single command structure representing `cmd arg1 arg2 < in > out` */
typedef struct {
    char **argv;             /* Argument vector, NULL-terminated */
    int argc;                /* Number of arguments */
    char *input_file;        /* Stdin redirection file (<), or NULL */
    char *output_file;       /* Stdout redirection file (> or >>), or NULL */
    bool append_output;      /* True if >>, false if > */
} command_t;

/* Pipeline structure representing `cmd1 | cmd2 | cmd3 [&]` */
typedef struct {
    command_t **commands;    /* Array of commands in the pipeline */
    int command_count;       /* Number of commands in this pipeline */
    bool is_background;      /* True if ended with '&' */
} pipeline_t;

/* Complete command line sequence representing `pipe1 ; pipe2 ; ...` */
typedef struct {
    pipeline_t **pipelines;  /* Array of pipelines executed sequentially */
    int pipeline_count;      /* Number of pipelines */
} command_line_t;

/* Parser functions */
command_line_t *parser_parse(const token_list_t *tokens);
void            command_line_free(command_line_t *cmd_line);
command_t      *command_create(void);
void            command_free(command_t *cmd);
pipeline_t     *pipeline_create(void);
void            pipeline_free(pipeline_t *pipeline);

#endif /* CSHELL_PARSER_H */
