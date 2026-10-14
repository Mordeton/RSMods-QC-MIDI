#include "../stdafx.h"
#include "QCAutomationMod.hpp"
#include "QCAutomation.hpp"
#include "ToneSource.hpp"

void QCAutomationMod::OnInitialize(Framework::ModContext& c) {
	QCAutomation::Initialize();
	for (unsigned int key : std::array<unsigned int, 8>{'1', '2', '3', '4', VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD4}) {
		c.Commands().BindKey(
			"QCToneSlot" + std::to_string(key), key,
			Framework::KeyEdge::Down, Framework::Availability::Active,
			[](Framework::ModContext&, const Framework::KeyEvent& event) {
				QCAutomation::ToneSource::NotifyManualToneSlotHotkey(event.virtualKey);
			});
	}
}

void QCAutomationMod::OnTick(Framework::ModContext& c) {
	if (c.phase == Framework::GamePhase::Loading) return;
	// Keep the out-of-song reset running even when QC is disabled, as the old host did.
	if (c.phase == Framework::GamePhase::Song)
		QCAutomation::HandleInSongState();
	else
		QCAutomation::HandleOutOfSongState();
}

static Framework::ModRegistrar<QCAutomationMod> _qcAutomationReg;
