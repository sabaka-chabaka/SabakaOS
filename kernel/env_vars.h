#pragma once
#include <stdint.h>

#define ENV_VAR_MAX 32

struct EnvVar {
    const char* key;
    const char* value;
};

const char* env_get(const char* key);
void env_set(const char* key, const char* value);
void env_unset(const char* key);