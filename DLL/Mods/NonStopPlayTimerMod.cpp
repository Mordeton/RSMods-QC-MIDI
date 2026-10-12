#include "../stdafx.h"
#include "NonStopPlayTimerMod.hpp"

using Framework::ModContext;
namespace Setting = Settings::Setting;

namespace {
	constexpr double DefaultTimeLimit = 10.9899997711182; // The default pre-song timer for Non-Stop Play.
}

void NonStopPlayTimerMod::OnEnabled(ModContext& c) {
	ApplyTimer(c);
	active = true;
}

void NonStopPlayTimerMod::OnSettingsChanged(ModContext& c) {
	if (active)
		ApplyTimer(c);
}

void NonStopPlayTimerMod::OnDisabled(ModContext&) {
	active = false;
}

void NonStopPlayTimerMod::ApplyTimer(ModContext& c) {
	const double desired = c.IsOn(Setting::UseCustomNSPTimer)
		? c.Int(Setting::CustomNSPTimeLimit) / 1000.0
		: DefaultTimeLimit;

	const double current = SongTimer::GetNonStopPlayTimer();

	const double eps = std::numeric_limits<double>::epsilon() * std::max(1.0, std::max(std::abs(desired), std::abs(current))) * 4;
	if (std::abs(current - desired) > eps) {
		LOG_INFO("Updating NSP timer..." << std::endl);
		SongTimer::SetNonStopPlayTimer(desired);
	}
}

static Framework::ModRegistrar<NonStopPlayTimerMod> _nonStopPlayTimerReg;
