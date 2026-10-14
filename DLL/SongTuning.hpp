#pragma once

#include <array>

#include "Tuning.h"

namespace SongTuning {
	bool TryGetCurrentTuning(std::array<byte, 6>& outTuning);
	std::array<byte, 6> GetCurrentTuning(bool verbose = false);
	bool IsExtendedRangeSong();
	std::array<int, 2> GetHighestLowestString(bool bass = false);
	std::array<int, 2> GetHighestLowestString(Tuning tuningOverride, bool bass = false);
	bool IsSongInDrop(Tuning tuning);
	bool IsSongInStandard(Tuning tuning);
	int GetTrueTuning();
	void InstallTunerHook();
	Tuning GetTuningAtTuner(bool logFailures = true);
	bool IsPlayerOnBass();
	bool IsExtendedRangeTuner();
};
