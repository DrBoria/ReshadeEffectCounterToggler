#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>
#include <imgui.h>
#include <reshade.hpp>

#include "constants.h"
#include "input.h"

using namespace reshade::api;

// ---------------------------------------------------------------------------
// XInput gamepad support (dynamically loaded; not linked at build time).
// ---------------------------------------------------------------------------

struct XInputGamepad
{
	WORD wButtons;
	BYTE bLeftTrigger;
	BYTE bRightTrigger;
	SHORT sThumbLX;
	SHORT sThumbLY;
	SHORT sThumbRX;
	SHORT sThumbRY;
};

struct XInputState
{
	DWORD dwPacketNumber;
	XInputGamepad Gamepad;
};

typedef DWORD(WINAPI *XInputGetState_t)(DWORD, XInputState *);
static XInputGetState_t g_xinput_get_state = nullptr;
static HMODULE g_xinput_module = nullptr;

static void init_xinput()
{
	if (g_xinput_module != nullptr)
		return;
	const char *names[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
	for (const char *name : names)
	{
		g_xinput_module = LoadLibraryA(name);
		if (g_xinput_module != nullptr)
		{
			g_xinput_get_state = reinterpret_cast<XInputGetState_t>(GetProcAddress(g_xinput_module, "XInputGetState"));
			if (g_xinput_get_state != nullptr)
				return;
			FreeLibrary(g_xinput_module);
			g_xinput_module = nullptr;
		}
	}
}

static uint16_t gamepad_buttons()
{
	init_xinput();
	if (g_xinput_get_state == nullptr)
		return 0;
	XInputState state = {};
	if (g_xinput_get_state(0, &state) != 0)
		return 0;
	return state.Gamepad.wButtons;
}

static const uint16_t g_gamepad_button_flags[] = {
	XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT,
	XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
	XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B,
	XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y
};

static const char *g_gamepad_button_names[] = {
	"Pad DPad Up", "Pad DPad Down", "Pad DPad Left", "Pad DPad Right",
	"Pad Start", "Pad Back", "Pad LThumb", "Pad RThumb",
	"Pad LB", "Pad RB", "Pad A", "Pad B", "Pad X", "Pad Y"
};

// Gamepad buttons are level-triggered (held while pressed), unlike keyboard keys
// which are edge-triggered. We only want to act on a quick click (press + release
// within a short window). Holding a button (long press) must NOT trigger anything.
static constexpr DWORD kClickMaxDurationMs = 300;

static uint16_t g_prev_gamepad_buttons = 0;
static uint16_t g_gamepad_clicks = 0;      // buttons that completed a quick click this frame
static uint16_t g_gamepad_held = 0;        // buttons currently held down
static DWORD g_gamepad_press_time[kGamepadButtonCount] = {};

void poll_gamepad()
{
	uint16_t buttons = gamepad_buttons();
	uint16_t rising = buttons & ~g_prev_gamepad_buttons;
	uint16_t falling = g_prev_gamepad_buttons & ~buttons;
	DWORD now = GetTickCount();

	// Clear clicks from the previous frame so each click is consumed exactly once.
	g_gamepad_clicks = 0;

	for (uint8_t i = 0; i < kGamepadButtonCount; i++)
	{
		uint16_t flag = g_gamepad_button_flags[i];
		if (rising & flag)
		{
			g_gamepad_press_time[i] = now;
			g_gamepad_held |= flag;
		}
		if (falling & flag)
		{
			g_gamepad_held &= ~flag;
			// Only a quick press+release counts as a click; a long hold does not.
			if (now - g_gamepad_press_time[i] <= kClickMaxDurationMs)
				g_gamepad_clicks |= flag;
		}
	}

	g_prev_gamepad_buttons = buttons;
}

static bool gamepad_button_pressed(uint8_t index)
{
	if (index >= kGamepadButtonCount)
		return false;
	return (g_gamepad_clicks & g_gamepad_button_flags[index]) != 0;
}

static uint32_t gamepad_last_button_pressed()
{
	for (uint8_t i = 0; i < kGamepadButtonCount; i++)
		if ((g_gamepad_clicks & g_gamepad_button_flags[i]) != 0)
			return GP_FLAG | i;
	return 0;
}

// ---------------------------------------------------------------------------
// Unified key handling (keyboard + gamepad).
// ---------------------------------------------------------------------------

bool are_keys_pressed(uint32_t keys, effect_runtime *runtime)
{
	if ((keys & GP_FLAG) != 0)
		return gamepad_button_pressed(static_cast<uint8_t>(keys & 0xFF));

	uint8_t key0 = keys & 0xFF;
	uint8_t key1 = (keys >> 8) & 0xFF;
	uint8_t key2 = (keys >> 16) & 0xFF;
	uint8_t key3 = (keys >> 24) & 0xFF;

	bool ret = runtime->is_key_pressed(key0);
	ret = ret && (key1 == 0 || runtime->is_key_down(0x11));
	ret = ret && (key2 == 0 || runtime->is_key_down(0x10));
	ret = ret && (key3 == 0 || runtime->is_key_down(0x12));
	return ret;
}

uint32_t reshade_last_key_pressed(effect_runtime *runtime)
{
	uint32_t gp = gamepad_last_button_pressed();
	if (gp != 0)
		return gp;
	for (uint32_t i = 0x06; i < 256; i++)
		if (runtime->is_key_pressed(static_cast<uint8_t>(i)))
			return static_cast<uint8_t>(i);
	return 0;
}

static std::string vk_code_name(uint8_t key)
{
	switch (key)
	{
	case 0x01: return "LMB";
	case 0x02: return "RMB";
	case 0x04: return "MMB";
	case 0x08: return "Backspace";
	case 0x09: return "Tab";
	case 0x0D: return "Enter";
	case 0x1B: return "Esc";
	case 0x20: return "Space";
	case 0x21: return "PageUp";
	case 0x22: return "PageDown";
	case 0x23: return "End";
	case 0x24: return "Home";
	case 0x25: return "Left";
	case 0x26: return "Up";
	case 0x27: return "Right";
	case 0x28: return "Down";
	case 0x2D: return "Insert";
	case 0x2E: return "Delete";
	case 0x41: return "A";
	case 0x42: return "B";
	case 0x43: return "C";
	case 0x44: return "D";
	case 0x45: return "E";
	case 0x46: return "F";
	case 0x47: return "G";
	case 0x48: return "H";
	case 0x49: return "I";
	case 0x4A: return "J";
	case 0x4B: return "K";
	case 0x4C: return "L";
	case 0x4D: return "M";
	case 0x4E: return "N";
	case 0x4F: return "O";
	case 0x50: return "P";
	case 0x51: return "Q";
	case 0x52: return "R";
	case 0x53: return "S";
	case 0x54: return "T";
	case 0x55: return "U";
	case 0x56: return "V";
	case 0x57: return "W";
	case 0x58: return "X";
	case 0x59: return "Y";
	case 0x5A: return "Z";
	case 0x60: return "Num0";
	case 0x61: return "Num1";
	case 0x62: return "Num2";
	case 0x63: return "Num3";
	case 0x64: return "Num4";
	case 0x65: return "Num5";
	case 0x66: return "Num6";
	case 0x67: return "Num7";
	case 0x68: return "Num8";
	case 0x69: return "Num9";
	case 0x70: return "F1";
	case 0x71: return "F2";
	case 0x72: return "F3";
	case 0x73: return "F4";
	case 0x74: return "F5";
	case 0x75: return "F6";
	case 0x76: return "F7";
	case 0x77: return "F8";
	case 0x78: return "F9";
	case 0x79: return "F10";
	case 0x7A: return "F11";
	case 0x7B: return "F12";
	case 0x7C: return "F13";
	case 0x7D: return "F14";
	case 0x7E: return "F15";
	case 0x7F: return "F16";
	case 0x80: return "F17";
	case 0x81: return "F18";
	case 0x82: return "F19";
	case 0x83: return "F20";
	case 0x84: return "F21";
	case 0x85: return "F22";
	case 0x86: return "F23";
	case 0x87: return "F24";
	case 0x90: return "NumLock";
	case 0x91: return "ScrollLock";
	case 0xBA: return ";";
	case 0xBB: return "=";
	case 0xBC: return ",";
	case 0xBD: return "-";
	case 0xBE: return ".";
	case 0xBF: return "/";
	case 0xC0: return "`";
	case 0xDB: return "[";
	case 0xDC: return "\\";
	case 0xDD: return "]";
	case 0xDE: return "'";
	default:
	{
		char buf[16];
		snprintf(buf, sizeof(buf), "0x%02X", key);
		return buf;
	}
	}
}

std::string key_name(uint32_t keys)
{
	if ((keys & GP_FLAG) != 0)
		return g_gamepad_button_names[keys & 0xFF];

	uint8_t key0 = keys & 0xFF;
	uint8_t key1 = (keys >> 8) & 0xFF;
	uint8_t key2 = (keys >> 16) & 0xFF;
	uint8_t key3 = (keys >> 24) & 0xFF;

	if (key0 == 0)
		return "None";

	return (key1 ? "Ctrl + " : std::string()) + (key2 ? "Shift + " : std::string()) + (key3 ? "Alt + " : std::string()) + vk_code_name(key0);
}

bool key_capture(const char *label, uint32_t *keys, effect_runtime *runtime)
{
	char buf[64];
	std::string name = key_name(*keys);
	strncpy_s(buf, sizeof(buf), name.c_str(), _TRUNCATE);

	ImGui::InputTextWithHint(label, "Click to set keyboard shortcut", buf, sizeof(buf),
		ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_NoHorizontalScroll);

	if (ImGui::IsItemActive())
	{
		const uint32_t last_key_pressed = reshade_last_key_pressed(runtime);
		if (last_key_pressed != 0)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
			{
				*keys = 0;
			}
			else if ((last_key_pressed & GP_FLAG) != 0)
			{
				*keys = last_key_pressed;
			}
			else if (last_key_pressed < 0x10 || last_key_pressed > 0x12)
			{
				*keys = last_key_pressed;
				*keys |= static_cast<uint32_t>(runtime->is_key_down(0x11)) << 8;
				*keys |= static_cast<uint32_t>(runtime->is_key_down(0x10)) << 16;
				*keys |= static_cast<uint32_t>(runtime->is_key_down(0x12)) << 24;
			}
			return true;
		}
	}
	else if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
	}

	return false;
}
