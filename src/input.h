#pragma once

#include <cstdint>
#include <string>

namespace reshade::api
{
	struct effect_runtime;
}

// Unified input handling for both keyboard and gamepad (XInput).
// A key is a uint32_t: byte0 = main key (VK code), byte1 = Ctrl, byte2 = Shift,
// byte3 = Alt. Bit 31 (GP_FLAG) marks a gamepad button (low byte = XInput index).

// True on the frame the given key (or gamepad button) is pressed.
bool are_keys_pressed(uint32_t keys, reshade::api::effect_runtime *runtime);

// Returns the most recently pressed key/button, or 0 if none this frame.
uint32_t reshade_last_key_pressed(reshade::api::effect_runtime *runtime);

// Human-readable name for a key value (e.g. "Ctrl + M", "Pad A").
std::string key_name(uint32_t keys);

// ImGui widget that captures a key/button into *keys. Returns true when changed.
bool key_capture(const char *label, uint32_t *keys, reshade::api::effect_runtime *runtime);
