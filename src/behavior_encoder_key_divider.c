/*
 * Copyright (c) 2026 Takashi Imai
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_encoder_key_divider

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>

#include <lism/encoder_divider_state.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_encoder_key_divider_config {
    struct zmk_behavior_binding binding;
    uint32_t divisor;
    uint32_t timeout_ms;
};

struct behavior_encoder_key_divider_data {
    struct k_spinlock lock;
    struct lism_encoder_divider_state state;
    uint32_t forwarded_param;
    bool forwarded;
};

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param_values[] = {
    {
        .display_name = "Key",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_HID_USAGE,
    },
};

static const struct behavior_parameter_metadata_set param_metadata_set[] = {{
    .param1_values = param_values,
    .param1_values_len = ARRAY_SIZE(param_values),
}};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(param_metadata_set),
    .sets = param_metadata_set,
};

#endif

static int on_encoder_key_divider_pressed(struct zmk_behavior_binding *binding,
                                          struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_encoder_key_divider_config *cfg = dev->config;
    struct behavior_encoder_key_divider_data *data = dev->data;
    k_spinlock_key_t key = k_spin_lock(&data->lock);

    if (data->forwarded) {
        k_spin_unlock(&data->lock, key);
        LOG_ERR("Encoder divider received a press before the previous release");
        return -EBUSY;
    }

    if (!lism_encoder_divider_state_update(&data->state, binding->param1, cfg->divisor,
                                           event.timestamp, cfg->timeout_ms)) {
        k_spin_unlock(&data->lock, key);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    struct zmk_behavior_binding forwarded_binding = cfg->binding;
    forwarded_binding.param1 = binding->param1;
    data->forwarded_param = binding->param1;
    data->forwarded = true;
    k_spin_unlock(&data->lock, key);

    const int ret = zmk_behavior_invoke_binding(&forwarded_binding, event, true);
    if (ret < 0) {
        key = k_spin_lock(&data->lock);
        data->forwarded = false;
        k_spin_unlock(&data->lock, key);
        LOG_ERR("Encoder divider key press failed: %d", ret);
        return ret;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_encoder_key_divider_released(struct zmk_behavior_binding *binding,
                                           struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_encoder_key_divider_config *cfg = dev->config;
    struct behavior_encoder_key_divider_data *data = dev->data;
    k_spinlock_key_t key = k_spin_lock(&data->lock);

    if (!data->forwarded) {
        k_spin_unlock(&data->lock, key);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    struct zmk_behavior_binding forwarded_binding = cfg->binding;
    forwarded_binding.param1 = data->forwarded_param;
    data->forwarded = false;
    k_spin_unlock(&data->lock, key);

    const int ret = zmk_behavior_invoke_binding(&forwarded_binding, event, false);
    if (ret < 0) {
        LOG_ERR("Encoder divider key release failed: %d", ret);
    }
    return ret;
}

static const struct behavior_driver_api behavior_encoder_key_divider_driver_api = {
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
    .binding_pressed = on_encoder_key_divider_pressed,
    .binding_released = on_encoder_key_divider_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif
};

#define ENCODER_KEY_DIVIDER_INST(n)                                                               \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, bindings) == 1,                                              \
                 "Encoder key divider requires exactly one binding");                            \
    BUILD_ASSERT(DT_INST_PROP(n, divisor) > 0, "Encoder key divider divisor must be positive");  \
    BUILD_ASSERT(DT_INST_PROP(n, timeout_ms) > 0,                                                 \
                 "Encoder key divider timeout must be positive");                                \
    static const struct behavior_encoder_key_divider_config                                       \
        behavior_encoder_key_divider_config_##n = {                                               \
            .binding = {.behavior_dev =                                                           \
                            DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, 0))},               \
            .divisor = DT_INST_PROP(n, divisor),                                                   \
            .timeout_ms = DT_INST_PROP(n, timeout_ms),                                             \
    };                                                                                             \
    static struct behavior_encoder_key_divider_data behavior_encoder_key_divider_data_##n;        \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_encoder_key_divider_data_##n,                 \
                            &behavior_encoder_key_divider_config_##n, POST_KERNEL,                 \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                  \
                            &behavior_encoder_key_divider_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ENCODER_KEY_DIVIDER_INST)

#define RESET_ENCODER_KEY_DIVIDER(inst)                                                           \
    do {                                                                                           \
        struct behavior_encoder_key_divider_data *data = DEVICE_DT_INST_GET(inst)->data;          \
        k_spinlock_key_t key = k_spin_lock(&data->lock);                                           \
        lism_encoder_divider_state_reset(&data->state);                                            \
        k_spin_unlock(&data->lock, key);                                                           \
    } while (false);

static int encoder_key_divider_layer_state_changed_listener(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *event = as_zmk_layer_state_changed(eh);

    /* Layers 0 and 1 share the two-step mode; higher layers discard pending input. */
    if (event != NULL && event->layer >= 2) {
        DT_INST_FOREACH_STATUS_OKAY(RESET_ENCODER_KEY_DIVIDER)
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(encoder_key_divider_layer_state, encoder_key_divider_layer_state_changed_listener);
ZMK_SUBSCRIPTION(encoder_key_divider_layer_state, zmk_layer_state_changed);
