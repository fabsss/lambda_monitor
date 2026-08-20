#ifndef WARMUP_FSM_H
#define WARMUP_FSM_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Warm-up state machine states.
 */
typedef enum {
    WARMUP_STATE_WARMUP,
    WARMUP_STATE_OPERATING
} warmup_state_t;

/**
 * @brief Warm-up FSM instance data.
 */
typedef struct {
    warmup_state_t state;
    uint32_t elapsed_s;
    uint32_t t_warmup_s;
} warmup_fsm_t;

/**
 * @brief Initialize the warm-up FSM.
 *
 * @param fsm        Pointer to FSM instance to initialize.
 * @param t_warmup_s Warm-up timeout in seconds (spec §3.2 default: 90).
 */
void warmup_fsm_init(warmup_fsm_t *fsm, uint32_t t_warmup_s);

/**
 * @brief Advance the FSM by one tick.
 *
 * While in WARMUP, accumulates elapsed time and transitions to OPERATING
 * either when a switching edge is detected or when the configured warm-up
 * timeout elapses. Once in OPERATING, this function is a no-op.
 *
 * @param fsm                     Pointer to FSM instance.
 * @param delta_s                 Seconds elapsed since the previous tick.
 * @param switching_edge_detected True if a switching edge was observed this tick.
 */
void warmup_fsm_tick(warmup_fsm_t *fsm, uint32_t delta_s, bool switching_edge_detected);

#endif /* WARMUP_FSM_H */
