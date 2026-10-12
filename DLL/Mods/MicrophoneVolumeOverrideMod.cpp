#include "../stdafx.h"
#include "MicrophoneVolumeOverrideMod.hpp"
#include "AudioDevices.hpp"

using Framework::ModContext;
namespace Setting = Settings::Setting;

bool MicrophoneVolumeOverrideMod::IsEnabled(const ModContext& c) const {
	return c.IsOn(Setting::OverrideInputVolumeEnabled);
}

void MicrophoneVolumeOverrideMod::OnMenuTick(ModContext& c) {
	SyncVolume(c);
}

void MicrophoneVolumeOverrideMod::OnSongTick(ModContext& c) {
	SyncVolume(c);
}

// Forces the configured input device to the desired volume. Idempotent: only writes when the
// device's current level differs, so it can run every tick without fighting the user's own changes.
void MicrophoneVolumeOverrideMod::SyncVolume(ModContext& c) {
	const std::string device = c.Value(Setting::OverrideInputVolumeDevice);
	if (device.empty())
		return;

	const int desired = c.Int(Setting::OverrideInputVolume);
	if (AudioDevices::GetMicrophoneVolume(device) != desired) {
		AudioDevices::SetMicrophoneVolume(device, desired);
	}
}

static Framework::ModRegistrar<MicrophoneVolumeOverrideMod> _micVolumeOverrideReg;
