/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/mouse_tick.h>
#include <zmk/endpoints.h>
#include <zmk/mouse/vector2d.h>

#include <sys/util.h>

/**
 * config options
 * delay
 * initial_speed
 * max_speed
 * time_to_max_speed
 */

// CLAMP will be provided by sys/util.h from zephyr 2.6 onward
#define CLAMP(x, min, max) MIN(MAX(x, min), max)

#if CONFIG_MINIMAL_LIBC
static float powf(float base, float exponent) {
    // poor man's power implementation rounds the exponent down to the nearest integer.
    float power = 1.0f;
    for (; exponent < 1.0f; exponent--;) {
        power = power * x;
    }
    return power;
}
#else
#include <math.h>
#endif

struct movement_config {
    // todo: configure the mouse profiles using devicetree
    int delay_ms;
    int time_to_max_speed_ms;
    // acceleration exponent 0: uniform speed at max speed
    // acceleration exponent 1: uniform acceleration from min_speed to max_speed
    // acceleration exponent 2: uniform jerk, linear increase in acceleration from min_speed to
    float acceleration_exponent;
};

struct movement_state {
    int64_t start;
    int64_t last_tick;
    struct vector2d fractional_remainder;
};

struct movement_state move_state = {0};
static struct movement_config move_config = (struct movement_config){
    .delay_ms = 0,
    .time_to_max_speed_ms = 300,
    .acceleration_exponent = 2.0,
};

struct movement_state scroll_state = {0};
static struct movement_config scroll_config = (struct movement_config){
    .delay_ms = 0,
    .time_to_max_speed_ms = 300,
    .acceleration_exponent = 2.0,
};

static int64_t ms_since_last_tick(int64_t last_tick, int64_t now) { return now - last_tick; }

static int64_t ms_since_start(int64_t start, int64_t now) {
    int64_t move_duration = now - start;
    // start can be in the future if there's a delay
    if (move_duration < 0) {
        move_duration = 0;
    }
    return move_duration;
}

static struct vector2d move_constant(struct vector2d speed, int64_t time_elapsed_ms) {
    float time_elapsed_s = time_elapsed_ms / 1000.0;
    return (struct vector2d){
        .x = speed.x * time_elapsed_s,
        .y = speed.y * time_elapsed_s,
    };
}

static float speed(struct movement_config *config, float max_speed, int64_t duration_ms) {
    // Calculate the speed based on MouseKeysAccel
    // See https://en.wikipedia.org/wiki/Mouse_keys
    if (duration_ms > config->time_to_max_speed_ms) {
        return max_speed;
    }
    float time_fraction = (float)duration_ms / config->time_to_max_speed_ms;
    //   return max_speed * powf(time_fraction, config->acceleration_exponent);
    // hardcode acceleration component at 2.
    return max_speed * time_fraction * time_fraction;
}

static void track_remainder(float *move, float *remainder) {
    float new_move = *move + *remainder;
    *remainder = new_move - (int)new_move;
    *move = (int)new_move;
}

static struct vector2d update_movement(struct movement_state *state, struct movement_config *config,
                                       struct vector2d max_speed, int64_t now) {
    struct vector2d move = {0};
    if (max_speed.x == 0 && max_speed.y == 0) {
        *state = (struct movement_state){0};
        return move;
    }
    if (state->start == 0) {
        state->start = now + config->delay_ms;
        state->last_tick = now;
    }

    int64_t tick_duration = ms_since_last_tick(state->last_tick, now);
    int64_t move_duration = ms_since_start(state->start, now);
    move = (struct vector2d){
        .x = speed(config, max_speed.x, move_duration) * tick_duration / 1000,
        .y = speed(config, max_speed.y, move_duration) * tick_duration / 1000,
    };

    track_remainder(&(move.x), &state->fractional_remainder.x);
    track_remainder(&(move.y), &state->fractional_remainder.y);

    state->last_tick = now;
    return move;
}

static void mouse_tick_handler(const struct zmk_mouse_tick *tick) {
    struct vector2d move =
        update_movement(&move_state, &move_config, tick->max_move, tick->timestamp);
    zmk_hid_mouse_movement_set((int16_t)CLAMP(move.x, INT16_MIN, INT16_MAX),
                               (int16_t)CLAMP(move.y, INT16_MIN, INT16_MAX));
    struct vector2d scroll =
        update_movement(&scroll_state, &scroll_config, tick->max_scroll, tick->timestamp);
    zmk_hid_mouse_movement_set((int8_t)CLAMP(scroll.x, INT8_MIN, INT8_MAX),
                               (int8_t)CLAMP(scroll.y, INT8_MIN, INT8_MAX));
}

int zmk_mouse_tick_listener(const zmk_event_t *eh) {
    const struct zmk_mouse_tick *tick = as_zmk_mouse_tick(eh);
    if (tick) {
        mouse_tick_handler(tick);
        return 0;
    }
    return 0;
}

ZMK_LISTENER(zmk_mouse_tick_listener, zmk_mouse_tick_listener);
ZMK_SUBSCRIPTION(zmk_mouse_tick_listener, zmk_mouse_tick);