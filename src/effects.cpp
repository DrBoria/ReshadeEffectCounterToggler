#include <utility>
#include <vector>

#include <reshade.hpp>

#include "effects.h"
#include "state.h"

using namespace reshade::api;

static std::vector<std::pair<effect_technique, bool>> g_snapshot;

void snapshot_effects()
{
	g_snapshot.clear();
	if (g_runtime == nullptr)
		return;
	g_runtime->enumerate_techniques(nullptr, [](effect_runtime *runtime, effect_technique technique, void *)
	{
		g_snapshot.emplace_back(technique, runtime->get_technique_state(technique));
	}, nullptr);
}

void apply_effects(bool enabled)
{
	if (g_runtime == nullptr)
		return;
	for (const auto &entry : g_snapshot)
		g_runtime->set_technique_state(entry.first, enabled);
}

void hide_effects()
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

void show_effects()
{
	if (!g_effects_hidden)
		return;
	if (g_runtime == nullptr)
		return;
	// Restore each technique to its saved snapshot state (only the ones that were active).
	for (const auto &entry : g_snapshot)
		g_runtime->set_technique_state(entry.first, entry.second);
	if (g_effects_state_saved)
	{
		g_runtime->set_effects_state(g_effects_state);
		g_effects_state_saved = false;
	}
	g_effects_hidden = false;
}

void update_effects_hidden()
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
