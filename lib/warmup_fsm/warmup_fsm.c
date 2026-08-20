#include "warmup_fsm.h"

void warmup_fsm_init(warmup_fsm_t *fsm, uint32_t t_warmup_s)
{
    fsm->state = WARMUP_STATE_WARMUP;
    fsm->elapsed_s = 0;
    fsm->t_warmup_s = t_warmup_s;
}

void warmup_fsm_tick(warmup_fsm_t *fsm, uint32_t delta_s, bool switching_edge_detected)
{
    if (fsm->state == WARMUP_STATE_OPERATING) {
        return;
    }

    fsm->elapsed_s += delta_s;

    if (switching_edge_detected || fsm->elapsed_s >= fsm->t_warmup_s) {
        fsm->state = WARMUP_STATE_OPERATING;
    }
}
