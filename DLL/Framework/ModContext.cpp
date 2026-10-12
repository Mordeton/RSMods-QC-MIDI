#include "ModContext.hpp"

// Keeping Settings.hpp confined here is the whole point of the
// out-of-line accessors: the rest of the framework never sees it.
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "../RSColor.h"
#include "../Settings.hpp"

namespace Framework {
	bool ModContext::IsOn(std::string_view key) const { return Settings::IsOn(std::string(key)); }
	bool ModContext::IsOff(std::string_view key) const { return Settings::IsOff(std::string(key)); }
	std::string ModContext::Value(std::string_view key) const { return Settings::ReturnSettingValue(std::string(key)); }
	int ModContext::Int(std::string_view key) const { return Settings::GetModSetting(std::string(key)); }

	Settings::When ModContext::When(std::string_view key) const { return Settings::GetWhen(std::string(key)); }
	Settings::StringColorMode ModContext::ColorMode() const { return Settings::GetStringColorMode(); }
	Settings::NoteColorMode ModContext::NoteColorMode() const { return Settings::GetNoteColorMode(); }
}
