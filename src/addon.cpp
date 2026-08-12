#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <imgui.h>
#include <reshade.hpp>

using namespace reshade::api;

#define GP_FLAG 0x80000000u

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

static bool gamepad_button_pressed(uint8_t index)
{
	if (index >= 14)
		return false;
	uint16_t buttons = gamepad_buttons();
	return (buttons & g_gamepad_button_flags[index]) != 0;
}

static uint32_t gamepad_last_button_pressed()
{
	uint16_t buttons = gamepad_buttons();
	for (uint8_t i = 0; i < 14; i++)
		if ((buttons & g_gamepad_button_flags[i]) != 0)
			return GP_FLAG | i;
	return 0;
}

struct KeyBinding
{
	uint32_t key = 0;
	bool down = false;
};

enum class GroupMode
{
	Switch,
	Screens
};

struct ToggleGroup
{
	std::string name;
	GroupMode mode = GroupMode::Screens;
	std::vector<KeyBinding> keys;
	uint32_t fallback_key = 0;
	bool fallback_down = false;
	bool open = false;
};

static effect_runtime *g_runtime = nullptr;
static std::vector<ToggleGroup> g_groups;
static uint32_t g_global_fallback_key = 0;
static bool g_global_fallback_down = false;
static bool g_effects_hidden = false;
static bool g_effects_state_saved = false;
static bool g_effects_state = true;
static bool g_manual_hidden = false;
static bool g_manual_toggle = false;
static bool g_need_snapshot = false;
static bool g_need_restore = false;
static bool g_need_reload_snapshot = false;

static std::vector<std::pair<effect_technique, bool>> g_snapshot;

static std::string key_name(uint32_t key);

static bool are_keys_pressed(uint32_t keys, effect_runtime *runtime)
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

static uint32_t reshade_last_key_pressed(effect_runtime *runtime)
{
	uint32_t gp = gamepad_last_button_pressed();
	if (gp != 0)
		return gp;
	for (uint32_t i = 0x06; i < 256; i++)
		if (runtime->is_key_pressed(static_cast<uint8_t>(i)))
			return static_cast<uint8_t>(i);
	return 0;
}

static void snapshot_effects()
{
	g_snapshot.clear();
	if (g_runtime == nullptr)
		return;
	g_runtime->enumerate_techniques(nullptr, [](effect_runtime *runtime, effect_technique technique, void *)
	{
		g_snapshot.emplace_back(technique, runtime->get_technique_state(technique));
	}, nullptr);
}

static void apply_effects(bool enabled)
{
	if (g_runtime == nullptr)
		return;
	for (const auto &entry : g_snapshot)
		g_runtime->set_technique_state(entry.first, enabled);
}

static void hide_effects()
{
	if (g_effects_hidden)
		return;
	if (g_runtime == nullptr)
		return;
	g_effects_state_saved = true;
	g_effects_state = g_runtime->get_effects_state();
	snapshot_effects();
	apply_effects(false);
	g_effects_hidden = true;
}

static void show_effects()
{
	if (!g_effects_hidden)
		return;
	if (g_runtime == nullptr)
		return;
	apply_effects(true);
	if (g_effects_state_saved)
	{
		g_runtime->set_effects_state(g_effects_state);
		g_effects_state_saved = false;
	}
	g_effects_hidden = false;
}

static void update_effects_hidden()
{
	bool any_open = false;
	for (const auto &group : g_groups)
	{
		if (group.open)
		{
			any_open = true;
			break;
		}
	}
	bool should_hide = any_open || g_manual_hidden;
	if (should_hide && !g_effects_hidden)
		hide_effects();
	else if (!should_hide && g_effects_hidden)
		show_effects();
}

static void on_reshade_present(effect_runtime *runtime)
{
	g_runtime = runtime;

	if (g_manual_toggle)
	{
		g_manual_toggle = false;
		g_manual_hidden = !g_manual_hidden;
		update_effects_hidden();
	}

	if (g_need_snapshot)
	{
		g_need_snapshot = false;
		snapshot_effects();
	}

	if (g_need_restore)
	{
		g_need_restore = false;
		show_effects();
	}

	if (g_need_reload_snapshot)
	{
		g_need_reload_snapshot = false;
		if (g_effects_hidden)
			snapshot_effects();
	}

	if (g_global_fallback_key != 0 && are_keys_pressed(g_global_fallback_key, runtime))
	{
		g_global_fallback_down = !g_global_fallback_down;
		for (auto &group : g_groups)
			group.open = g_global_fallback_down;
		update_effects_hidden();
	}

	for (auto &group : g_groups)
	{
		if (group.fallback_key != 0 && are_keys_pressed(group.fallback_key, runtime))
		{
			group.fallback_down = !group.fallback_down;
			group.open = group.fallback_down;
			update_effects_hidden();
			continue;
		}

		for (auto &binding : group.keys)
		{
			if (binding.key == 0)
				continue;
			if (are_keys_pressed(binding.key, runtime))
			{
				binding.down = !binding.down;
				if (group.mode == GroupMode::Switch)
				{
					group.open = binding.down;
				}
				else
				{
					bool any = false;
					for (const auto &b : group.keys)
					{
						if (b.down)
						{
							any = true;
							break;
						}
					}
					group.open = any;
				}
				update_effects_hidden();
			}
		}
	}
}

static void on_reshade_reloaded_effects(effect_runtime *runtime)
{
	g_runtime = runtime;
	if (g_effects_hidden)
		g_need_reload_snapshot = true;
}

static bool key_capture(const char *label, uint32_t *keys, effect_runtime *runtime)
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

static void on_reshade_overlay(effect_runtime *runtime)
{
	g_runtime = runtime;

	if (ImGui::Button(g_manual_hidden ? "Show effects (manual)" : "Hide effects (manual)"))
	{
		g_manual_toggle = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Snapshot"))
	{
		g_need_snapshot = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Restore"))
	{
		g_need_restore = true;
	}

	ImGui::Separator();

	if (ImGui::Button("Add group"))
	{
		ToggleGroup group;
		group.name = "Group " + std::to_string(g_groups.size() + 1);
		g_groups.push_back(group);
	}

	ImGui::Text("Global fallback key:");
	key_capture("##global_fallback", &g_global_fallback_key, runtime);

	for (size_t i = 0; i < g_groups.size(); ++i)
	{
		ToggleGroup &group = g_groups[i];
		std::string label = group.name + "##" + std::to_string(i);
		if (ImGui::CollapsingHeader(label.c_str()))
		{
			ImGui::Text("Mode:");
			ImGui::SameLine();
			const char *modes[] = { "Switch", "Screens" };
			int mode = static_cast<int>(group.mode);
			if (ImGui::Combo("##mode", &mode, modes, 2))
				group.mode = static_cast<GroupMode>(mode);

			ImGui::Text("Fallback key:");
			key_capture("##fallback", &group.fallback_key, runtime);

			ImGui::Text("Keys:");
			for (size_t k = 0; k < group.keys.size(); ++k)
			{
				ImGui::PushID(static_cast<int>(i * 100 + k));
				key_capture("##key", &group.keys[k].key, runtime);
				ImGui::SameLine();
				if (ImGui::Button("X"))
				{
					group.keys.erase(group.keys.begin() + k);
					--k;
				}
				ImGui::PopID();
			}
			if (ImGui::Button("Add key"))
			{
				group.keys.push_back(KeyBinding());
			}
			ImGui::SameLine();
			if (ImGui::Button("Delete group"))
			{
				g_groups.erase(g_groups.begin() + i);
				--i;
			}
		}
	}

	ImGui::Separator();
	ImGui::Text("Effects hidden: %s", g_effects_hidden ? "yes" : "no");
	ImGui::Text("Groups open: %zu", std::count_if(g_groups.begin(), g_groups.end(), [](const ToggleGroup &g) { return g.open; }));
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

static std::string key_name(uint32_t keys)
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

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		reshade::register_addon(hinstDLL);
		reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);
		reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reshade_reloaded_effects);
		reshade::register_overlay(nullptr, on_reshade_overlay);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_overlay(nullptr, on_reshade_overlay);
		reshade::unregister_event<reshade::addon_event::reshade_reloaded_effects>(on_reshade_reloaded_effects);
		reshade::unregister_event<reshade::addon_event::reshade_present>(on_reshade_present);
		reshade::unregister_addon(hinstDLL);
		break;
	}
	return TRUE;
}
