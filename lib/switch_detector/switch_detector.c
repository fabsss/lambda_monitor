#include "switch_detector.h"

#define SWITCH_WINDOW_S 60u

void switch_detector_init(switch_detector_t *sd)
{
    sd->last_index = 0;
    sd->has_last = false;
    sd->edge_count = 0;
    sd->window_elapsed_s = 0;
    sd->switches_per_min = 0;
    sd->seconds_since_last_edge = 0;
}

static bool crossed_zero(int32_t prev, int32_t now)
{
    if (prev == 0 || now == 0) return prev != now;
    return (prev < 0) != (now < 0);
}

bool switch_detector_update(switch_detector_t *sd, int32_t index, uint32_t delta_s)
{
    bool edge = false;

    if (sd->has_last) {
        edge = crossed_zero(sd->last_index, index);
    }

    sd->window_elapsed_s += delta_s;
    sd->seconds_since_last_edge += delta_s;

    if (edge) {
        sd->edge_count++;
        sd->seconds_since_last_edge = 0;
    }

    if (sd->window_elapsed_s >= SWITCH_WINDOW_S) {
        sd->switches_per_min = sd->edge_count;
        sd->edge_count = 0;
        sd->window_elapsed_s = 0;
    }

    sd->last_index = index;
    sd->has_last = true;
    return edge;
}
