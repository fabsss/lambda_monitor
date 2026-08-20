#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define RING_BUFFER_CAPACITY 600

/**
 * @brief Fixed-capacity FIFO ring buffer for the oscilloscope curve (Screen 2).
 *
 * Never grows beyond RING_BUFFER_CAPACITY; once full, pushing a new point
 * overwrites the oldest one.
 */
typedef struct {
    int32_t index_values[RING_BUFFER_CAPACITY];
    uint32_t timestamps_s[RING_BUFFER_CAPACITY];
    uint16_t head;
    uint16_t count;
} ring_buffer_t;

/**
 * @brief Initialize the ring buffer to empty.
 */
void ring_buffer_init(ring_buffer_t *rb);

/**
 * @brief Push a new (index_value, timestamp) point.
 *
 * When the buffer is full, the oldest point is overwritten.
 *
 * @param rb           Pointer to buffer instance.
 * @param index_value  Mixture index sample.
 * @param timestamp_s  Timestamp in seconds.
 */
void ring_buffer_push(ring_buffer_t *rb, int32_t index_value, uint32_t timestamp_s);

/**
 * @brief Number of points currently stored (0..RING_BUFFER_CAPACITY).
 */
uint16_t ring_buffer_count(const ring_buffer_t *rb);

/**
 * @brief Read the i-th point in chronological order (0 = oldest).
 *
 * @param rb              Pointer to buffer instance.
 * @param i               Logical index, 0 = oldest stored point.
 * @param out_index_value Output: mixture index at that point.
 * @param out_timestamp_s Output: timestamp at that point.
 * @return                true if i is within [0, count), false otherwise.
 */
bool ring_buffer_get(const ring_buffer_t *rb, uint16_t i, int32_t *out_index_value, uint32_t *out_timestamp_s);

#endif /* RING_BUFFER_H */
