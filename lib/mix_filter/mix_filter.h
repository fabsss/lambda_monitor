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

/* Largest window_s any caller may request (adc_task.c pushes at ~1 sample/s,
 * so this bounds the ring buffer to window_s samples). */
#define SLOW_FILTER_MAX_WINDOW_S 30

/**
 * @brief Slow moving-average filter for trend display.
 *
 * True sliding window over the last window_s samples (one push per
 * second from adc_task.c), like fast_filter_t but with a caller-chosen
 * window length. Each push drops the oldest sample and adds the newest,
 * so the average updates every second instead of jumping once per
 * window like a tumbling/block average would.
 */
typedef struct {
    int32_t samples[SLOW_FILTER_MAX_WINDOW_S];
    uint32_t window_s;
    uint32_t count;
    uint32_t next;
    int64_t sum;
    int32_t last_avg;
} slow_filter_t;

/**
 * @brief Initialize the slow filter (empty).
 *
 * @param f        Pointer to filter instance.
 * @param window_s Averaging window length in seconds (samples), clamped
 *                 to [1, SLOW_FILTER_MAX_WINDOW_S].
 */
void slow_filter_init(slow_filter_t *f, uint32_t window_s);

/**
 * @brief Push a new sample and update the sliding-window average.
 *
 * @param f       Pointer to filter instance.
 * @param value   Sample value.
 * @param delta_s Seconds elapsed since the previous sample (unused beyond
 *                 sanity; pushes are assumed to arrive roughly once per
 *                 second, matching adc_task.c's 1Hz tick).
 */
void slow_filter_push(slow_filter_t *f, int32_t value, uint32_t delta_s);

/**
 * @brief Get the current moving average over the last window_s samples.
 *
 * @param f Pointer to filter instance.
 * @return  Moving average over however many samples have been pushed so
 *          far (up to window_s); 0 if none have been pushed yet.
 */
int32_t slow_filter_average(const slow_filter_t *f);

#endif /* MIX_FILTER_H */
