#include "../stdafx.h"
#include "RemoveHeadstockMod.hpp"

using Framework::ModContext;
using Settings::When;
using Framework::GamePhase;
namespace Setting = Settings::Setting;

bool RemoveHeadstockMod::IsEnabled(const ModContext& c) const {
	return c.IsOn(Setting::RemoveHeadstockEnabled);
}

void RemoveHeadstockMod::SyncState(ModContext& c) {
	const When when = c.When(Setting::RemoveHeadstockWhen);

	D3DHooks::RemoveHeadstockInThisMenu = when == When::Startup || (when == When::Song && c.phase == GamePhase::Song);
}

void RemoveHeadstockMod::OnEnabled(ModContext& c) {
	SyncState(c);
	active = true;
}

void RemoveHeadstockMod::OnSettingsChanged(ModContext& c) {
	if (active)
		SyncState(c);
}

void RemoveHeadstockMod::OnSongEnter(ModContext& c) {
	SyncState(c);
}

void RemoveHeadstockMod::OnDisabled(ModContext&) {
	active = false;
	D3DHooks::RemoveHeadstockInThisMenu = false;
}

void RemoveHeadstockMod::OnSongExit(ModContext& c) {
	SyncState(c);
}

static Framework::ModRegistrar<RemoveHeadstockMod> _removeHeadstockReg;
