#pragma once
#include <stdint.h>

#define PIPE_BUF_SIZE 1024
#define PIPE_MAX 8

struct Pipe {
    char buf[PIPE_BUF_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
    bool used;
    bool write_closed;
};

int pipe_create();
void pipe_destroy(int id);

int pipe_read(int id, char* buf, uint32_t len);
int pipe_write(int id, const char* buf, uint32_t len);

int pipe_read_nb(int id, char* buf, uint32_t len);

void pipe_close_write(int id);
bool pipe_empty(int id);
void pipe_init_all();