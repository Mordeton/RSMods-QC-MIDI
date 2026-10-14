#pragma once
#include <string>
#include <vector>
#include <array>
#include <Windows.h>

namespace QCAutomation {
struct ToneDescription { std::string automaticRole, effectiveRole, target, warning; };
ToneDescription DescribeTone(const std::string& tone, const std::string& overrideRole);
std::string DescribeTuning(const std::array<int, 6>& tuning, bool available);
namespace Panel {
void Tick(bool inSong);
void PublishStatus(const std::string& tone, const std::string& automationTone, const std::string& role, const std::string& target,
    const std::string& mode, const std::string& scene, const std::string& send);
std::string CurrentOverride(const std::string& tone);
void Render();
bool HandleInput(HWND window, UINT message, WPARAM key, LPARAM parameter);
}
}
