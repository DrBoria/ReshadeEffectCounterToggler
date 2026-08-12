#include <algorithm>
#include <string>

#include <windows.h>
#include <imgui.h>
#include <reshade.hpp>

#include "config.h"
#include "constants.h"
#include "effects.h"
#include "input.h"
#include "state.h"

using namespace reshade::api;

// Shared state (declared extern in state.h).
effect_runtime *g_runtime = nullptr;
std::vector<ToggleGroup> g_groups;
uint32_t g_global_fallback_key = 0;

bool g_effects_hidden = false;
bool g_effects_state_saved = false;
bool g_effects_state = true;
bool g_manual_hidden = false;
bool g_manual_toggle = false;
bool g_need_snapshot = false;
bool g_need_restore = false;
bool g_need_reload_snapshot = false;
bool g_config_loaded = false;

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

	// Per-group keys: pressing any key in a group toggles that group's open state.
	for (auto &group : g_groups)
	{
		// Per-group fallback: closes this group, but only if it is currently open.
		if (group.fallback_key != 0 && are_keys_pressed(group.fallback_key, runtime))
		{
			if (group.open)
			{
				group.open = false;
				update_effects_hidden();
			}
			continue;
		}

		bool pressed = false;
		for (uint32_t key : group.keys)
		{
			if (key != 0 && are_keys_pressed(key, runtime))
			{
				pressed = true;
				break;
			}
		}
		if (pressed)
		{
			group.open = !group.open;
			update_effects_hidden();
		}
	}

	// Global fallback: closes all open groups, but only if at least one group is open.
	if (g_global_fallback_key != 0 && are_keys_pressed(g_global_fallback_key, runtime))
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
		if (any_open)
		{
			for (auto &group : g_groups)
				group.open = false;
			update_effects_hidden();
		}
	}
}

static void on_reshade_reloaded_effects(effect_runtime *runtime)
{
	g_runtime = runtime;
	if (g_effects_hidden)
		g_need_reload_snapshot = true;
}

static void on_reshade_overlay(effect_runtime *runtime)
{
	g_runtime = runtime;

	if (!g_config_loaded)
	{
		g_config_loaded = true;
		load_groups(runtime);
	}

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
	ImGui::SameLine();
	if (ImGui::Button("Save all groups"))
	{
		save_groups(runtime);
	}

	ImGui::Text("Global fallback key:");
	key_capture("##global_fallback", &g_global_fallback_key, runtime);

	for (size_t i = 0; i < g_groups.size(); ++i)
	{
		ToggleGroup &group = g_groups[i];
		std::string label = group.name + "##" + std::to_string(i);
		if (ImGui::CollapsingHeader(label.c_str()))
		{
			ImGui::Text("Fallback key:");
			key_capture("##fallback", &group.fallback_key, runtime);

			ImGui::Text("Keys:");
			for (size_t k = 0; k < group.keys.size(); ++k)
			{
				ImGui::PushID(static_cast<int>(i * 100 + k));
				key_capture("##key", &group.keys[k], runtime);
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
				group.keys.push_back(0);
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
