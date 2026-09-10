/**
 * @file executor.h
 * @brief Command execution engine, pipeline dispatch, and I/O redirection.
 */

#ifndef CSHELL_EXECUTOR_H
#define CSHELL_EXECUTOR_H

#include "parser.h"
#include "platform.h"

/* Execution functions */
int executor_execute_line(command_line_t *cmd_line);
int executor_execute_pipeline(pipeline_t *pipeline);
int executor_execute_builtin(command_t *cmd);

#endif /* CSHELL_EXECUTOR_H */
