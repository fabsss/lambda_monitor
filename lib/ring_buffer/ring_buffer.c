#include "ring_buffer.h"

void ring_buffer_init(ring_buffer_t *rb)
{
    rb->head = 0;
    rb->count = 0;
}

void ring_buffer_push(ring_buffer_t *rb, int32_t index_value, uint32_t timestamp_s)
{
    uint16_t write_pos;

    if (rb->count < RING_BUFFER_CAPACITY) {
        write_pos = (uint16_t)((rb->head + rb->count) % RING_BUFFER_CAPACITY);
        rb->count++;
    } else {
        /* Full: overwrite oldest slot (at head) and advance head */
        write_pos = rb->head;
        rb->head = (uint16_t)((rb->head + 1) % RING_BUFFER_CAPACITY);
    }

    rb->index_values[write_pos] = index_value;
    rb->timestamps_s[write_pos] = timestamp_s;
}

uint16_t ring_buffer_count(const ring_buffer_t *rb)
{
    return rb->count;
}

bool ring_buffer_get(const ring_buffer_t *rb, uint16_t i, int32_t *out_index_value, uint32_t *out_timestamp_s)
{
    if (i >= rb->count) {
        return false;
    }

    uint16_t pos = (uint16_t)((rb->head + i) % RING_BUFFER_CAPACITY);
    if (out_index_value) {
        *out_index_value = rb->index_values[pos];
    }
    if (out_timestamp_s) {
        *out_timestamp_s = rb->timestamps_s[pos];
    }
    return true;
}
