#pragma once
// Offline boundary substitutes. The regression executable includes the real QC runtime.
#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
using byte = unsigned char;
#define LOG_INFO(message) do {} while (false)
#define LOG_WARNING(message) do {} while (false)
#define LOG_ERROR(message) do {} while (false)
struct Tuning {
    std::array<byte, 6> strings;
    Tuning(byte a, byte b, byte c, byte d, byte e, byte f) : strings{a,b,c,d,e,f} {}
};
namespace Settings {
    inline std::map<std::string, std::string> values;
    inline std::string ReturnSettingValue(const std::string& key) { return values[key]; }
    inline int GetModSetting(const std::string& key) {
        const auto value = ReturnSettingValue(key);
        return value.empty() ? 0 : std::stoi(value);
    }
}
namespace GameState {
    inline std::string song = "test-song", arrangement = "lead";
    inline bool arrangementAvailable = true;
    inline bool TryGetCurrentArrangementID(std::string& out) { out = arrangementAvailable ? arrangement : ""; return !out.empty(); }
    inline std::string GetSongKey() { return song; }
    inline std::string GetArrangementID() { return arrangement; }
}
namespace SongTimer {
    inline float time = 10;
    inline float SongTimer() { return time; }
}
namespace SongTuning {
    inline bool available = true;
    inline std::array<byte, 6> tuning{};
    inline bool TryGetCurrentTuning(std::array<byte, 6>& out) { out = tuning; return available; }
    inline std::array<int, 2> GetHighestLowestString(Tuning input) {
        int high = -128, low = 127;
        for (byte raw : input.strings) {
            const int value = raw > 127 ? int(raw) - 256 : int(raw);
            high = std::max(high, value); low = std::min(low, value);
        }
        return {high, low};
    }
    inline bool IsSongInDrop(Tuning input) {
        for (size_t i = 2; i < 6; ++i) if (input.strings[i] != input.strings[1]) return false;
        return byte(input.strings[0] + 2) == input.strings[1];
    }
}
namespace Midi {
    struct Message { std::string device; int channel, controller, value; };
    inline std::vector<Message> messages;
    inline std::string selected;
    inline bool available = true, failScene = false, failPreset = false;
    inline bool RefreshMidiOutDevice(const std::string& device) { selected = device; return available; }
    inline bool SendRawControlChange(char cc, char value, char channel) {
        if (!available || (cc == Settings::GetModSetting("QCAutomationSceneCC") && failScene)) return false;
        messages.push_back({selected, channel, cc, value}); return true;
    }
    inline bool SendRawProgramChange(char value, char channel) {
        if (!available || failPreset) return false;
        messages.push_back({selected, channel, -1, value}); return true;
    }
}
namespace QCAutomation::ToneSource {
    inline bool metadataAvailable = false;
    inline std::string metadataSong, metadataArrangement;
    inline std::array<int, 6> metadataTuning{};
    inline bool TryGetArrangementTuning(const std::string& song, const std::string& arrangement, std::array<int, 6>& tuning) {
        if (!metadataAvailable || song.empty() || arrangement.empty() || song != metadataSong || arrangement != metadataArrangement) return false;
        tuning = metadataTuning; return true;
    }
    inline int manualSlot = 0;
    inline std::string tone = "clean";
    inline void Initialize() {}
    inline void HandleInSongState() {}
    inline void HandleOutOfSongState() { manualSlot = 0; }
    inline bool TryGetCurrentToneSlot(int& slot) { slot = manualSlot; return slot >= 2 && slot <= 4; }
    inline bool TryGetAutomationToneName(std::string& name) { name = tone; return !name.empty(); }
    inline bool TryGetCurrentAuthoredToneName(std::string& name) { name = tone; return !name.empty(); }
}
