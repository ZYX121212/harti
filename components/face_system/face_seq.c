/* components/face_system/face_seq.c */
#include "face_seq.h"
#include "face_animator.h"
#include "../../main/harti_config.h"
#include "lvgl.h"
#include <string.h>

/* ── State ──────────────────────────────────────────────────── */

typedef enum { SEQ_IDLE, SEQ_PLAYING } seq_state_t;

static seq_state_t      s_state      = SEQ_IDLE;
static seq_step_t       s_buf[SEQ_MAX_STEPS];
static uint8_t          s_count      = 0;
static uint8_t          s_loop_count = 0;
static uint8_t          s_loop_done  = 0;
static uint8_t          s_cur        = 0;
static uint32_t         s_step_start = 0;
static expression_id_t  s_target     = 0; /* expression that triggered this seq */

/* ── Built-in entry sequences ───────────────────────────────── */
/* Each ends with a {0xFF, 0, 0} step that resolves to the target expression. */

static const seq_step_t SEQ_SURPRISED[] = {
    {EMOTION_NEUTRAL,    80, 0},
    {0xFF,                0, 0},
};
static const seq_step_t SEQ_EXCITED[] = {
    {EMOTION_HAPPY,     120, 0},
    {0xFF,                0, 0},
};
static const seq_step_t SEQ_DIZZY[] = {
    {EMOTION_SURPRISED, 100, 0},
    {0xFF,                0, 0},
};
static const seq_step_t SEQ_HEART_EYES[] = {
    {EMOTION_HAPPY,     150, 0},
    {0xFF,                0, 0},
};

typedef struct {
    expression_id_t   id;
    const seq_step_t *steps;
    uint8_t           count;
} entry_def_t;

static const entry_def_t ENTRY_DEFS[] = {
    {EMOTION_SURPRISED,  SEQ_SURPRISED,  2},
    {EMOTION_EXCITED,    SEQ_EXCITED,    2},
    {EMOTION_DIZZY,      SEQ_DIZZY,      2},
    {EMOTION_HEART_EYES, SEQ_HEART_EYES, 2},
};
#define ENTRY_DEF_COUNT ((int)(sizeof(ENTRY_DEFS) / sizeof(ENTRY_DEFS[0])))

/* ── Internal helpers ───────────────────────────────────────── */

static void apply_step(uint8_t idx, uint32_t now);

static void advance(uint32_t now) {
    s_cur++;
    if (s_cur >= s_count) {
        s_loop_done++;
        if (s_loop_count == 0xFF || s_loop_done < s_loop_count) {
            s_cur = 0;
        } else {
            s_state = SEQ_IDLE;
            return;
        }
    }
    apply_step(s_cur, now);
}

static void apply_step(uint8_t idx, uint32_t now) {
    s_step_start = now;
    const seq_step_t *step = &s_buf[idx];

    if (step->expr_id == 0xFF) {
        animator_set_expression(s_target);
    } else {
        animator_set_expression(step->expr_id);
    }

    uint32_t total = (uint32_t)step->trans_ms + (uint32_t)step->hold_ms;
    if (total == 0) {
        advance(now);  /* zero-duration step: advance immediately */
    }
}

/* ── Public API ─────────────────────────────────────────────── */

void face_seq_init(void) {
    s_state = SEQ_IDLE;
    s_count = 0;
}

void face_seq_tick(void) {
    if (s_state != SEQ_PLAYING) return;
    uint32_t now     = lv_tick_get();
    uint32_t elapsed = now - s_step_start;
    const seq_step_t *step  = &s_buf[s_cur];
    uint32_t total = (uint32_t)step->trans_ms + (uint32_t)step->hold_ms;
    if (total > 0 && elapsed >= total) {
        advance(now);
    }
}

void face_seq_play(const face_seq_t *seq) {
    if (!seq) return;
    face_seq_play_steps(seq->steps, seq->step_count, seq->loop_count);
}

void face_seq_play_steps(const seq_step_t *steps, uint8_t count, uint8_t loops) {
    if (!steps || count == 0 || count > SEQ_MAX_STEPS) return;
    memcpy(s_buf, steps, count * sizeof(seq_step_t));
    s_count      = count;
    s_loop_count = loops;
    s_loop_done  = 0;
    s_cur        = 0;
    s_state      = SEQ_PLAYING;
    apply_step(0, lv_tick_get());
}

void face_seq_stop(void) {
    s_state = SEQ_IDLE;
}

bool face_seq_is_playing(void) {
    return s_state == SEQ_PLAYING;
}

void face_seq_on_expression_set(expression_id_t id) {
    face_seq_stop();
    s_target = id;

    for (int i = 0; i < ENTRY_DEF_COUNT; i++) {
        if (ENTRY_DEFS[i].id == id) {
            face_seq_play_steps(ENTRY_DEFS[i].steps, ENTRY_DEFS[i].count, 0);
            return;
        }
    }

    animator_set_expression(id);
}
