
#include "circular_buffer.h"
#include <string.h>
void cb_init(cir_buf_t *cb)
{
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;
}

bool cb_is_full(cir_buf_t *cb)
{
    return cb->count == BUFFER_SIZE;
}

bool cb_is_empty(cir_buf_t *cb)
{
    return cb->count == 0;
}

bool cb_write(cir_buf_t *cb, char value)
{
    if (cb_is_full(cb))
    {
        return false;
    }

    cb->data[cb->head] = value;
    cb->head = (cb->head + 1) % BUFFER_SIZE;
    cb->count++;
    return true;
}

bool cb_write_chunk(cir_buf_t *cb, char *buf, int size)
{
    // Check if there's enough space
    if (cb->count + size > BUFFER_SIZE)
    {
        return false; // Not enough space
    }

    // Write in one or two chunks to handle circular wrapping
    int first_chunk = size;
    if (cb->head + size > BUFFER_SIZE)
    {
        first_chunk = BUFFER_SIZE - cb->head;
    }

    memcpy(&cb->data[cb->head], buf, first_chunk); // FROM buf TO buffer

    if (first_chunk < size) // Need to wrap around
    {
        memcpy(&cb->data[0], buf + first_chunk, size - first_chunk);
    }

    cb->head = (cb->head + size) % BUFFER_SIZE;
    cb->count += size; // Adding data, not removing

    return true;
}

bool cb_read(cir_buf_t *cb, char *value)
{
    if (cb_is_empty(cb))
    {
        return false; // Buffer empty
    }

    *value = cb->data[cb->tail];
    cb->tail = (cb->tail + 1) % BUFFER_SIZE;
    cb->count--;
    return true;
}

int cb_read_chunk(cir_buf_t *cb, char *buf, int size)
{

    if (cb->count == 0)
    {
        return 0;
    }

    int read_size = 0;

    for (int i = 0; i < cb->count; i++)
    {
        int idx = (cb->tail + i) % BUFFER_SIZE;
        if (cb->data[idx] == '\n')
        {
            read_size = i + 1; // Include the newline
        }
    }

    if (read_size == 0)
    {
        return 0; // No complete line available
    }

    // Read the data
    int first_chunk = read_size;
    if (cb->tail + read_size > BUFFER_SIZE)
    {
        first_chunk = BUFFER_SIZE - cb->tail;
    }

    memcpy(buf, &cb->data[cb->tail], first_chunk);

    if (first_chunk < read_size)
    {
        memcpy(buf + first_chunk, &cb->data[0], read_size - first_chunk);
    }

    cb->tail = (cb->tail + read_size) % BUFFER_SIZE;
    cb->count -= read_size;

    return read_size; // Return number of bytes read
}

cir_buf_t ser_cb;
cir_buf_t sock_cb;
