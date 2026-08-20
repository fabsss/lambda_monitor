#ifndef MIX_FILTER_H
#define MIX_FILTER_H

#include <stdint.h>

#define FAST_FILTER_SIZE 8

/**
 * @brief Fast moving-average filter over the last FAST_FILTER_SIZE samples.
 *
 * Intended for live display refresh. Uses a circular sample buffer; the
 * average is computed over however many samples have been pushed so far
 * (up to FAST_FILTER_SIZE).
 */
typedef struct {
    int32_t samples[FAST_FILTER_SIZE];
    uint8_t count;
    uint8_t next;
} fast_filter_t;

/**
 * @brief Initialize the fast filter (empty).
 */
void fast_filter_init(fast_filter_t *f);

/**
 * @brief Push a new sample and return the updated moving average.
 *
 * @param f  Pointer to filter instance.
 * @param mv New sample value (millivolts or index units).
 * @return   Moving average over the current window.
 */
int32_t fast_filter_push(fast_filter_t *f, int32_t mv);

/**
 * @brief Slow windowed-average filter for trend display.
 *
 * Accumulates values over a configurable time window (default 5s per
 * spec). When the window elapses, the average is published to
 * last_avg and accumulators reset.
 */
typedef struct {
    int32_t sum;
    uint32_t count;
    uint32_t window_s;
    uint32_t elapsed_s;
    int32_t last_avg;
} slow_filter_t;

/**
 * @brief Initialize the slow filter.
 *
 * @param f        Pointer to filter instance.
 * @param window_s Averaging window length in seconds.
 */
void slow_filter_init(slow_filter_t *f, uint32_t window_s);

/**
 * @brief Push a new sample into the slow filter's accumulator.
 *
 * When elapsed_s reaches window_s, last_avg is recomputed from the
 * accumulated sum/count and the accumulators reset.
 *
 * @param f       Pointer to filter instance.
 * @param value   Sample value.
 * @param delta_s Seconds elapsed since the previous sample.
 */
void slow_filter_push(slow_filter_t *f, int32_t value, uint32_t delta_s);

/**
 * @brief Get the most recently published window average.
 *
 * @param f Pointer to filter instance.
 * @return  Last published average (0 if no window has completed yet).
 */
int32_t slow_filter_average(const slow_filter_t *f);

#endif /* MIX_FILTER_H */
