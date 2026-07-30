/*
 * esp32_hid_xt_table.h - USB HID Usage Page 0x07 (Keyboard/Keypad) -> XT
 * scan code (set 1) translation table, shared by every keyboard source
 * that reports HID usage codes on this port: the physical Tab5 I2C
 * keyboard (esp32_tab5_kbd.cpp, HID mode) and, once wired up, a USB HID
 * boot-protocol keyboard.
 *
 * Defined in esp32_hid_xt_table.c (plain C) rather than as inline
 * `static const` arrays in this header: g++ does not support sparse
 * designated array initializers ("sorry, unimplemented: non-trivial
 * designated initializers not supported") the way GCC's C frontend
 * does, and esp32_tab5_kbd.cpp (C++, since the M5Stack keyboard
 * component is a C++ class) needs these tables too - tiny386's
 * kbd_tab5.cpp hit the same g++ limitation and worked around it with a
 * switch statement instead; a separate C translation unit is simpler
 * here since the table is shared by both a .c and a .cpp caller.
 */
#ifndef ESP32_HID_XT_TABLE_H
#define ESP32_HID_XT_TABLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint16_t hid_usage_to_xt[256];
extern const uint16_t hid_modifier_to_xt[8];

#ifdef __cplusplus
}
#endif

#endif /* ESP32_HID_XT_TABLE_H */
