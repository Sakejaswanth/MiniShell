/**
 * @file parser.c
 * @brief Command AST parsing implementation for C-Shell.
 */

#include "parser.h"
#include "cshell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *cshell_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) memcpy(copy, s, len + 1);
    return copy;
}

command_t *command_create(void) {
    command_t *cmd = (command_t *)calloc(1, sizeof(command_t));
    if (!cmd) return NULL;
    cmd->argv = (char **)calloc(CSHELL_MAX_ARGS, sizeof(char *));
    cmd->argc = 0;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append_output = false;
    return cmd;
}

void command_free(command_t *cmd) {
    if (!cmd) return;
    if (cmd->argv) {
        for (int i = 0; i < cmd->argc; i++) {
            if (cmd->argv[i]) {
                free(cmd->argv[i]);
                cmd->argv[i] = NULL;
            }
        }
        free(cmd->argv);
    }
    if (cmd->input_file) free(cmd->input_file);
    if (cmd->output_file) free(cmd->output_file);
    free(cmd);
}

pipeline_t *pipeline_create(void) {
    pipeline_t *pipe = (pipeline_t *)calloc(1, sizeof(pipeline_t));
    if (!pipe) return NULL;
    pipe->commands = (command_t **)calloc(CSHELL_MAX_PIPES, sizeof(command_t *));
    pipe->command_count = 0;
    pipe->is_background = false;
    return pipe;
}

void pipeline_free(pipeline_t *pipeline) {
    if (!pipeline) return;
    if (pipeline->commands) {
        for (int i = 0; i < pipeline->command_count; i++) {
            command_free(pipeline->commands[i]);
        }
        free(pipeline->commands);
    }
    free(pipeline);
}

static command_line_t *command_line_create(void) {
    command_line_t *cl = (command_line_t *)calloc(1, sizeof(command_line_t));
    if (!cl) return NULL;
    cl->pipelines = (pipeline_t **)calloc(CSHELL_MAX_PIPES, sizeof(pipeline_t *));
    cl->pipeline_count = 0;
    return cl;
}

void command_line_free(command_line_t *cmd_line) {
    if (!cmd_line) return;
    if (cmd_line->pipelines) {
        for (int i = 0; i < cmd_line->pipeline_count; i++) {
            pipeline_free(cmd_line->pipelines[i]);
        }
        free(cmd_line->pipelines);
    }
    free(cmd_line);
}

static bool command_add_arg(command_t *cmd, const char *arg) {
    if (cmd->argc >= CSHELL_MAX_ARGS - 1) {
        fprintf(stderr, "cshell: error: maximum argument limit (%d) exceeded\n", CSHELL_MAX_ARGS);
        return false;
    }
    cmd->argv[cmd->argc] = cshell_strdup(arg);
    if (!cmd->argv[cmd->argc]) return false;
    cmd->argc++;
    cmd->argv[cmd->argc] = NULL;
    return true;
}

command_line_t *parser_parse(const token_list_t *tokens) {
    if (!tokens || tokens->count == 0) return NULL;

    command_line_t *cmd_line = command_line_create();
    if (!cmd_line) return NULL;

    pipeline_t *current_pipeline = pipeline_create();
    command_t *current_command = command_create();
    size_t i = 0;

    while (i < tokens->count) {
        token_t *t = &tokens->tokens[i];

        if (t->type == TOKEN_ERROR) {
            command_free(current_command);
            pipeline_free(current_pipeline);
            command_line_free(cmd_line);
            return NULL;
        }

        if (t->type == TOKEN_WORD) {
            command_add_arg(current_command, t->value);
            i++;
            continue;
        }

        if (t->type == TOKEN_REDIRECT_IN) {
            i++;
            if (i >= tokens->count || tokens->tokens[i].type != TOKEN_WORD) {
                fprintf(stderr, "cshell: syntax error near unexpected token '<'\n");
                goto error_cleanup;
            }
            if (current_command->input_file) free(current_command->input_file);
            current_command->input_file = cshell_strdup(tokens->tokens[i].value);
            i++;
            continue;
        }

        if (t->type == TOKEN_REDIRECT_OUT || t->type == TOKEN_REDIRECT_APPEND) {
            bool append = (t->type == TOKEN_REDIRECT_APPEND);
            i++;
            if (i >= tokens->count || tokens->tokens[i].type != TOKEN_WORD) {
                fprintf(stderr, "cshell: syntax error near unexpected token '%s'\n",
                        append ? ">>" : ">");
                goto error_cleanup;
            }
            if (current_command->output_file) free(current_command->output_file);
            current_command->output_file = cshell_strdup(tokens->tokens[i].value);
            current_command->append_output = append;
            i++;
            continue;
        }

        if (t->type == TOKEN_PIPE) {
            if (current_command->argc == 0) {
                fprintf(stderr, "cshell: syntax error near unexpected token '|'\n");
                goto error_cleanup;
            }
            current_pipeline->commands[current_pipeline->command_count++] = current_command;
            current_command = command_create();
            i++;
            /* Check if next is EOF or another pipe */
            if (i < tokens->count && (tokens->tokens[i].type == TOKEN_PIPE ||
                                      tokens->tokens[i].type == TOKEN_EOF ||
                                      tokens->tokens[i].type == TOKEN_SEMICOLON)) {
                fprintf(stderr, "cshell: syntax error near unexpected token '%s'\n",
                        token_type_to_string(tokens->tokens[i].type));
                goto error_cleanup;
            }
            continue;
        }

        if (t->type == TOKEN_BACKGROUND) {
            current_pipeline->is_background = true;
            i++;
            continue;
        }

        if (t->type == TOKEN_SEMICOLON || t->type == TOKEN_EOF) {
            if (current_command->argc > 0) {
                current_pipeline->commands[current_pipeline->command_count++] = current_command;
                current_command = command_create();
            }

            if (current_pipeline->command_count > 0) {
                cmd_line->pipelines[cmd_line->pipeline_count++] = current_pipeline;
                current_pipeline = pipeline_create();
            }

            i++;
            if (t->type == TOKEN_EOF) break;
            continue;
        }

        i++;
    }

    command_free(current_command);
    pipeline_free(current_pipeline);

    if (cmd_line->pipeline_count == 0) {
        command_line_free(cmd_line);
        return NULL;
    }

    return cmd_line;

error_cleanup:
    command_free(current_command);
    pipeline_free(current_pipeline);
    command_line_free(cmd_line);
    return NULL;
}
