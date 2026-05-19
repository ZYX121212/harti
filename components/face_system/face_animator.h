#ifndef FACE_ANIMATOR_H
#define FACE_ANIMATOR_H

#include "face_model.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void animator_init(void);
void animator_tick(void);

void animator_set_expression(expression_id_t id);
void animator_set_component(face_component_t comp,
                            const void *target_params,
                            uint32_t duration_ms,
                            uint32_t delay_ms,
                            anim_path_type_t path_type);
void animator_set_component_instant(face_component_t comp, const void *target_params);
const face_state_t *animator_get_state(void);
void animator_cancel_all(void);

#ifdef __cplusplus
}
#endif

#endif
