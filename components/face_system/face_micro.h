#ifndef FACE_MICRO_H
#define FACE_MICRO_H

#include "face_model.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Call once at startup
void micro_animator_init(void);

// Call every frame: applies micro-animations on top of display_state
// Modifies display_state in-place (blink lids, saccade iris, breathing, tilt tracking)
void micro_animator_apply(face_state_t *display_state);

// Set tilt angles for eye tracking (pitch, roll in radians)
void micro_animator_set_tilt(float pitch, float roll);

// Enable/disable all micro-animations
void micro_animator_set_enabled(bool on);

// Trigger a one-shot wink on a specific eye (0=left, 1=right).
// Interrupts any in-progress blink on that eye.
void micro_animator_wink(int eye);

// Notify micro-animator of current expression. Resets expression-linked state machines.
void micro_animator_set_expression(expression_id_t id);

#ifdef __cplusplus
}
#endif

#endif
