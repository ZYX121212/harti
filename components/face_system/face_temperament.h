#pragma once
#include "face_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float warmth;        /* 0.0..1.0, expressiveness / exaggeration */
    float energy;        /* 0.0..1.0, expression intensity */
    float mischief;      /* 0.0..1.0, random micro-expression probability */
    float curiosity;     /* 0.0..1.0, reaction speed */
} temperament_profile_t;

extern const temperament_profile_t TEMPERAMENT_DEFAULT;
extern const temperament_profile_t TEMPERAMENT_CHILL;
extern const temperament_profile_t TEMPERAMENT_DRAMATIC;

void face_temperament_init(void);
void face_temperament_set_profile(const temperament_profile_t *p);
void face_temperament_notify_expression_change(expression_id_t prev, expression_id_t cur);
void face_temperament_apply(face_state_t *s);

#ifdef __cplusplus
}
#endif
