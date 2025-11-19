
#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H
#include <stdbool.h>

#define BUFFER_SIZE 2048

typedef struct
{
    char data[BUFFER_SIZE];
    int head;
    int tail;
    int count;
} cir_buf_t;

void cb_init(cir_buf_t *cb);

bool cb_is_full(cir_buf_t *cb);

bool cb_is_empty(cir_buf_t *cb);

bool cb_write(cir_buf_t *cb, char value);
bool cb_write_chunk(cir_buf_t *cb, char *buf, int size);

bool cb_read(cir_buf_t *cb, char *value);
int cb_read_chunk(cir_buf_t *cb, char *buf, int size);

extern cir_buf_t ser_cb;
extern cir_buf_t sock_cb;

#endif