#pragma once

namespace reshade::api
{
	struct effect_runtime;
}

// Persist groups and fallback keys to the ReShade INI config.
void save_groups(reshade::api::effect_runtime *runtime);
void load_groups(reshade::api::effect_runtime *runtime);
