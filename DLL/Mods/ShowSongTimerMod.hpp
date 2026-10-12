#pragma once

#include "../Framework/Framework.hpp"

class ShowSongTimerMod : public Framework::IMod {
public:
	MOD_ID(ShowSongTimerMod)
	bool IsEnabled(const Framework::ModContext& c) const override;

	void OnInitialize(Framework::ModContext& c) override;
	void OnSongEnter(Framework::ModContext& c) override;
	void OnSongExit(Framework::ModContext& c) override;
};
