/* components/face_system/face_seq.h */
#pragma once
#include "face_model.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEQ_MAX_STEPS 16

typedef struct {
    expression_id_t expr_id;   /* 0xFF = play the expression that triggered this seq */
    uint16_t        hold_ms;   /* time to hold this step after transition completes */
    uint16_t        trans_ms;  /* transition duration override (0 = use expr default) */
} seq_step_t;

typedef struct {
    const seq_step_t *steps;
    uint8_t           step_count;
    uint8_t           loop_count;  /* 0=once, 0xFF=infinite */
} face_seq_t;

/* Lifecycle */
void face_seq_init(void);
void face_seq_tick(void);   /* call every frame from face_animator_tick() */

/* Playback */
void face_seq_play(const face_seq_t *seq);
void face_seq_play_steps(const seq_step_t *steps, uint8_t count, uint8_t loops);
void face_seq_stop(void);
bool face_seq_is_playing(void);

/* Called by face_set_expression() — stops any running seq, checks for a
   built-in entry sequence, falls back to animator_set_expression(). */
void face_seq_on_expression_set(expression_id_t id);

#ifdef __cplusplus
}
#endif
