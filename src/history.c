/**
 * @file history.c
 * @brief Command history implementation and file persistence.
 */

#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} history_state_t;

static history_state_t g_history = {NULL, 0, 0};

static char *cshell_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) memcpy(copy, s, len + 1);
    return copy;
}

void history_init(size_t capacity) {
    if (capacity == 0) capacity = 100;
    g_history.capacity = capacity;
    g_history.count = 0;
    g_history.items = (char **)calloc(capacity, sizeof(char *));
}

void history_add(const char *line) {
    if (!line || !g_history.items) return;

    /* Trim leading whitespace */
    while (isspace((unsigned char)*line)) line++;
    if (*line == '\0') return;

    /* Prevent duplicate of the immediately previous command */
    if (g_history.count > 0 && strcmp(g_history.items[g_history.count - 1], line) == 0) {
        return;
    }

    /* Grow capacity if needed */
    if (g_history.count >= g_history.capacity) {
        size_t new_cap = g_history.capacity * 2;
        char **new_items = (char **)realloc(g_history.items, new_cap * sizeof(char *));
        if (!new_items) return;
        g_history.items = new_items;
        g_history.capacity = new_cap;
    }

    g_history.items[g_history.count] = cshell_strdup(line);
    if (g_history.items[g_history.count]) {
        g_history.count++;
    }
}

const char *history_get(int index) {
    if (index < 1 || (size_t)index > g_history.count) return NULL;
    return g_history.items[index - 1];
}

size_t history_count(void) {
    return g_history.count;
}

void history_print(int limit) {
    if (g_history.count == 0) {
        printf("History is empty.\n");
        return;
    }

    size_t start = 0;
    if (limit > 0 && (size_t)limit < g_history.count) {
        start = g_history.count - (size_t)limit;
    }

    for (size_t i = start; i < g_history.count; i++) {
        printf(" %5zu  %s\n", i + 1, g_history.items[i]);
    }
}

void history_load(const char *filename) {
    if (!filename) return;
    FILE *f = fopen(filename, "r");
    if (!f) return;

    char line_buf[1024];
    while (fgets(line_buf, sizeof(line_buf), f)) {
        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r')) {
            line_buf[--len] = '\0';
        }
        if (len > 0) {
            history_add(line_buf);
        }
    }
    fclose(f);
}

void history_save(const char *filename) {
    if (!filename || !g_history.items) return;
    FILE *f = fopen(filename, "w");
    if (!f) return;

    for (size_t i = 0; i < g_history.count; i++) {
        if (g_history.items[i]) {
            fprintf(f, "%s\n", g_history.items[i]);
        }
    }
    fclose(f);
}

void history_cleanup(void) {
    if (!g_history.items) return;
    for (size_t i = 0; i < g_history.count; i++) {
        if (g_history.items[i]) {
            free(g_history.items[i]);
            g_history.items[i] = NULL;
        }
    }
    free(g_history.items);
    g_history.items = NULL;
    g_history.count = 0;
    g_history.capacity = 0;
}

bool history_expand(const char *input, char *output, size_t max_len) {
    if (!input || !output || max_len == 0) return false;

    /* Check for !! (last command) */
    if (strcmp(input, "!!") == 0) {
        if (g_history.count == 0) {
            fprintf(stderr, "cshell: !!: event not found\n");
            return false;
        }
        const char *last_cmd = history_get((int)g_history.count);
        strncpy(output, last_cmd, max_len - 1);
        output[max_len - 1] = '\0';
        printf("%s\n", output);
        return true;
    }

    /* Check for !n (command by index) */
    if (input[0] == '!' && isdigit((unsigned char)input[1])) {
        int n = atoi(input + 1);
        const char *cmd = history_get(n);
        if (!cmd) {
            fprintf(stderr, "cshell: !%d: event not found\n", n);
            return false;
        }
        strncpy(output, cmd, max_len - 1);
        output[max_len - 1] = '\0';
        printf("%s\n", output);
        return true;
    }

    /* No expansion needed */
    strncpy(output, input, max_len - 1);
    output[max_len - 1] = '\0';
    return true;
}
