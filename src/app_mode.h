#pragma once
#include <stdint.h>

// ── App modes (one per physical orientation) ──────────────────────────────────
typedef enum {
    MODE_CLOCK      = 0,   // bord droit posé  → paysage rotation=1
    MODE_COUNTDOWN  = 1,   // bord gauche posé → paysage rotation=3
    MODE_WHEEL      = 2,   // bord haut pose   -> wheel
    MODE_D          = 3,   // bord bas posé    → portrait rotation=2
} AppMode;

// ── Countdown state machine ───────────────────────────────────────────────────
typedef enum {
    CD_IDLE    = 0,
    CD_RUNNING = 1,
    CD_DONE    = 2,
} CdState;

// ── Shared globals (defined in main.cpp) ──────────────────────────────────────
extern uint8_t  g_active_rotation;   // TFT rotation currently applied (0-3)
extern uint32_t g_countdown_ms;      // countdown remaining time in ms
extern CdState  g_cd_state;
extern AppMode  g_current_mode;
