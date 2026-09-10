/**
 * @file history.h
 * @brief Command history tracking, persistence, and expansion.
 */

#ifndef CSHELL_HISTORY_H
#define CSHELL_HISTORY_H

#include <stddef.h>
#include <stdbool.h>

void history_init(size_t capacity);
void history_add(const char *line);
const char *history_get(int index);
size_t history_count(void);
void history_print(int limit);
void history_load(const char *filename);
void history_save(const char *filename);
void history_cleanup(void);
bool history_expand(const char *input, char *output, size_t max_len);

#endif /* CSHELL_HISTORY_H */
