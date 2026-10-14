#pragma once

#include <optional>
#include <string>
#include <Windows.h>

namespace QCAutomation::ToneSource {
	void Initialize();
    bool GetActiveIdentity(std::string& song, std::string& arrangement, const std::string& expectedRuntimeId);
	void HandleInSongState();
	void HandleOutOfSongState();
	void NotifyManualToneSlotHotkey(WPARAM keyPressed);
	bool TryGetCurrentToneSlot(int& outToneSlot);
	std::optional<std::string> GetCurrentAuthoredToneName(float playbackTimeSec);
	bool TryGetAutomationToneName(std::string& outToneName);
	bool TryGetCurrentAuthoredToneName(std::string& outToneName);
}
