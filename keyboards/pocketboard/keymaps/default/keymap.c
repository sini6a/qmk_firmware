// Copyright 2025 Sinisha Stojchevski
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include <qp.h>  // QP library for display
#include "quantum.h"  // QMK quantum headers for key events
#include "Skyhook.qff.h"
#include "SkyhookLarge.qff.h"
#include "animation.qgf.h"
#include "eeconfig.h"  // Required for EEPROM functions

typedef struct {
    uint8_t hue;
    uint8_t saturation;
    uint8_t brightness;
    bool led_state;
} led_config_t;

static led_config_t led_config = {
    .hue = 160,      // Default Hue
    .saturation = 255,  // Default Saturation (full color)
    .brightness = 255,  // Default Brightness (full)
    .led_state = false   // LED initially ON
};

// EEPROM storage location (arbitrary but safe address)
#define EECONFIG_LED 10

void save_led_config_to_eeprom(void) {
    eeprom_update_block(&led_config, (void*)EECONFIG_LED, sizeof(led_config_t));
}

void load_led_config_from_eeprom(void) {
    if (eeconfig_is_enabled()) {
        eeprom_read_block(&led_config, (void*)EECONFIG_LED, sizeof(led_config_t));
    }
}

#define LED_PIN GP12

static painter_device_t display;
static painter_font_handle_t font; // Declare a font handle
static painter_font_handle_t large_font; // Declare a font handle
static painter_image_handle_t anim;
static deferred_token curr_anim;

static uint32_t last_keypress_time;  // Store last key press timestamp
static bool animating = false;       // Track if animation is running
static uint32_t last_anim_time = 0;   // Track animation frame timing

void keyboard_pre_init_user(void) {
    setPinOutput(LED_PIN);
}

void keyboard_post_init_kb(void) {
    // Create the display device (assuming an 80x160 resolution and SPI setup)
    display = qp_st7735_make_spi_device(80, 160, GP1, GP29, GP28, 0, 0);
    qp_init(display, QP_ROTATION_180);  // Initialize display with rotation

    last_keypress_time = timer_read();  // Initialize timer

    anim = qp_load_image_mem(gfx_animation);
    if (!anim) {
        qp_clear(display);
        qp_drawtext_recolor(display, 10, 40, font, "ANI ERR", 0, 255, 255, 0, 0, 0); // Red text, black background
        qp_flush(display);
    }
    // Start animation immediately
    animating = true;
    last_anim_time = timer_read();
    curr_anim = qp_animate(display, 0, 0, anim);

    load_led_config_from_eeprom();

    if (led_config.led_state == true) {
        writePinHigh(LED_PIN);
    }

    // Load font from memory (make sure it's available)
    font = qp_load_font_mem(font_Skyhook);
    large_font = qp_load_font_mem(font_SkyhookLarge);
    if (!font || !large_font) {
        qp_clear(display);
        qp_drawtext_recolor(display, 10, 40, font, "FONT ERR", led_config.hue, led_config.saturation, led_config.brightness, 0, 0, 0); // Red text, black background
        qp_flush(display);
    }

    // Clear the display with a solid black background at the start
    qp_rect(display, 0, 0, 80, 160, 0, 0, 0, true);  // Full-screen black rectangle
    qp_flush(display);  // Make sure to flush to the screen
}

// Non-blocking animation task
void animation_task(void) {
    if (!animating && timer_elapsed(last_keypress_time) > 10000) {
        animating = true;
        last_anim_time = timer_read();
        curr_anim = qp_animate(display, 0, 0, anim);
    }
}


// Process keypresses, convert keycode to string, and display on the screen
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        last_keypress_time = timer_read();  // Reset timer
        animating = false;  // Stop animation
        qp_stop_animation(curr_anim);

        // Create a buffer to hold the key label
        char key_label[8];  // Buffer to hold the key label (max length 7 + null terminator)
        qp_rect(display, 0, 0, 80, 160, 0, 0, 0, true);  // Full-screen black rectangle
        qp_flush(display);  // Make sure to flush to the screen

        uint16_t key = keycode;

        // Extract keycode from layer-tap and mod-tap keys
        if (key >= QK_MOD_TAP && key <= QK_MOD_TAP_MAX) {
            key = QK_MOD_TAP_GET_TAP_KEYCODE(keycode);
        } else if (key >= QK_LAYER_TAP && key <= QK_LAYER_TAP_MAX) {
            key = QK_LAYER_TAP_GET_TAP_KEYCODE(keycode);
        }


        // Map the keycode to a string (character)
        switch (key) {
            case RGB_TOG:
                led_config.led_state = !led_config.led_state;
                if (led_config.led_state) {
                    writePinHigh(LED_PIN);
                } else {
                    writePinLow(LED_PIN);
                }
                save_led_config_to_eeprom();
                snprintf(key_label, sizeof(key_label), "LED");
                break;
            case RGB_HUI:
                led_config.hue += 10;
                if (led_config.hue > 255) {
                    led_config.hue = 255;  // Reset hue if it overflows
                }
                save_led_config_to_eeprom();
                snprintf(key_label, sizeof(key_label), "HUE");
                break;
            case RGB_HUD:
                led_config.hue -= 10;
                if (led_config.hue < 0) {  // Underflow protection
                    led_config.hue = 0;
                }
                save_led_config_to_eeprom();
                snprintf(key_label, sizeof(key_label), "HUE");
                break;

            case RGB_SAI:
                led_config.saturation += 10;
                if (led_config.saturation > 255) {
                    led_config.saturation = 255;
                }
                save_led_config_to_eeprom();
                snprintf(key_label, sizeof(key_label), "SAT");
                break;

            case RGB_SAD:
                led_config.saturation -= 10;
                if (led_config.saturation < 0) {
                    led_config.saturation = 0;
                }
                save_led_config_to_eeprom();
                snprintf(key_label, sizeof(key_label), "SAT");
                break;

            case RGB_VAI:
                led_config.brightness += 10;
                if (led_config.brightness > 255) {
                    led_config.brightness = 255;
                }
                save_led_config_to_eeprom();
                snprintf(key_label, sizeof(key_label), "VAL");
                break;

            case RGB_VAD:
                led_config.brightness -= 10;
                if (led_config.brightness < 0) {
                    led_config.brightness = 0;
                }
                save_led_config_to_eeprom();
                snprintf(key_label, sizeof(key_label), "VAL");
                break;

            case KC_A: snprintf(key_label, sizeof(key_label), "A"); break;
            case KC_B: snprintf(key_label, sizeof(key_label), "B"); break;
            case KC_C: snprintf(key_label, sizeof(key_label), "C"); break;
            case KC_D: snprintf(key_label, sizeof(key_label), "D"); break;
            case KC_E: snprintf(key_label, sizeof(key_label), "E"); break;
            case KC_F: snprintf(key_label, sizeof(key_label), "F"); break;
            case KC_G: snprintf(key_label, sizeof(key_label), "G"); break;
            case KC_H: snprintf(key_label, sizeof(key_label), "H"); break;
            case KC_I: snprintf(key_label, sizeof(key_label), "I"); break;
            case KC_J: snprintf(key_label, sizeof(key_label), "J"); break;
            case KC_K: snprintf(key_label, sizeof(key_label), "K"); break;
            case KC_L: snprintf(key_label, sizeof(key_label), "L"); break;
            case KC_M: snprintf(key_label, sizeof(key_label), "M"); break;
            case KC_N: snprintf(key_label, sizeof(key_label), "N"); break;
            case KC_O: snprintf(key_label, sizeof(key_label), "O"); break;
            case KC_P: snprintf(key_label, sizeof(key_label), "P"); break;
            case KC_Q: snprintf(key_label, sizeof(key_label), "Q"); break;
            case KC_R: snprintf(key_label, sizeof(key_label), "R"); break;
            case KC_S: snprintf(key_label, sizeof(key_label), "S"); break;
            case KC_T: snprintf(key_label, sizeof(key_label), "T"); break;
            case KC_U: snprintf(key_label, sizeof(key_label), "U"); break;
            case KC_V: snprintf(key_label, sizeof(key_label), "V"); break;
            case KC_W: snprintf(key_label, sizeof(key_label), "W"); break;
            case KC_X: snprintf(key_label, sizeof(key_label), "X"); break;
            case KC_Y: snprintf(key_label, sizeof(key_label), "Y"); break;
            case KC_Z: snprintf(key_label, sizeof(key_label), "Z"); break;
            case KC_1: snprintf(key_label, sizeof(key_label), "1"); break;
            case KC_2: snprintf(key_label, sizeof(key_label), "2"); break;
            case KC_3: snprintf(key_label, sizeof(key_label), "3"); break;
            case KC_4: snprintf(key_label, sizeof(key_label), "4"); break;
            case KC_5: snprintf(key_label, sizeof(key_label), "5"); break;
            case KC_6: snprintf(key_label, sizeof(key_label), "6"); break;
            case KC_7: snprintf(key_label, sizeof(key_label), "7"); break;
            case KC_8: snprintf(key_label, sizeof(key_label), "8"); break;
            case KC_9: snprintf(key_label, sizeof(key_label), "9"); break;
            case KC_0: snprintf(key_label, sizeof(key_label), "0"); break;

            case KC_SEMICOLON: snprintf(key_label, sizeof(key_label), ";"); break;
            case KC_COMMA: snprintf(key_label, sizeof(key_label), ","); break;
            case KC_DOT: snprintf(key_label, sizeof(key_label), "."); break;
            case KC_SLASH: snprintf(key_label, sizeof(key_label), "/"); break;

            case KC_ESC: snprintf(key_label, sizeof(key_label), "ESC"); break;
            case KC_TAB: snprintf(key_label, sizeof(key_label), "TAB"); break;
            case KC_ENTER: snprintf(key_label, sizeof(key_label), "ENT"); break;
            case KC_SPACE: snprintf(key_label, sizeof(key_label), "SPC"); break;
            case KC_BACKSPACE: snprintf(key_label, sizeof(key_label), "BSP"); break;
            case KC_DELETE: snprintf(key_label, sizeof(key_label), "DEL"); break;

            case KC_LCTL: snprintf(key_label, sizeof(key_label), "CTR"); break;
            case KC_LSFT: snprintf(key_label, sizeof(key_label), "SHF"); break;
            case KC_LALT: snprintf(key_label, sizeof(key_label), "ALT"); break;
            case KC_LGUI: snprintf(key_label, sizeof(key_label), "GUI"); break;
            case KC_RCTL: snprintf(key_label, sizeof(key_label), "CTR"); break;
            case KC_RSFT: snprintf(key_label, sizeof(key_label), "SHF"); break;
            case KC_RALT: snprintf(key_label, sizeof(key_label), "ALT"); break;
            case KC_RGUI: snprintf(key_label, sizeof(key_label), "GUI"); break;
            default: snprintf(key_label, sizeof(key_label), "KEY"); break;
        }

        // Clear the screen before drawing new text
        //qp_clear(display);

        int length = strlen(key_label); // Get length of the key label

        if (length == 1) {
            // **Use large font and center the text**
            int x_position = 20;  // Adjust X position for centering
            int y_position = 50;  // Adjust Y position for centering

            qp_drawtext_recolor(display, x_position, y_position, large_font, key_label,
                                led_config.hue, led_config.saturation, led_config.brightness,  // Foreground: Blue (HSV)
                                0, 0, 0);       // Background: Black (HSV)
            qp_flush(display);
        } else {
            // **Use default font and draw text vertically**
            int x_position = 30;
            int y_position = 25;
            char single_char[2];  // Buffer to hold a single character + null terminator

            for (int i = 0; i < length; i++) {
                single_char[0] = key_label[i];  // Copy the character
                single_char[1] = '\0';          // Null terminate

                qp_drawtext_recolor(display, x_position, y_position, font, single_char,
                                led_config.hue, led_config.saturation, led_config.brightness,  // Foreground: Blue (HSV)
                                    0, 0, 0);       // Background: Black (HSV)
                qp_flush(display);

                y_position += 40; // Move down for the next character
            }
        }
    }
    return true;
}

// Continuously check for inactivity
void matrix_scan_user(void) {
    animation_task();

    if(!animating) {
    // Get the current LED state (TODO: Finish this)
    if (host_keyboard_led_state().caps_lock && !animating) {
        // Draw a small red dot at position (0,0)
        qp_circle(display, 20, 10, 3, 255, 255, 255, true);  // Full-screen black rectangle
        qp_flush(display);
    } else {
        // Erase the red dot by drawing over it with the background color
        qp_circle(display, 20, 10, 3, 0, 0, 0, true);  // Full-screen black rectangle
        qp_flush(display);

    }
    if (host_keyboard_led_state().num_lock && !animating) {
        // Draw a small red dot at position (0,0)
        qp_circle(display, 40, 10, 3, 255, 255, 255, true);  // Full-screen black rectangle
        qp_flush(display);
    } else {
        // Erase the red dot by drawing over it with the background color
        qp_circle(display, 40, 10, 3, 0, 0, 0, true);  // Full-screen black rectangle
        qp_flush(display);

    }
    if (host_keyboard_led_state().scroll_lock && !animating) {
        // Draw a small red dot at position (0,0)
        qp_circle(display, 60, 10, 3, 255, 255, 255, true);  // Full-screen black rectangle
        qp_flush(display);
    } else {
        // Erase the red dot by drawing over it with the background color
        qp_circle(display, 60, 10, 3, 0, 0, 0, true);  // Full-screen black rectangle
        qp_flush(display);

    }
    }


}


const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT( // base layer
        KC_Q,   KC_W,   KC_E,   KC_R,   KC_T,                                         KC_Y,   KC_U,   KC_I,   KC_O,   KC_P,
        LGUI_T(KC_A),   LALT_T(KC_S),   LCTL_T(KC_D),   LSFT_T(KC_F),   KC_G,         KC_H,   RSFT_T(KC_J),   RCTL_T(KC_K), ALGR_T(KC_L),   RGUI_T(KC_SEMICOLON),
        KC_Z,   KC_X,   KC_C,   KC_V,   KC_B,         KC_N,   KC_M,   KC_COMMA,   KC_DOT,   KC_SLSH,
                                   LT(5, KC_ESCAPE), LT(1, KC_SPC), LT(6, KC_TAB),         LT(4, KC_ENTER), LT(3, KC_BACKSPACE), LT(2, KC_DELETE)
    ),
    [1] = LAYOUT( // nav layer
        KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,         KC_AGIN,   KC_PASTE,   KC_COPY,   KC_CUT,   KC_UNDO,
        KC_LGUI,   KC_LALT,   KC_LCTL,   KC_LSFT,   KC_NO,         KC_CAPS,   KC_LEFT,   KC_DOWN,   KC_UP,   KC_RIGHT,
        KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,         KC_INS,   KC_HOME,   KC_PAGE_DOWN,   KC_PAGE_UP,   KC_END,
                                   KC_NO, KC_NO, KC_NO,         KC_ENTER, KC_BACKSPACE, KC_DELETE
    ),
    [2] = LAYOUT( // function layer
        KC_F12,   KC_F7,   KC_F8,   KC_F9,   KC_PSCR,         KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,
        KC_F11,   KC_F4,   KC_F5,   KC_F6,   KC_SCRL,         KC_NO,   KC_RSFT,   KC_RCTL,   KC_RALT,   KC_RGUI,
        KC_F10,   KC_F1,   KC_F2,   KC_F3,   KC_PAUS,         KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,
                                         KC_APP, KC_SPC, KC_TAB,         KC_NO, KC_NO, KC_NO
    ),
    [3] = LAYOUT( // number layer
        KC_LBRC,   KC_7,   KC_8,   KC_9,   KC_RCBR,         KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,
        KC_SCLN,   KC_4,   KC_5,   KC_6,   KC_EQL,         KC_NO,   KC_RSFT,   KC_RCTL,   KC_RALT,   KC_RGUI,
        KC_QUOTE,   KC_1,   KC_2,   KC_3,   KC_BSLS,         KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,
                                         KC_DOT, KC_0, KC_MINUS,         KC_NO, KC_NO, KC_NO
    ),
    [4] = LAYOUT( // symbol layer
        KC_LCBR,   KC_AMPR,   KC_ASTR,   KC_LPRN,   KC_RCBR,         KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,
        KC_COLN,   KC_DLR,   KC_PERC,   KC_CIRC,   KC_PLUS,         KC_NO,   KC_RSFT,   KC_RCTL,   KC_RALT,   KC_RGUI,
        KC_TILD,   KC_EXLM,   KC_AT,   KC_HASH,   KC_PIPE,         KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,
                                         KC_LPRN, KC_RPRN, KC_UNDS,         KC_NO, KC_NO, KC_NO
    ),
    [5] = LAYOUT( // media layer
        KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,         RGB_TOG,   RGB_MOD,   RGB_HUI,   RGB_SAI,   RGB_VAI,
        KC_LGUI,   KC_LALT,   KC_LCTL,   KC_LSFT,   KC_NO,         KC_NO,   KC_MPRV,   KC_VOLD,   KC_VOLU,   KC_MNXT,
        KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,         KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,
                                         KC_NO, KC_NO, KC_NO,         KC_MSTP, KC_MPLY, KC_MUTE
    ),
    [6] = LAYOUT( // mouse layer
        KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,         KC_AGIN,   KC_PASTE,   KC_COPY,   KC_CUT,   KC_UNDO,
        KC_LGUI,   KC_LALT,   KC_LCTL,   KC_LSFT,   KC_NO,         KC_NO,   KC_MS_L,   KC_MS_D,   KC_MS_U,   KC_MS_R,
        KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,         KC_NO,   KC_WH_L,   KC_WH_D,   KC_WH_U,   KC_WH_R,
                                   KC_NO, KC_NO, KC_NO,         KC_BTN2, KC_BTN1, KC_BTN3
    )
};
