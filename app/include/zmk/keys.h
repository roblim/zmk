/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <dt-bindings/zmk/keys.h>
#include <dt-bindings/zmk/modifiers.h>

typedef uint32_t zmk_key_t;
typedef uint8_t zmk_mod_t;
typedef uint8_t zmk_mod_flags_t;

struct zmk_key_event {
    uint32_t column;
    uint32_t row;
    zmk_key_t key;
    bool pressed;
};

struct zmk_key_decoded {
    zmk_mod_flags_t modifiers;
    uint8_t page;
    uint16_t id;
};

/**
 * Convert a uint32_t devicetree key code parameter to a struct zmk_key_decoded.
 */
#define ZMK_KEY_DECODE(encoded)                                                                    \
    (struct zmk_key_decoded) {                                                                     \
        .modifiers = SELECT_MODS(encoded), .page = ZMK_HID_USAGE_PAGE(encoded),                    \
        .id = ZMK_HID_USAGE_ID(encoded),                                                           \
    }

static inline bool is_mod(uint8_t usage_page, uint32_t keycode) {
    return (keycode >= HID_USAGE_KEY_KEYBOARD_LEFTCONTROL &&
            keycode <= HID_USAGE_KEY_KEYBOARD_RIGHT_GUI && usage_page == HID_USAGE_KEY);
}