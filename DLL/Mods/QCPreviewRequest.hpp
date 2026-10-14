#pragma once
#include <string>
#include <cstdint>
namespace QCAutomation::Panel {
struct PreviewRequest {
    std::string requested, loaded;
    uint64_t generation = 0;
    void Select(const std::string& song) {
        if (song != requested) { requested = song; loaded.clear(); ++generation; }
    }
    bool NeedsLoad() const { return !requested.empty() && loaded != requested; }
    bool Complete(uint64_t token) {
        if (token != generation) return false;
        loaded = requested;
        return true;
    }
};
}
