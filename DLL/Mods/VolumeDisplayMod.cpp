#include "../stdafx.h"
#include "VolumeDisplayMod.hpp"
#include "VolumeControl.hpp"
#include "../D3DOverlay.hpp"

using Framework::ModContext;
using Framework::KeyEdge;
using Framework::Availability;
using Framework::KeyEvent;
namespace Setting = Settings::Setting;

void VolumeDisplayMod::OnInitialize(ModContext& c) {
	auto commands = c.Commands();
	const auto volumeEnabled = [](const ModContext& context, const KeyEvent&) { return context.IsOn(Setting::VolumeControlEnabled); };

	commands.BindSetting(Setting::Key::MutePlayer1, KeyEdge::Up,
		Availability::Active,
		[](ModContext&, const KeyEvent&) { ToggleMute(false); }, {}, "Mute Player 1");

	commands.BindSetting(Setting::Key::MutePlayer2, KeyEdge::Up,
		Availability::Active,
		[](ModContext&, const KeyEvent&) { ToggleMute(true); }, {}, "Mute Player 2");

	commands.BindSetting(Setting::Key::DisplayMixer, KeyEdge::Up,
		Availability::Active,
		[](ModContext&, const KeyEvent&) { GameOverlay::displayMixer = false; });

	commands.BindSetting(Setting::Key::ChangedSelectedVolume, KeyEdge::Up,
		Availability::Active,
		[](ModContext&, const KeyEvent&) {
			++GameOverlay::currentVolumeIndex;
			if (GameOverlay::currentVolumeIndex >= GameOverlay::mixerChannels.size()) {
				GameOverlay::currentVolumeIndex = 0;
			}
		}, volumeEnabled);

	commands.BindSetting(Setting::Key::DisplayMixer, KeyEdge::Down,
		Availability::Active,
		[](ModContext&, const KeyEvent&) { GameOverlay::displayMixer = true; },
		volumeEnabled, "Display Mixer");

	for (const auto& binding : volumeBindings) {
		commands.BindSetting(binding.key, KeyEdge::Down,
			Availability::Active,
			[channel = std::string(binding.channel), overlayIndex = binding.overlayIndex]
			(ModContext& context, const KeyEvent& event) {
				ChangeVolume(context, event, channel, overlayIndex);
			}, volumeEnabled);
	}
}

void VolumeDisplayMod::ToggleMute(bool player2) {
	bool& muted = player2 ? VolumeControl::player2Muted : VolumeControl::player1Muted;

	if (muted) {
		VolumeControl::UnmutePlayer(player2);
	}
	else {
		VolumeControl::MutePlayer(player2);
	}

	GameOverlay::displayCurrentVolume = true;
	GameOverlay::displayVolumeStartTime = std::chrono::steady_clock::now();
	GameOverlay::currentVolumeIndex = player2 ? 3 : 2;
}

void VolumeDisplayMod::ChangeVolume(const ModContext& c, const KeyEvent& event, std::string_view channel, int overlayIndex) {
	const int interval = c.Int(Setting::VolumeControlInterval);

	if (event.control) {
		VolumeControl::DecreaseVolume(interval, std::string(channel));
	}
	else {
		VolumeControl::IncreaseVolume(interval, std::string(channel));
	}

	GameOverlay::displayCurrentVolume = true;
	GameOverlay::displayVolumeStartTime = std::chrono::steady_clock::now();
	GameOverlay::currentVolumeIndex = overlayIndex;
}

void VolumeDisplayMod::OnMenuTick(ModContext& c) {
	SyncDisplay(c);
}

void VolumeDisplayMod::OnSongTick(ModContext& c) {
	SyncDisplay(c);
}

// The volume overlay is raised by the volume keybindings; hide it again once it has been up for 3s.
void VolumeDisplayMod::SyncDisplay(ModContext& c) {
	if (c.IsOn(Setting::VolumeControlEnabled) && MoreThanThreeSecondsPassed()) {
		GameOverlay::displayCurrentVolume = false;
	}
}

bool VolumeDisplayMod::MoreThanThreeSecondsPassed() const {
	const auto currentTime = std::chrono::steady_clock::now();

	return currentTime - GameOverlay::displayVolumeStartTime > std::chrono::seconds(3);
}

static Framework::ModRegistrar<VolumeDisplayMod> _volumeDisplayReg;
