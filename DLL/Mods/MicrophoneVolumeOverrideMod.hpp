#pragma once

#include "../Framework/Framework.hpp"

class MicrophoneVolumeOverrideMod : public Framework::IMod {
public:
	MOD_ID(MicrophoneVolumeOverrideMod)
	bool IsEnabled(const Framework::ModContext& c) const override;

	void OnMenuTick(Framework::ModContext& c) override;
	void OnSongTick(Framework::ModContext& c) override;

private:
	void SyncVolume(Framework::ModContext& c);
};
