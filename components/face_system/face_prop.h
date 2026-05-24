#ifndef FACE_PROP_H
#define FACE_PROP_H

#include "face_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void prop_animator_init(void);
void prop_animator_apply(face_state_t *s);

void face_prop_show(prop_type_t type, float angle, float distance, uint32_t duration_ms);
void face_prop_hide(prop_type_t type, uint32_t duration_ms);
void face_prop_clear(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif
