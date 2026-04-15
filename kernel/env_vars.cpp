#include "env_vars.h"

#include "kstring.h"

static EnvVar env_vars[ENV_VAR_MAX];
static uint32_t env_count = 0;

const char *env_get(const char *key) {
    for (uint32_t i = 0; i < env_count; i++) {
        if (kstrcmp(env_vars[i].key, key) == 0) return env_vars[i].value;
    }

    return nullptr;
}

void env_set(const char *key, const char *value) {
    for (uint32_t i = 0; i < env_count; i++)
    {
        if (kstrcmp(env_vars[i].key, key) == 0)
        {
            env_vars[i].value = value;
            return;
        }
    }

    env_vars[env_count].key = key;
    env_vars[env_count].value = value;
    env_count++;
}

void env_unset(const char *key) {
    for (uint32_t i = 0; i < env_count; i++)
    {
        if (kstrcmp(env_vars[i].key, key) == 0)
        {
            env_vars[i].value = nullptr;
            return;
        }
    }
}