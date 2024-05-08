
#include QMK_KEYBOARD_H
#include "print.h"


// World record for fastest index finger tapping is 1092 taps per minute, which
// is 55ms for a single tap.
// https://recordsetter.com/world-record/index-finger-taps-minute/46066
#define LED_CMD_TIMEOUT 200
#define DELTA_X_THRESHOLD 180
#define DELTA_Y_THRESHOLD 45
#define SCROLL_DIRECTION 1

typedef enum {
    unset     = 0,
    TG_SCROLL = 2, // 2x pushes
    CYC_DPI   = 4, // 4x pushes
    CMD_RESET = 6  // 6c pushes
} led_cmd_t;

// State
static bool   scroll_enabled  = false;
static bool   caps_lock_state = false;
static int8_t delta_x         = 0;
static int8_t delta_y         = 0;

typedef struct {
    led_cmd_t led_cmd;
    uint8_t   caps_lock_count;
} cmd_window_state_t;

// Dummy
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {{{KC_NO}}};

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    if (scroll_enabled) {
        delta_x += mouse_report.x;
        delta_y += mouse_report.y;

        if (delta_x > DELTA_X_THRESHOLD) {
            mouse_report.h = 1;
            delta_x        = 0;
        } else if (delta_x < -DELTA_X_THRESHOLD) {
            mouse_report.h = -1;
            delta_x        = 0;
        }

        if (delta_y > DELTA_Y_THRESHOLD) {
            mouse_report.v = -SCROLL_DIRECTION;
            delta_y        = 0;
        } else if (delta_y < -DELTA_Y_THRESHOLD) {
            mouse_report.v = SCROLL_DIRECTION;
            delta_y        = 0;
        }
        mouse_report.x = 0;
        mouse_report.y = 0;
    }
    return mouse_report;
}

void keyboard_post_init_user(void) {
    caps_lock_state = host_keyboard_led_state().caps_lock;
}

uint32_t reset_timeout(uint32_t trigger_time, void *cb_arg){
    cmd_window_state_t *cmd_window_state = (cmd_window_state_t *)cb_arg;
    if (cmd_window_state->led_cmd == cmd_window_state->caps_lock_count){
        if (
            cmd_window_state->caps_lock_count != 2
            && cmd_window_state->caps_lock_count != 4
            && cmd_window_state->caps_lock_count != 6
        ){
            cmd_window_state->led_cmd         = 0;
            cmd_window_state->caps_lock_count = 0;
        }
    }
    return 0;
}

uint32_t command_timeout(uint32_t trigger_time, void *cb_arg) {
    cmd_window_state_t *cmd_window_state = (cmd_window_state_t *)cb_arg;
#   ifdef CONSOLE_ENABLE
    uprintf("Received led_cmd (", cmd_window_state->led_cmd);
    uprintf("Received count (", cmd_window_state->caps_lock_count);
#   endif
    if (cmd_window_state->led_cmd == cmd_window_state->caps_lock_count){
        if (
            cmd_window_state->caps_lock_count == 2
            || cmd_window_state->caps_lock_count == 4
            || cmd_window_state->caps_lock_count == 6
        ){
            switch (cmd_window_state->led_cmd) {
                case TG_SCROLL:
                    scroll_enabled = !scroll_enabled;
                    break;
                case CYC_DPI:
                    cycle_dpi();
                    break;
                case CMD_RESET:
                    reset_keyboard();
                    break;
                default:
                    // Ignore unrecognised commands.
                    break;
            }
            cmd_window_state->led_cmd         = 0;
            cmd_window_state->caps_lock_count = 0;
        }
    }

    return 0; // Don't repeat
}

bool led_update_user(led_t led_state) {
    static cmd_window_state_t cmd_window_state = {
      .led_cmd = 0,
      .caps_lock_count = 0
    };

    // set the state and if on the correct count execute callback
    if (led_state.caps_lock != caps_lock_state) {
        cmd_window_state.caps_lock_count++;

        if (
            cmd_window_state.caps_lock_count == 2
            || cmd_window_state.caps_lock_count == 4
            || cmd_window_state.caps_lock_count == 6
        ){
            cmd_window_state.led_cmd = cmd_window_state.caps_lock_count;
            defer_exec(LED_CMD_TIMEOUT, command_timeout, &cmd_window_state);
        }

        if (
            cmd_window_state.caps_lock_count != 2
            && cmd_window_state.caps_lock_count != 4
            && cmd_window_state.caps_lock_count != 6
        ){
            defer_exec(LED_CMD_TIMEOUT, reset_timeout, &cmd_window_state);
        }

    }

    // Keep our copy of the LED states in sync with the host.
    caps_lock_state = led_state.caps_lock;
    return true;
}
