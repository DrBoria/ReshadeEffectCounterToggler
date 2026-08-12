#pragma once

#include <cstdint>

// Marks a key value as a gamepad button (low byte = XInput button index).
#define GP_FLAG 0x80000000u

// XInput gamepad button bit flags (Gamepad.wButtons).
#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB 0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB 0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER 0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_X 0x4000
#define XINPUT_GAMEPAD_Y 0x8000

// Number of supported gamepad buttons.
inline constexpr uint32_t kGamepadButtonCount = 14;

// INI section used for persisting groups and fallback keys.
inline constexpr const char *kConfigSection = "ReshadeEffectCounterToggler";
