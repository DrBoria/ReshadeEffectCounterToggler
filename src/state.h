#pragma once

#include <cstdint>
#include <vector>

#include "types.h"

namespace reshade::api
{
	struct effect_runtime;
}

// Shared addon state. Definitions live in addon.cpp.
extern reshade::api::effect_runtime *g_runtime;
extern std::vector<ToggleGroup> g_groups;
extern uint32_t g_global_fallback_key;

extern bool g_effects_hidden;
extern bool g_effects_state_saved;
extern bool g_effects_state;
extern bool g_manual_hidden;
extern bool g_manual_toggle;
extern bool g_need_snapshot;
extern bool g_need_restore;
extern bool g_need_reload_snapshot;
extern bool g_config_loaded;
