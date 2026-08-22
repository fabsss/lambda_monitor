#include "mix_filter.h"

void fast_filter_init(fast_filter_t *f)
{
    for (uint8_t i = 0; i < FAST_FILTER_SIZE; i++) {
        f->samples[i] = 0;
    }
    f->count = 0;
    f->next = 0;
}

int32_t fast_filter_push(fast_filter_t *f, int32_t mv)
{
    f->samples[f->next] = mv;
    f->next = (uint8_t)((f->next + 1) % FAST_FILTER_SIZE);
    if (f->count < FAST_FILTER_SIZE) {
        f->count++;
    }

    int64_t sum = 0;
    for (uint8_t i = 0; i < f->count; i++) {
        sum += f->samples[i];
    }

    return (int32_t)(sum / (int64_t)f->count);
}

void slow_filter_init(slow_filter_t *f, uint32_t window_s)
{
    if (window_s < 1) {
        window_s = 1;
    } else if (window_s > SLOW_FILTER_MAX_WINDOW_S) {
        window_s = SLOW_FILTER_MAX_WINDOW_S;
    }

    for (uint32_t i = 0; i < SLOW_FILTER_MAX_WINDOW_S; i++) {
        f->samples[i] = 0;
    }
    f->window_s = window_s;
    f->count = 0;
    f->next = 0;
    f->sum = 0;
    f->last_avg = 0;
}

void slow_filter_push(slow_filter_t *f, int32_t value, uint32_t delta_s)
{
    (void)delta_s;

    if (f->count < f->window_s) {
        f->sum += value;
        f->count++;
    } else {
        f->sum += value - f->samples[f->next];
    }
    f->samples[f->next] = value;
    f->next = (f->next + 1) % f->window_s;

    f->last_avg = (int32_t)(f->sum / (int64_t)f->count);
}

int32_t slow_filter_average(const slow_filter_t *f)
{
    return f->last_avg;
}
