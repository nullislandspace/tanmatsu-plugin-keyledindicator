// SPDX-License-Identifier: MIT
// Tanmatsu Test Plugin - Key Press LED Indicator
//
// This plugin demonstrates input hooks and LED control.
// When any key is pressed, LED 5 blinks white for 100ms.
// Key presses are forwarded normally to the launcher.

#include "tanmatsu_plugin.h"
#include "pax_gfx.h"

// Plugin state
static plugin_context_t* plugin_ctx = NULL;
static int hook_id = -1;
static int widget_id = -1;
static volatile uint32_t led_off_time = 0;  // Tick when LED should turn off
static volatile bool led_active = false;

// LED index used by this plugin (LEDs 2-5 are available for plugins)
#define KEY_LED_INDEX 5

// Status widget callback - draws a red circle when running
// Draws to the left of x_right, returns width used
static int status_widget_callback(pax_buf_t* buffer, int x_right, int y, int height, void* user_data) {
    (void)user_data;
    // Draw a red circle centered vertically in the status bar
    int radius = 6;
    int cx = x_right - radius - 2;  // Draw to the left of x_right with 2px margin
    int cy = y + height / 2;
    pax_draw_circle(buffer, 0xFFFF0000, cx, cy, radius);  // Red circle
    return radius * 2 + 4;  // Width = diameter + margins
}

// Plugin metadata
static const plugin_info_t plugin_info = {
    .name = "Key LED Indicator",
    .slug = "at.cavac.key-led-indicator",
    .version = "1.1.0",
    .author = "Rene Schickbauer",
    .description = "Blinks LED on keypress",
    .api_version = TANMATSU_PLUGIN_API_VERSION,
    .type = PLUGIN_TYPE_SERVICE,
    .flags = 0,
};

// Return plugin info
static const plugin_info_t* get_info(void) {
    return &plugin_info;
}

// Input hook callback - called for every input event
static bool input_hook_callback(plugin_input_event_t* event, void* user_data) {
    (void)user_data;

    // Only react to key press events (state == true), not releases
    if (event->state) {
        // Turn on LED white
        asp_led_set_pixel_rgb(KEY_LED_INDEX, 255, 255, 255);
        asp_led_send();

        // Set time to turn off LED (current time + 100ms)
        led_off_time = asp_plugin_get_tick_ms() + 100;
        led_active = true;
    }

    // Return false - don't consume the event, let it pass through to the launcher
    return false;
}

// Plugin initialization
static int plugin_init(plugin_context_t* ctx) {
    asp_log_info("keyled", "Key LED plugin initializing...");

    // Store context for LED claim/release
    plugin_ctx = ctx;

    // Claim LED for this plugin
    if (!asp_plugin_led_claim(ctx, KEY_LED_INDEX)) {
        asp_log_error("keyled", "Failed to claim LED %d", KEY_LED_INDEX);
        return -1;
    }

    // Register input hook
    hook_id = asp_plugin_input_hook_register(ctx, input_hook_callback, NULL);
    if (hook_id < 0) {
        asp_log_error("keyled", "Failed to register input hook");
        asp_plugin_led_release(ctx, KEY_LED_INDEX);
        return -1;
    }

    // Register status widget to show plugin is running
    widget_id = asp_plugin_status_widget_register(ctx, status_widget_callback, NULL);
    if (widget_id < 0) {
        asp_log_error("keyled", "Failed to register status widget");
    }

    asp_log_info("keyled", "Key LED plugin initialized, hook_id=%d, widget_id=%d", hook_id, widget_id);
    return 0;
}

// Plugin cleanup
static void plugin_cleanup(plugin_context_t* ctx) {
    // Unregister status widget
    if (widget_id >= 0) {
        asp_plugin_status_widget_unregister(widget_id);
        widget_id = -1;
    }

    // Unregister input hook
    if (hook_id >= 0) {
        asp_plugin_input_hook_unregister(hook_id);
        hook_id = -1;
    }

    // Turn off LED and release claim
    asp_led_set_pixel_rgb(KEY_LED_INDEX, 0, 0, 0);
    asp_led_send();
    asp_plugin_led_release(ctx, KEY_LED_INDEX);

    plugin_ctx = NULL;
    asp_log_info("keyled", "Key LED plugin cleaned up");
}

// Service main loop - monitors LED timing
static void plugin_service_run(plugin_context_t* ctx) {
    asp_log_info("keyled", "Key LED service starting...");

    while (!asp_plugin_should_stop(ctx)) {
        // Check if LED needs to be turned off
        if (led_active) {
            uint32_t now = asp_plugin_get_tick_ms();
            if (now >= led_off_time) {
                // Turn off LED
                asp_led_set_pixel_rgb(KEY_LED_INDEX, 0, 0, 0);
                asp_led_send();
                led_active = false;
            }
        }

        // Short sleep to avoid busy loop
        asp_plugin_delay_ms(10);
    }

    asp_log_info("keyled", "Key LED service stopped");
}

// Plugin entry point structure
static const plugin_entry_t entry = {
    .get_info = get_info,
    .init = plugin_init,
    .cleanup = plugin_cleanup,
    .menu_render = NULL,
    .menu_select = NULL,
    .service_run = plugin_service_run,
    .hook_event = NULL,
};

// Register this plugin with the host
TANMATSU_PLUGIN_REGISTER(entry);
