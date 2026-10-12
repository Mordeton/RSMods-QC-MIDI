#pragma once

#include "../Framework/Framework.hpp"

class VolumeDisplayMod : public Framework::IMod {
public:
	MOD_ID(VolumeDisplayMod)

	void OnInitialize(Framework::ModContext& c) override;
	void OnMenuTick(Framework::ModContext& c) override;
	void OnSongTick(Framework::ModContext& c) override;

private:
	static void ToggleMute(bool player2);
	static void ChangeVolume(const Framework::ModContext& c, const Framework::KeyEvent& event,
		std::string_view channel, int overlayIndex);
	void SyncDisplay(Framework::ModContext& c);
	bool MoreThanThreeSecondsPassed() const;

	struct VolumeBinding {
		const char* key;
		const char* channel;
		int overlayIndex;
	};

	static constexpr std::array<VolumeBinding, 7> volumeBindings = { {
		{ Settings::Setting::Key::MasterVolume,     Settings::Setting::Channel::Master,    0 },
		{ Settings::Setting::Key::SongVolume,       Settings::Setting::Channel::Music,     1 },
		{ Settings::Setting::Key::Player1Volume,    Settings::Setting::Channel::Player1,   2 },
		{ Settings::Setting::Key::Player2Volume,    Settings::Setting::Channel::Player2,   3 },
		{ Settings::Setting::Key::MicrophoneVolume, Settings::Setting::Channel::Mic,       4 },
		{ Settings::Setting::Key::VoiceOverVolume,  Settings::Setting::Channel::VoiceOver, 5 },
		{ Settings::Setting::Key::SFXVolume,        Settings::Setting::Channel::SFX,       6 },
	} };
};
