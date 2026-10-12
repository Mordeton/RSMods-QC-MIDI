#pragma once

#include "../Framework/Framework.hpp"

class RemoveLyricsMod : public Framework::IMod {
public:
	MOD_ID(RemoveLyricsMod)
	bool IsEnabled(const Framework::ModContext& c) const override;

	void OnInitialize(Framework::ModContext& c) override;
	void OnMenuTick(Framework::ModContext& c) override;
	void OnSongTick(Framework::ModContext& c) override;

private:
	void ApplyStartup(Framework::ModContext& c);
};
