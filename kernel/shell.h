#pragma once
#include <stdint.h>

#define SHELL_MAX_ARGS 16
#define SHELL_ARG_LEN 64

struct ShellArgs {
    int argc;
    char argv[SHELL_MAX_ARGS][SHELL_ARG_LEN];
};

typedef void (*ShellCmdFn)(const ShellArgs& args);

struct ShellCommand {
    const char* name;
    const char* help;
    ShellCmdFn fn;
};

void shell_init();
void shell_execute(const char* line);
void shell_register(const char* name, const char* help, ShellCmdFn fn);