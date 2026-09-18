/*
 * Copyright (c) 2026 Takashi Imai
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

struct lism_encoder_divider_state {
    uint32_t direction;
    uint32_t count;
    int64_t last_input_ms;
    bool has_direction;
};

void lism_encoder_divider_state_reset(struct lism_encoder_divider_state *state);

bool lism_encoder_divider_state_update(struct lism_encoder_divider_state *state,
                                       uint32_t direction, uint32_t divisor, int64_t now_ms,
                                       uint32_t timeout_ms);
