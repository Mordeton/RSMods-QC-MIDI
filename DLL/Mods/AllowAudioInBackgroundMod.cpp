#include "../stdafx.h"
#include "AllowAudioInBackgroundMod.hpp"

using Framework::ModContext;
namespace Setting = Settings::Setting;

bool AllowAudioInBackgroundMod::IsEnabled(const ModContext& c) const {
    return c.IsOn(Setting::AllowAudioInBackground);
}

void AllowAudioInBackgroundMod::OnEnabled(ModContext&) {
    VolumeControl::AllowAltTabbingWithAudio();
}

void AllowAudioInBackgroundMod::OnDisabled(ModContext&) {
    VolumeControl::DisableAltTabbingWithAudio();
}

static Framework::ModRegistrar<AllowAudioInBackgroundMod> _allowAudioInBackgroundReg;
