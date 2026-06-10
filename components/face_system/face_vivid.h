#pragma once
#include "face_model.h"

#ifdef __cplusplus
extern "C" {
#endif

void face_vivid_init(void);
void face_vivid_apply(face_state_t *s);

/* Tear drop readout for the renderer.
   x/y are pixel offsets relative to each eye's tear anchor;
   opacity is 0..1 (0 = inactive / invisible). */
typedef struct {
    float x;
    float y;
    float opacity;
} tear_drop_t;

/* Fill out[4] with the current tear state. [0,1]=left eye, [2,3]=right eye. */
void face_vivid_get_tears(tear_drop_t out[4], const face_state_t *s);

#ifdef __cplusplus
}
#endif
