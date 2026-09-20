/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include <lism/gesture_state.h>

#define THRESHOLD 90
#define COOLDOWN_MS 150

static enum lism_gesture_direction update(struct lism_gesture_state *state,
                                           enum lism_gesture_axis axis, int32_t value, bool sync,
                                           int64_t now_ms) {
    return lism_gesture_state_update(state, axis, value, sync, now_ms, THRESHOLD, COOLDOWN_MS);
}

int main(void) {
    struct lism_gesture_state state;

    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_X, 60, false, 1000) == LISM_GESTURE_NONE);
    assert(update(&state, LISM_GESTURE_AXIS_Y, 40, true, 1000) == LISM_GESTURE_RIGHT);

    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_X, 89, true, 1000) == LISM_GESTURE_NONE);
    assert(update(&state, LISM_GESTURE_AXIS_X, 1, true, 1015) == LISM_GESTURE_RIGHT);
    assert(state.x == 0 && state.y == 0);

    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_X, -90, true, 1000) == LISM_GESTURE_LEFT);

    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_Y, 90, true, 1000) == LISM_GESTURE_DOWN);

    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_X, 45, false, 1000) == LISM_GESTURE_NONE);
    assert(update(&state, LISM_GESTURE_AXIS_Y, 45, true, 1000) == LISM_GESTURE_RIGHT);

    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_X, 44, false, 1000) == LISM_GESTURE_NONE);
    assert(update(&state, LISM_GESTURE_AXIS_Y, -46, true, 1000) == LISM_GESTURE_UP);
    assert(state.x == 0 && state.y == 0);

    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_X, 90, true, 1000) == LISM_GESTURE_RIGHT);
    assert(update(&state, LISM_GESTURE_AXIS_X, 100, true, 1100) == LISM_GESTURE_NONE);
    assert(state.x == 0 && state.y == 0);
    assert(update(&state, LISM_GESTURE_AXIS_X, 89, true, 1150) == LISM_GESTURE_NONE);
    assert(state.x == 0 && state.y == 0);
    assert(update(&state, LISM_GESTURE_AXIS_X, 89, true, 1165) == LISM_GESTURE_NONE);
    assert(update(&state, LISM_GESTURE_AXIS_X, 1, true, 1180) == LISM_GESTURE_RIGHT);

    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_X, 90, true, 1000) == LISM_GESTURE_RIGHT);
    assert(update(&state, LISM_GESTURE_AXIS_X, 20, false, 1149) == LISM_GESTURE_NONE);
    assert(update(&state, LISM_GESTURE_AXIS_Y, 15, true, 1150) == LISM_GESTURE_NONE);
    assert(state.x == 0 && state.y == 0);
    assert(update(&state, LISM_GESTURE_AXIS_X, 90, true, 1165) == LISM_GESTURE_RIGHT);

    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_X, 30, true, 1000) == LISM_GESTURE_NONE);
    assert(update(&state, LISM_GESTURE_AXIS_X, -30, true, 1015) == LISM_GESTURE_NONE);
    assert(state.x == 0);

    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_X, 89, true, 1000) == LISM_GESTURE_NONE);
    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_X, 1, true, 1015) == LISM_GESTURE_NONE);

    lism_gesture_state_reset(&state);
    assert(update(&state, LISM_GESTURE_AXIS_X, INT32_MAX, false, 1000) == LISM_GESTURE_NONE);
    assert(update(&state, LISM_GESTURE_AXIS_X, 1, false, 1000) == LISM_GESTURE_NONE);
    assert(state.x == INT32_MAX);

    puts("gesture_state_test: PASS");
    return 0;
}
