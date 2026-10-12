#include "../stdafx.h"
#include "SongPreviewsMod.hpp"

using Framework::ModContext;
namespace Setting = Settings::Setting;

bool SongPreviewsMod::IsEnabled(const ModContext& c) const {
    return c.IsOn(Setting::SongPreviews);
}

void SongPreviewsMod::OnEnabled(ModContext&) {
    VolumeControl::DisableSongPreviewAudio();
}

void SongPreviewsMod::OnDisabled(ModContext&) {
    VolumeControl::EnableSongPreviewAudio();
}

static Framework::ModRegistrar<SongPreviewsMod> _songPreviewsReg;
