/*
Copyright 2023 @ Nuphy <https://nuphy.com/>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ansi.h"
#include "usb_main.h"
#include "print.h"

user_config_t user_config;
DEV_INFO_STRUCT dev_info =
    {
        .rf_baterry = 100,
        .link_mode  = LINK_USB,
        .rf_state   = RF_IDLE,
};

bool f_uart_ack         = 0;
bool f_bat_show         = 0;
bool f_bat_hold         = 0;
bool f_chg_show         = 1;
bool f_sys_show         = 0;
bool f_sleep_show       = 0;
bool f_func_save        = 0;
bool f_usb_offline      = 0;
bool f_rf_read_data_ok  = 0;
bool f_rf_sts_sysc_ok   = 0;
bool f_rf_new_adv_ok    = 0;
bool f_rf_reset         = 0;
bool f_send_channel     = 0;
bool f_rf_hand_ok       = 0;
bool f_dial_sw_init_ok  = 0;
bool f_goto_sleep       = 0;
bool f_wakeup_prepare   = 0;
bool f_rf_sw_press      = 0;
bool f_dev_reset_press  = 0;
bool f_rgb_test_press   = 0;
bool f_bat_num_show     = 0;

uint8_t host_mode;
host_driver_t *m_host_driver     = 0;
uint8_t  rf_sw_temp              = 0;
uint16_t rf_linking_time         = 0;
uint16_t rf_link_show_time       = 0;
uint8_t  rf_blink_cnt            = 0;
uint16_t no_act_time             = 0;
uint16_t dev_reset_press_delay   = 0;
uint16_t rf_sw_press_delay       = 0;
uint16_t rgb_test_press_delay    = 0;

extern uint8_t side_mode;
extern uint8_t side_light;
extern uint8_t side_speed;
extern uint8_t side_rgb;
extern uint8_t side_colour;
extern report_keyboard_t *keyboard_report;
extern uint8_t uart_bit_report_buf[32];
extern uint8_t bitkb_report_buf[32];
extern uint8_t bytekb_report_buf[8];

extern void m_side_led_show(void);
extern void Sleep_Handle(void);
extern void num_led_show(void);

extern void rf_uart_init(void);
extern void rf_device_init(void);
extern void dev_sts_sync(void);
extern void uart_receive_pro(void);
extern void uart_send_report_func(void);
extern uint8_t uart_send_cmd(uint8_t cmd, uint8_t ack_cnt, uint8_t delayms);
extern void uart_send_report(uint8_t report_type, uint8_t *report_buf, uint8_t report_size);

extern void device_reset_show(void);
extern void device_reset_init(void);
extern void rgb_test_show(void);

extern void light_speed_contol(uint8_t fast);
extern void light_level_control(uint8_t brighten);
extern void side_colour_control(uint8_t dir);
extern void side_mode_control(uint8_t dir);

const uint8_t side_led_index = 64;

//Modifiers
bool f_shift_on         = 0;
bool f_ctrl_on          = 0;
bool f_win_on           = 0;
bool f_alt_on           = 0;
bool f_shift_lock_on    = 0;
bool f_ctrl_lock_on     = 0;
bool f_win_lock_on      = 0;
bool f_alt_lock_on      = 0;
//const is31_led PROGMEM g_is31_leds[] (keymap.c)
const uint8_t tab_led_index         = 14; //Tab
const uint8_t caps_led_index        = 28; //Caps
const uint8_t lshift_led_index      = 41; //L_Shift
const uint8_t rshift_led_index      = 52; //R_Shift
const uint8_t del_led_index         = 54; //Del
const uint8_t ctrl_led_index        = 55; //Ctrl
const uint8_t win_led_index         = 56; //Win
const uint8_t alt_led_index         = 57; //Alt
const uint8_t opt_led_index         = 59;
const uint8_t cmd_led_index         = 60;
static bool wiggle_trigger          = false;
static uint32_t matrix_scan_timer   = 0;

const uint8_t modifier_leds[9]= {0, tab_led_index, caps_led_index, lshift_led_index, ctrl_led_index, win_led_index, alt_led_index, opt_led_index, cmd_led_index};

/**
 * @brief  gpio initial.
 */
void m_gpio_init(void)
{
    setPinOutput(DC_BOOST_PIN); writePinHigh(DC_BOOST_PIN);

    // Initializes the RGB Driver SDB pin
    setPinOutput(RGB_DRIVER_SDB1); writePinHigh(RGB_DRIVER_SDB1);
    setPinOutput(RGB_DRIVER_SDB2); writePinHigh(RGB_DRIVER_SDB2);

    // RF wake up pin configuration
    setPinOutput(NRF_WAKEUP_PIN);
    writePinHigh(NRF_WAKEUP_PIN);

    // RFboot Control pin
    setPinInputHigh(NRF_BOOT_PIN);

    // RF reset pin configuration
    setPinOutput(NRF_RESET_PIN); writePinLow(NRF_RESET_PIN);
    wait_ms(50);
    writePinHigh(NRF_RESET_PIN);

    // Switch detection pin
    setPinInputHigh(DEV_MODE_PIN);
    setPinInputHigh(SYS_MODE_PIN);
}

/**
 * @brief  long press key process.
 */
void long_press_key(void)
{
    static uint32_t long_press_timer = 0;

    if (timer_elapsed32(long_press_timer) < 100) return;
    long_press_timer = timer_read32();

    // Open a new RF device
    if (f_rf_sw_press) {
        rf_sw_press_delay++;
        if (rf_sw_press_delay >= RF_LONG_PRESS_DELAY) {
            f_rf_sw_press        = 0;

            dev_info.link_mode   = rf_sw_temp;
            dev_info.rf_channel  = rf_sw_temp;
            dev_info.ble_channel = rf_sw_temp;

            uint8_t timeout = 5;
            while (timeout--) {
                uart_send_cmd(CMD_NEW_ADV, 0, 1);
                wait_ms(20);
                uart_receive_pro();
                if (f_rf_new_adv_ok) break;
            }
        }
    } else {
        rf_sw_press_delay = 0;
    }

    // The device is restored to factory Settings
    if (f_dev_reset_press) {
        dev_reset_press_delay++;
        if (dev_reset_press_delay >= DEV_RESET_PRESS_DELAY) {
            f_dev_reset_press = 0;

            if (dev_info.link_mode != LINK_USB) {
                if (dev_info.link_mode != LINK_RF_24) {
                    dev_info.link_mode      = LINK_BT_1;
                    dev_info.ble_channel    = LINK_BT_1;
                    dev_info.rf_channel     = LINK_BT_1;
                }
            } else {
                dev_info.ble_channel = LINK_BT_1;
                dev_info.rf_channel  = LINK_BT_1;
            }

            uart_send_cmd(CMD_SET_LINK, 10, 10);
            wait_ms(500);
            uart_send_cmd(CMD_CLR_DEVICE, 10, 10);

            eeconfig_init();
            device_reset_show();
            device_reset_init();

            if (dev_info.sys_sw_state == SYS_SW_MAC) {
                default_layer_set(1 << 0);  // MAC
                keymap_config.nkro = 0;
            } else {
                default_layer_set(1 << 3);  // WIN
                keymap_config.nkro = 1;
            }
        }
    } else {
        dev_reset_press_delay = 0;
    }

    // Enter the RGB test mode
    if (f_rgb_test_press) {
        rgb_test_press_delay++;
        if (rgb_test_press_delay >= RGB_TEST_PRESS_DELAY) {
            f_rgb_test_press = 0;
            rgb_test_show();
        }
    } else {
        rgb_test_press_delay = 0;
    }
}

/**
 * @brief  Release all keys, clear keyboard report.
 */
void m_break_all_key(void)
{
    uint8_t report_buf[16];
    bool nkro_temp = keymap_config.nkro;

    clear_weak_mods();
    clear_mods();
    clear_keyboard();

    keymap_config.nkro = 1;
    memset(keyboard_report, 0, sizeof(report_keyboard_t));
    host_keyboard_send(keyboard_report);
    wait_ms(10);

    keymap_config.nkro = 0;
    memset(keyboard_report, 0, sizeof(report_keyboard_t));
    host_keyboard_send(keyboard_report);
    wait_ms(10);

    keymap_config.nkro = nkro_temp;

    if (dev_info.link_mode != LINK_USB) {
        memset(report_buf, 0, 16);
        uart_send_report(CMD_RPT_BIT_KB, report_buf, 16);
        wait_ms(10);
        uart_send_report(CMD_RPT_BYTE_KB, report_buf, 8);
        wait_ms(10);
    }

    memset(uart_bit_report_buf, 0, sizeof(uart_bit_report_buf));
    memset(bitkb_report_buf, 0, sizeof(bitkb_report_buf));
    memset(bytekb_report_buf, 0, sizeof(bytekb_report_buf));
}

/**
 * @brief  switch device link mode.
 * @param mode : link mode
 */
static void switch_dev_link(uint8_t mode)
{
    if (mode > LINK_USB) return;
    m_break_all_key();

    dev_info.link_mode = mode;
    dev_info.rf_state = RF_IDLE;
    f_send_channel    = 1;

    if (mode == LINK_USB) {
        host_mode = HOST_USB_TYPE;
        host_set_driver(m_host_driver);
        rf_link_show_time = 0;
    }
    else {
        host_mode = HOST_RF_TYPE;
        host_set_driver(0);
    }
}

/**
 * @brief  scan dial switch.
 */
void dial_sw_scan(void)
{
    uint8_t dial_scan               = 0;
    static uint8_t dial_save        = 0xf0;
    static uint8_t debounce         = 0;
    static uint32_t dial_scan_timer = 0;
    static bool     f_first         = true;

    if (!f_first) {
        if (timer_elapsed32(dial_scan_timer) < 20) return;
    }
    dial_scan_timer = timer_read32();

    setPinInputHigh(DEV_MODE_PIN);
    setPinInputHigh(SYS_MODE_PIN);

    if (readPin(DEV_MODE_PIN)) dial_scan |= 0X01;
    if (readPin(SYS_MODE_PIN)) dial_scan |= 0X02;

    if (dial_save != dial_scan) {
        m_break_all_key();

        no_act_time     = 0;
        rf_linking_time = 0;

        dial_save         = dial_scan;
        debounce          = 25;
        f_dial_sw_init_ok = 0;
        return;
    } else if (debounce) {
        debounce--;
        return;
    }

    if (dial_scan & 0x01) {
        if (dev_info.link_mode != LINK_USB) {
            switch_dev_link(LINK_USB);
        }
    } else {
        if (dev_info.link_mode != dev_info.rf_channel) {
            switch_dev_link(dev_info.rf_channel);
        }
    }

    if (dial_scan & 0x02) {
        if (dev_info.sys_sw_state != SYS_SW_WIN) {
            f_sys_show = 1;
            default_layer_set(1 << 3);
            dev_info.sys_sw_state = SYS_SW_WIN;
            keymap_config.nkro    = 1;
            m_break_all_key();
        }
    } else {
        if (dev_info.sys_sw_state != SYS_SW_MAC) {
            f_sys_show = 1;
            default_layer_set(1 << 0);
            dev_info.sys_sw_state = SYS_SW_MAC;
            keymap_config.nkro    = 0;
            m_break_all_key();
        }
    }

    if (f_dial_sw_init_ok == 0) {
        f_dial_sw_init_ok = 1;
        f_first           = false;
        if (dev_info.link_mode != LINK_USB) {
            host_set_driver(0);
        }
    }
}

/**
 * @brief  power on scan dial switch.
 */
void m_power_on_dial_sw_scan(void)
{
    uint8_t dial_scan_dev = 0;
    uint8_t dial_scan_sys = 0;
    uint8_t dial_check_dev = 0;
    uint8_t dial_check_sys = 0;
    uint8_t debounce = 0;

    setPinInputHigh(DEV_MODE_PIN);
    setPinInputHigh(SYS_MODE_PIN);

    // Debounce to get a stable state
    for(debounce=0; debounce<10; debounce++) {
        dial_scan_dev = 0;
        dial_scan_sys = 0;
        if (readPin(DEV_MODE_PIN)) dial_scan_dev = 0x01;
        else dial_scan_dev = 0;
        if (readPin(SYS_MODE_PIN)) dial_scan_sys = 0x01;
        else dial_scan_sys = 0;
        if((dial_scan_dev != dial_check_dev)||(dial_scan_sys != dial_check_sys))
        {
            dial_check_dev = dial_scan_dev;
            dial_check_sys = dial_scan_sys;
            debounce = 0;
        }
        wait_ms(1);
    }

    // RF link mode
    if (dial_scan_dev) {
        if (dev_info.link_mode != LINK_USB) {
            switch_dev_link(LINK_USB);
        }
    } else {
        if (dev_info.link_mode != dev_info.rf_channel) {
            switch_dev_link(dev_info.rf_channel);
        }
    }

    // Win or Mac
    if (dial_scan_sys) {
        if (dev_info.sys_sw_state != SYS_SW_WIN) {
            default_layer_set(1 << 3);
            dev_info.sys_sw_state = SYS_SW_WIN;
            keymap_config.nkro    = 1;
            m_break_all_key();
        }
    } else {
        if (dev_info.sys_sw_state != SYS_SW_MAC) {
            default_layer_set(1 << 0);
            dev_info.sys_sw_state = SYS_SW_MAC;
            keymap_config.nkro    = 0;
            m_break_all_key();
        }
    }
}

/**
 * @brief  qmk process record
 */
bool process_record_user(uint16_t keycode, keyrecord_t *record)
{
    switch (keycode) {
        case RF_DFU:
            if (record->event.pressed) {
                if (dev_info.link_mode != LINK_USB) return false;
                uart_send_cmd(CMD_RF_DFU, 10, 20);
            }
            return false;

        case LNK_USB:
            if (record->event.pressed) {
                m_break_all_key();
            } else {
                dev_info.link_mode = LINK_USB;
                uart_send_cmd(CMD_SET_LINK, 10, 10);
            }
            return false;

        case LNK_RF:
            if (record->event.pressed) {
                if (dev_info.link_mode != LINK_USB) {
                    rf_sw_temp    = LINK_RF_24;
                    f_rf_sw_press = 1;
                    m_break_all_key();
                }
            } else if (f_rf_sw_press) {
                f_rf_sw_press = 0;
                if (rf_sw_press_delay < RF_LONG_PRESS_DELAY) {
                    dev_info.link_mode   = rf_sw_temp;
                    dev_info.rf_channel  = rf_sw_temp;
                    dev_info.ble_channel = rf_sw_temp;
                    uart_send_cmd(CMD_SET_LINK, 10, 20);
                }
            }
            return false;

        case LNK_BLE1:
            if (record->event.pressed) {
                if (dev_info.link_mode != LINK_USB) {
                    rf_sw_temp    = LINK_BT_1;
                    f_rf_sw_press = 1;
                    m_break_all_key();
                }
            } else if (f_rf_sw_press) {
                f_rf_sw_press = 0;
                if (rf_sw_press_delay < RF_LONG_PRESS_DELAY) {
                    dev_info.link_mode   = rf_sw_temp;
                    dev_info.rf_channel  = rf_sw_temp;
                    dev_info.ble_channel = rf_sw_temp;
                    uart_send_cmd(CMD_SET_LINK, 10, 20);
                }
            }
            return false;

        case LNK_BLE2:
            if (record->event.pressed) {
                if (dev_info.link_mode != LINK_USB) {
                    rf_sw_temp    = LINK_BT_2;
                    f_rf_sw_press = 1;
                    m_break_all_key();
                }
            } else if (f_rf_sw_press) {
                f_rf_sw_press = 0;
                if (rf_sw_press_delay < RF_LONG_PRESS_DELAY) {
                    dev_info.link_mode   = rf_sw_temp;
                    dev_info.rf_channel  = rf_sw_temp;
                    dev_info.ble_channel = rf_sw_temp;
                    uart_send_cmd(CMD_SET_LINK, 10, 20);
                }
            }
            return false;

        case LNK_BLE3:
            if (record->event.pressed) {
                if (dev_info.link_mode != LINK_USB) {
                    rf_sw_temp    = LINK_BT_3;
                    f_rf_sw_press = 1;
                    m_break_all_key();
                }
            } else if (f_rf_sw_press) {
                f_rf_sw_press = 0;
                if (rf_sw_press_delay < RF_LONG_PRESS_DELAY) {
                    dev_info.link_mode   = rf_sw_temp;
                    dev_info.rf_channel  = rf_sw_temp;
                    dev_info.ble_channel = rf_sw_temp;
                    uart_send_cmd(CMD_SET_LINK, 10, 20);
                }
            }
            return false;

        case MAC_TASK:
            if (record->event.pressed) {
                host_consumer_send(0x029F);
            } else {
                host_consumer_send(0);
            }
            return false;

        case MAC_SEARCH:
            if (record->event.pressed) {
                register_code(KC_LGUI);
                register_code(KC_SPACE);
                wait_ms(50);
                unregister_code(KC_LGUI);
                unregister_code(KC_SPACE);
            }
            return false;

        case MAC_VOICE:
            if (record->event.pressed) {
                host_consumer_send(0xcf);
            } else {
                host_consumer_send(0);
            }
            return false;

        case MAC_CONSOLE:
            if (record->event.pressed) {
                host_consumer_send(0x02A0);
            } else {
                host_consumer_send(0);
            }
            return false;

        case MAC_DND:
            if (record->event.pressed) {
                host_system_send(0x9b);
            } else {
                host_system_send(0);
            }
            return false;

        case MAC_PRT:
            if (record->event.pressed) {
                register_code(KC_LGUI);
                register_code(KC_LSFT);
                register_code(KC_3);
                wait_ms(50);
                unregister_code(KC_3);
                unregister_code(KC_LSFT);
                unregister_code(KC_LGUI);
            }
            return false;

        case MAC_PRTA:
            if (record->event.pressed) {
                if (keymap_config.nkro) {
                    register_code(KC_LGUI);
                    register_code(KC_LSFT);
                    register_code(KC_S);
                    wait_ms(50);
                    unregister_code(KC_S);
                    unregister_code(KC_LSFT);
                    unregister_code(KC_LGUI);
                }
                else {
                    register_code(KC_LGUI);
                    register_code(KC_LSFT);
                    register_code(KC_4);
                    wait_ms(50);
                    unregister_code(KC_4);
                    unregister_code(KC_LSFT);
                    unregister_code(KC_LGUI);
                }
            }
            return false;

        case SIDE_VAI:
            if (record->event.pressed) {
                light_level_control(1);
            }
            return false;

        case SIDE_VAD:
            if (record->event.pressed) {
                light_level_control(0);
            }
            return false;

        case SIDE_MOD:
            if (record->event.pressed) {
                side_mode_control(1);
            }
            return false;

        case SIDE_HUI:
            if (record->event.pressed) {
                side_colour_control(1);
            }
            return false;

        case SIDE_SPI:
            if (record->event.pressed) {
                light_speed_contol(1);
            }
            return false;

        case SIDE_SPD:
            if (record->event.pressed) {
                light_speed_contol(0);
            }
            return false;

        case DEV_RESET:
            if (record->event.pressed) {
                f_dev_reset_press = 1;
                m_break_all_key();
            } else {
                f_dev_reset_press = 0;
            }
            return false;

        case SLEEP_MODE:
            if (record->event.pressed) {
                if(user_config.sleep_enable) user_config.sleep_enable = false;
                else user_config.sleep_enable = true;
                f_sleep_show       = 1;
                eeconfig_update_user_datablock(&user_config);
            }
            return false;

        case BAT_SHOW:
            if (record->event.pressed) {
                f_bat_hold = !f_bat_hold;
            }
            return false;

        case RGB_TEST:
            if (record->event.pressed) {
                f_rgb_test_press = 1;
            } else {
                f_rgb_test_press = 0;
            }
            return false;

        case SHIFT_GRV:
            if (record->event.pressed) {
                register_code(KC_LSFT);
                register_code(KC_GRV);
            }
            else {
                unregister_code(KC_LSFT);
                unregister_code(KC_GRV);
            }
            return false;
        case BAT_NUM:
            if (record->event.pressed) {
                f_bat_num_show = 1;
            } else {
                f_bat_num_show = 0;
            }
            return false;
        case RGB_HEAT_MAP:  //C:\dev\qmk_firmware\docs\feature_rgb_matrix.md
            if (record->event.pressed) {
                rgb_matrix_mode(RGB_MATRIX_TYPING_HEATMAP);
            }
            return false;
        case RGB_GRD_LEFT_RIGHT:
            if (record->event.pressed) {
                rgb_matrix_mode(RGB_MATRIX_GRADIENT_LEFT_RIGHT);
            }
            return false;
        case RGB_GRD_UP_DOWN:
            if (record->event.pressed) {
                rgb_matrix_mode(RGB_MATRIX_GRADIENT_UP_DOWN);
            }
            return false;
        case RGB_SPLASH:
            if (record->event.pressed) {
                rgb_matrix_mode(RGB_MATRIX_SPLASH);
            }
            return false;
        case CLEAR_MODS:
            if (record->event.pressed) {
                m_break_all_key();
                rgb_matrix_reload_from_eeprom();
            }
            return false;
        case WIGGLE:
            if (record->event.pressed) {
                wiggle_trigger ^= true;
            }
            return false;
        /*
        case RGB_SOLID_RED:
            if (record->event.pressed) {
                rgb_matrix_mode(RGB_MATRIX_SOLID_COLOR);        
                rgb_matrix_set_color_all(0xFF, 0x00, 0x00);
            }
            return false;
        case RGB_SOLID_GREEN:
            if (record->event.pressed) {
                rgb_matrix_mode(RGB_MATRIX_SOLID_COLOR);        
                rgb_matrix_set_color_all(0x00, 0xFF, 0x00);
            }
            return false;
        case RGB_SOLID_BLUE:
            if (record->event.pressed) {
                rgb_matrix_mode(RGB_MATRIX_SOLID_COLOR);            
                rgb_matrix_set_color_all(0x00, 0x00, 0xFF);
            }
            return false;
        //ACTION_TAP_DANCE_LAYER_TOGGLE(kc, layer): Sends the kc keycode when tapped once, or toggles the state of layer. (this functions like the TG layer keycode).
        */
        default:
            return true;
    }
}

/**
    @brief  timer process.
 */
void timer_pro(void)
{
    static uint32_t interval_timer = 0;
    static bool f_first            = true;

    if (f_first) {
        f_first        = false;
        interval_timer = timer_read32();
        m_host_driver  = host_get_driver();
    }

    if (timer_elapsed32(interval_timer) < 10) {
        return;
    } else if (timer_elapsed32(interval_timer) > 20) {
        interval_timer = timer_read32();
    } else {
        interval_timer += 10;
    }

    if (rf_link_show_time < RF_LINK_SHOW_TIME)
        rf_link_show_time++;

    if (no_act_time < 0xffff)
        no_act_time++;

    if (rf_linking_time < 0xffff)
        rf_linking_time++;
}

/**
 * @brief  londing eeprom data.
 */
void m_londing_eeprom_data(void)
{
    eeconfig_read_user_datablock(&user_config);
    if (user_config.default_brightness_flag != 0xA5) {
        rgb_matrix_sethsv(255, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS - RGB_MATRIX_VAL_STEP * 2);
        user_config.default_brightness_flag = 0xA5;
        user_config.ee_side_mode            = side_mode;
        user_config.ee_side_light           = side_light;
        user_config.ee_side_speed           = side_speed;
        user_config.ee_side_rgb             = side_rgb;
        user_config.ee_side_colour          = side_colour;
        user_config.sleep_enable            = true;
        eeconfig_update_user_datablock(&user_config);
    } else {
        side_mode   = user_config.ee_side_mode;
        side_light  = user_config.ee_side_light;
        side_speed  = user_config.ee_side_speed;
        side_rgb    = user_config.ee_side_rgb;
        side_colour = user_config.ee_side_colour;
    }
}

void keyboard_post_init_user(void)
{
    m_gpio_init();
    rf_uart_init();
    wait_ms(500);
    rf_device_init();

    m_break_all_key();
    m_londing_eeprom_data();
    m_power_on_dial_sw_scan();
}

/**
   housekeeping_task_user
 */
void housekeeping_task_user(void)
{
    timer_pro();

    uart_receive_pro();

    uart_send_report_func();

    dev_sts_sync();

    long_press_key();

    dial_sw_scan();

    m_side_led_show();

    Sleep_Handle();
}

void matrix_scan_user(void) { 
    if (timer_elapsed32(matrix_scan_timer) > 30000) { // 30 seconds
        matrix_scan_timer = timer_read32();  // resets timer
        if (wiggle_trigger) {
            tap_code(KC_F24); // tap if enabled
        }
    }
}

void toBinary(uint8_t a)
{
    uint8_t i;

    for(i=0x80;i!=0;i>>=1)
        uprintf("%c",(a&i)?'1':'0');

    print("\n");
}

void oneshot_mods_changed_user(uint8_t mods) {
    print("osm changed. mods: ");
    toBinary(mods);

    if (mods & MOD_MASK_SHIFT) {
        f_shift_on = true;
    }
    else if (mods & MOD_MASK_CTRL) {
        f_ctrl_on = true;
    }
    else if (mods & MOD_MASK_GUI) {
        f_win_on = true;
    }
    else if (mods & MOD_MASK_ALT) {
        f_alt_on = true;
    }
    else {
        f_shift_on = false;
        f_ctrl_on = false;
        f_win_on = false;
        f_alt_on = false;
    }
}

void oneshot_locked_mods_changed_user(uint8_t mods) {
    print("osm locked changed. mods: ");
    toBinary(mods);

    f_shift_lock_on = mods & MOD_MASK_SHIFT;
    f_ctrl_lock_on = mods & MOD_MASK_CTRL;
    f_win_lock_on = mods & MOD_MASK_GUI;
    f_alt_lock_on = mods & MOD_MASK_ALT;
}

/**
    @brief https://www.vandelaydesign.com/beach-color-palettes/ (White Sand)
*/
void turn_off_unless_active_modifier(uint8_t ledIndex) {
    if (ledIndex == tab_led_index && wiggle_trigger) {
        rgb_matrix_set_color(ledIndex, 0x00, 0xFF, 0xFF);
    }
    else if (ledIndex == lshift_led_index && (f_shift_on || f_shift_lock_on)) {
        //rgb_matrix_set_color(ledIndex, 0x83, 0xdf, 0xe3);
        rgb_matrix_set_color(ledIndex, 0x00, 0xFF, 0xFF);
    }
    else if (ledIndex == ctrl_led_index && (f_ctrl_on || f_ctrl_lock_on)) {
        //rgb_matrix_set_color(ledIndex, 0x35, 0xB2, 0x34);  //TD Green
        rgb_matrix_set_color(ledIndex, 0xCE, 0xC8, 0xC1);
    }
    else if (ledIndex == win_led_index && (f_win_on || f_win_lock_on)) {
        rgb_matrix_set_color(ledIndex, 0xF6, 0xF0, 0xFC);
    }
    else if (ledIndex == alt_led_index && (f_alt_on || f_alt_lock_on)) {
        rgb_matrix_set_color(ledIndex, 0x94, 0xA7, 0xA8);
    }
    else {
        rgb_matrix_set_color(ledIndex, 0x00, 0x00, 0x00);
    }
}

/**
   rgb_matrix_indicators_user
 */
bool rgb_matrix_indicators_user(void)
{
    if(f_bat_num_show) {
        num_led_show();
    }
    else {
        uint8_t current_layer = get_highest_layer(layer_state);
        switch (current_layer) {
            case 4:
                for (int i = 0; i < side_led_index; i++) {
                    if (i == caps_led_index) {
                        //rgb_matrix_set_color(i, 0x26, 0x8f, 0x8e);
                        rgb_matrix_set_color(i, 0x00, 0xFF, 0xFF);                        
                    }
                    //Function Row
                    else if (i >= 1 && i <= 12) {
                        rgb_matrix_set_color(i, 0xFF, 0x00, 0x00);              //Maroon
                    }
                    //Nav Keys
                    else if (i == 53 || (i >= 61 && i <= 63)) {
                        rgb_matrix_set_color(i, 0xFF, 0x00, 0x00);              //Maroon
                    }
                    else {
                        turn_off_unless_active_modifier(i);
                    }
                }
                break;
            //Lighting Layer
            case 5:
                for (int i = 0; i < 9; i++) {
                    if (modifier_leds[i] == opt_led_index) {
                        rgb_matrix_set_color(modifier_leds[i], 0x00, 0xFF, 0xFF);
                    }
                    else {
                        turn_off_unless_active_modifier(modifier_leds[i]);
                    }
                }
                break;
            //Cmd Layer
            case 6:
                for (int i = 0; i < 9; i++) {
                    if (modifier_leds[i] == cmd_led_index) {
                        rgb_matrix_set_color(modifier_leds[i], 0x00, 0xFF, 0xFF);
                    }
                    else {
                        turn_off_unless_active_modifier(modifier_leds[i]);
                    }
                }
                break;
            default:
                //Works because it doesn't pass through here on the Caps Lock case
                if (f_shift_on || f_shift_lock_on || f_ctrl_on || f_ctrl_lock_on || f_alt_on || f_alt_lock_on || f_win_on || f_win_lock_on || wiggle_trigger) {  
                    for (int i = 0; i < 9; i++) {
                        turn_off_unless_active_modifier(modifier_leds[i]);
                    }
                }
            break;
        }
    }

    return true;
}
