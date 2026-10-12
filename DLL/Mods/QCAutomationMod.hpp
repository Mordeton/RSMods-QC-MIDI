#pragma once

#include "../Framework/Framework.hpp"

// Runs QC automation on the host main thread alongside the current upstream mods.
class QCAutomationMod : public Framework::IMod {
public:
	MOD_ID(QCAutomationMod)
	void OnInitialize(Framework::ModContext& c) override;
	void OnTick(Framework::ModContext& c) override;
};
