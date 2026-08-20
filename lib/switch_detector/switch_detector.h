#ifndef SWITCH_DETECTOR_H
#define SWITCH_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Switching-frequency detector state.
 *
 * Tracks zero-crossings of the mixture index (sensor switching around
 * lambda = 1) over a rolling 60s window, plus time since the last edge.
 */
typedef struct {
    int32_t last_index;
    bool has_last;
    uint32_t edge_count;
    uint32_t window_elapsed_s;
    uint32_t switches_per_min;
    uint32_t seconds_since_last_edge;
} switch_detector_t;

/**
 * @brief Initialize the switch detector.
 *
 * @param sd Pointer to detector instance to initialize.
 */
void switch_detector_init(switch_detector_t *sd);

/**
 * @brief Feed a new mixture-index sample into the detector.
 *
 * Detects a zero-crossing edge relative to the previous sample (sign
 * change, where 0 counts as its own side). Accumulates edge counts over
 * a 60s window and republishes switches_per_min once the window elapses.
 *
 * @param sd      Pointer to detector instance.
 * @param index   Current mixture index sample.
 * @param delta_s Seconds elapsed since the previous sample.
 * @return         true if this sample is an edge, false otherwise.
 */
bool switch_detector_update(switch_detector_t *sd, int32_t index, uint32_t delta_s);

#endif /* SWITCH_DETECTOR_H */
