/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_caps_word

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/modifiers_state_changed.h>
#include <zmk/keys.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct caps_word_continue_item {
    uint32_t id;
    uint16_t page;
    uint8_t implicit_modifiers;
};

struct behavior_caps_word_config {
    zmk_mod_flags_t mods;
    int idle_timeout_ms;
    size_t continuations_count;
    struct caps_word_continue_item continuations[];
};

struct behavior_caps_word_data {
    struct k_timer idle_timer;
    bool active;
};

static const struct device *devs[DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)];
static size_t dev_count = 0;
static int pressed_key_count = 0;

static void restart_caps_word_idle_timer(const struct device *dev) {
    const struct behavior_caps_word_config *config = dev->config;
    struct behavior_caps_word_data *data = dev->data;

    if (config->idle_timeout_ms) {
        k_timer_start(&data->idle_timer, K_MSEC(config->idle_timeout_ms), K_NO_WAIT);
    }
}

static void restart_caps_word_idle_timer_all_devs(void) {
    for (int i = 0; i < dev_count; i++) {
        if (devs[i]) {
            restart_caps_word_idle_timer(devs[i]);
        }
    }
}

static void cancel_caps_word_idle_timer(const struct device *dev) {
    const struct behavior_caps_word_config *config = dev->config;
    struct behavior_caps_word_data *data = dev->data;

    if (config->idle_timeout_ms) {
        k_timer_stop(&data->idle_timer);
    }
}

static void activate_caps_word(const struct device *dev) {
    struct behavior_caps_word_data *data = dev->data;

    data->active = true;
    restart_caps_word_idle_timer(dev);
}

static void deactivate_caps_word(const struct device *dev) {
    struct behavior_caps_word_data *data = dev->data;

    data->active = false;
    cancel_caps_word_idle_timer(dev);
}

static int on_caps_word_binding_pressed(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    struct behavior_caps_word_data *data = dev->data;

    if (data->active) {
        deactivate_caps_word(dev);
    } else {
        activate_caps_word(dev);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_caps_word_binding_released(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_caps_word_driver_api = {
    .binding_pressed = on_caps_word_binding_pressed,
    .binding_released = on_caps_word_binding_released,
};

static int caps_word_keycode_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_caps_word, caps_word_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_caps_word, zmk_keycode_state_changed);

static bool caps_word_is_caps_includelist(const struct behavior_caps_word_config *config,
                                          uint16_t usage_page, uint8_t usage_id,
                                          uint8_t implicit_modifiers) {
    for (int i = 0; i < config->continuations_count; i++) {
        const struct caps_word_continue_item *continuation = &config->continuations[i];
        LOG_DBG("Comparing with 0x%02X - 0x%02X (with implicit mods: 0x%02X)", continuation->page,
                continuation->id, continuation->implicit_modifiers);

        if (continuation->page == usage_page && continuation->id == usage_id &&
            (continuation->implicit_modifiers &
             (implicit_modifiers | zmk_hid_get_explicit_mods())) ==
                continuation->implicit_modifiers) {
            LOG_DBG("Continuing capsword, found included usage: 0x%02X - 0x%02X", usage_page,
                    usage_id);
            return true;
        }
    }

    return false;
}

static bool caps_word_is_alpha(uint8_t usage_id) {
    return (usage_id >= HID_USAGE_KEY_KEYBOARD_A && usage_id <= HID_USAGE_KEY_KEYBOARD_Z);
}

static bool caps_word_is_numeric(uint8_t usage_id) {
    return (usage_id >= HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION &&
            usage_id <= HID_USAGE_KEY_KEYBOARD_0_AND_RIGHT_PARENTHESIS);
}

static bool caps_word_should_enhance(const struct behavior_caps_word_config *config,
                                     struct zmk_keycode_state_changed *ev) {
    if (ev->usage_page != HID_USAGE_KEY) {
        return false;
    }

    if (caps_word_is_alpha(ev->keycode)) {
        return true;
    }

    return false;
}

static void caps_word_enhance_usage(const struct behavior_caps_word_config *config,
                                    struct zmk_keycode_state_changed *ev) {
    if (caps_word_should_enhance(config, ev)) {
        LOG_DBG("Enhancing usage 0x%02X with modifiers: 0x%02X", ev->keycode, config->mods);
        ev->implicit_modifiers |= config->mods;
    }
}

static int caps_word_keycode_state_changed_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (!ev->state) {
        // Idle timer should only run when all keys are released.
        pressed_key_count--;
        pressed_key_count = MAX(pressed_key_count, 0);

        if (pressed_key_count == 0) {
            restart_caps_word_idle_timer_all_devs();
        }

        return ZMK_EV_EVENT_BUBBLE;
    }

    pressed_key_count++;

    for (int i = 0; i < dev_count; i++) {
        const struct device *dev = devs[i];
        if (dev == NULL) {
            continue;
        }

        struct behavior_caps_word_data *data = dev->data;
        if (!data->active) {
            continue;
        }

        cancel_caps_word_idle_timer(dev);

        const struct behavior_caps_word_config *config = dev->config;

        caps_word_enhance_usage(config, ev);

        if (!caps_word_is_alpha(ev->keycode) && !caps_word_is_numeric(ev->keycode) &&
            !is_mod(ev->usage_page, ev->keycode) &&
            !caps_word_is_caps_includelist(config, ev->usage_page, ev->keycode,
                                           ev->implicit_modifiers)) {
            LOG_DBG("Deactivating caps_word for 0x%02X - 0x%02X", ev->usage_page, ev->keycode);
            deactivate_caps_word(dev);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

static void caps_word_timeout_handler(struct k_timer *timer) {
    struct behavior_caps_word_data *data =
        CONTAINER_OF(timer, struct behavior_caps_word_data, idle_timer);

    LOG_DBG("Deactivating caps_word for idle timeout");
    data->active = false;
}

static int behavior_caps_word_init(const struct device *dev) {
    const struct behavior_caps_word_config *config = dev->config;
    struct behavior_caps_word_data *data = dev->data;

    if (config->idle_timeout_ms) {
        k_timer_init(&data->idle_timer, caps_word_timeout_handler, NULL);
    }

    __ASSERT(dev_count < ARRAY_SIZE(devs), "Too many devices");

    devs[dev_count] = dev;
    dev_count++;
    return 0;
}

#define CAPS_WORD_LABEL(i, _n) DT_INST_LABEL(i)

#define PARSE_CONTINUATION(i)                                                                      \
    {                                                                                              \
        .page = ZMK_HID_USAGE_PAGE(i), .id = ZMK_HID_USAGE_ID(i),                                  \
        .implicit_modifiers = SELECT_MODS(i)                                                       \
    }

#define CONTINUATION_ITEM(i, n) PARSE_CONTINUATION(DT_INST_PROP_BY_IDX(n, continue_list, i))

#define KP_INST(n)                                                                                 \
    static struct behavior_caps_word_data behavior_caps_word_data_##n = {.active = false};         \
    static struct behavior_caps_word_config behavior_caps_word_config_##n = {                      \
        .mods = DT_INST_PROP_OR(n, mods, MOD_LSFT),                                                \
        .idle_timeout_ms = DT_INST_PROP(n, idle_timeout_ms),                                       \
        .continuations = {LISTIFY(DT_INST_PROP_LEN(n, continue_list), CONTINUATION_ITEM, (, ),     \
                                  n)},                                                             \
        .continuations_count = DT_INST_PROP_LEN(n, continue_list),                                 \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, behavior_caps_word_init, NULL, &behavior_caps_word_data_##n,          \
                          &behavior_caps_word_config_##n, APPLICATION,                             \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_caps_word_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)

#endif
