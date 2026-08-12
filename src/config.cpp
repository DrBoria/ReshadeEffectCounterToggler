#include <charconv>
#include <string>

#include <reshade.hpp>

#include "config.h"
#include "constants.h"
#include "state.h"

using namespace reshade::api;

void save_groups(effect_runtime *runtime)
{
	if (runtime == nullptr)
		return;
	reshade::set_config_value(runtime, kConfigSection, "GlobalFallbackKey", g_global_fallback_key);
	reshade::set_config_value(runtime, kConfigSection, "GroupCount", static_cast<int>(g_groups.size()));
	for (size_t i = 0; i < g_groups.size(); ++i)
	{
		const ToggleGroup &group = g_groups[i];
		std::string name_key = "Group" + std::to_string(i) + "Name";
		std::string fallback_key = "Group" + std::to_string(i) + "Fallback";
		std::string keys_key = "Group" + std::to_string(i) + "Keys";
		reshade::set_config_value(runtime, kConfigSection, name_key.c_str(), group.name.c_str());
		reshade::set_config_value(runtime, kConfigSection, fallback_key.c_str(), group.fallback_key);
		std::string keys_str;
		for (size_t k = 0; k < group.keys.size(); ++k)
		{
			if (k > 0)
				keys_str += ',';
			keys_str += std::to_string(group.keys[k]);
		}
		reshade::set_config_value(runtime, kConfigSection, keys_key.c_str(), keys_str.c_str());
	}
}

void load_groups(effect_runtime *runtime)
{
	if (runtime == nullptr)
		return;
	g_groups.clear();
	reshade::get_config_value(runtime, kConfigSection, "GlobalFallbackKey", g_global_fallback_key);
	int group_count = 0;
	reshade::get_config_value(runtime, kConfigSection, "GroupCount", group_count);
	for (int i = 0; i < group_count; ++i)
	{
		ToggleGroup group;
		std::string name_key = "Group" + std::to_string(i) + "Name";
		std::string fallback_key = "Group" + std::to_string(i) + "Fallback";
		std::string keys_key = "Group" + std::to_string(i) + "Keys";
		char name_buf[256] = "";
		size_t name_len = sizeof(name_buf) - 1;
		if (reshade::get_config_value(runtime, kConfigSection, name_key.c_str(), name_buf, &name_len))
			group.name = name_buf;
		reshade::get_config_value(runtime, kConfigSection, fallback_key.c_str(), group.fallback_key);
		char keys_buf[1024] = "";
		size_t keys_len = sizeof(keys_buf) - 1;
		if (reshade::get_config_value(runtime, kConfigSection, keys_key.c_str(), keys_buf, &keys_len))
		{
			std::string keys_str(keys_buf, keys_len);
			size_t pos = 0;
			while (pos < keys_str.size())
			{
				size_t comma = keys_str.find(',', pos);
				std::string token = keys_str.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
				if (!token.empty())
				{
					uint32_t key = 0;
					auto res = std::from_chars(token.data(), token.data() + token.size(), key);
					if (res.ec == std::errc{})
						group.keys.push_back(key);
				}
				if (comma == std::string::npos)
					break;
				pos = comma + 1;
			}
		}
		g_groups.push_back(group);
	}
}
