#pragma once

#include <cstdint>
#include <string>
#include <vector>

// A named group of equivalent keys. Pressing any key in the group toggles the
// group's open state. Effects are hidden while any group is open.
struct ToggleGroup
{
	std::string name;
	std::vector<uint32_t> keys;   // all keys in the group are equivalent
	uint32_t fallback_key = 0;    // per-group fallback (optional): closes this group only if open
	bool open = false;
};
