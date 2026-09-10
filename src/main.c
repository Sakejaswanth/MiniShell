/**
 * @file main.c
 * @brief Shell CLI entry point and argument parsing.
 */

#include "cshell.h"
#include <stdio.h>
#include <string.h>

static void print_banner(void) {
    printf("\033[1;36m");
    printf("  ____       ____  _          _ _ \n");
    printf(" / ___|     / ___|| |__   ___| | |\n");
    printf("| |   _____ \\___ \\| '_ \\ / _ \\ | |\n");
    printf("| |__|_____| ___) | | | |  __/ | |\n");
    printf(" \\____|     |____/|_| |_|\\___|_|_|\n");
    printf("\033[0m");
    printf(" C-Shell v%s - Modular Operating Systems Shell\n", CSHELL_VERSION);
    printf(" Type '\033[1;33mhelp\033[0m' for built-in commands or '\033[1;31mexit\033[0m' to quit.\n\n");
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            printf("%s version %s\n", CSHELL_NAME, CSHELL_VERSION);
            return 0;
        }

        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            printf("Usage: %s [options] [script_file]\n", CSHELL_NAME);
            printf("Options:\n");
            printf("  -c <command>   Execute specified command string and exit\n");
            printf("  -v, --version  Show version information\n");
            printf("  -h, --help     Show this help message\n");
            return 0;
        }

        if (strcmp(argv[1], "-c") == 0) {
            if (argc < 3) {
                fprintf(stderr, "%s: -c requires an argument\n", CSHELL_NAME);
                return 1;
            }
            cshell_init(false);
            int ret = cshell_execute_string(argv[2]);
            cshell_cleanup();
            return ret;
        }

        /* Script file execution */
        cshell_init(false);
        int ret = cshell_execute_script(argv[1]);
        cshell_cleanup();
        return ret;
    }

    /* Interactive REPL mode */
    print_banner();
    cshell_init(true);
    int ret = cshell_run_repl();
    cshell_cleanup();
    return ret;
}
