#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace SongTimer::Detail {
// The game can release any link while leaving the tuner. Every dereference,
// including the final float, must use the guarded reader (MemUtil::TryRead).
template <typename Reader>
std::optional<float> ReadTimer(uintptr_t address,
    std::span<const unsigned int> offsets, Reader& read) {
    for (const auto offset : offsets) {
        uintptr_t next = 0;
        if (!address || !read(address, next) || !next ||
            next > (std::numeric_limits<uintptr_t>::max)() - offset)
            return std::nullopt;
        address = next + offset;
    }
    float value = 0.f;
    if (!address || !read(address, value) || !std::isfinite(value))
        return std::nullopt;
    return value;
}

template <typename Reader>
float ReadSongTime(bool inSong, uintptr_t base,
    std::span<const unsigned int> baseOffsets, uintptr_t rare,
    std::span<const unsigned int> rareOffsets, Reader read) {
    const auto baseTime = ReadTimer(base, baseOffsets, read);
    if (!baseTime)
        return 0.f; // No cached value from a previous song/seek.
    if (inSong && *baseTime == 0.f) {
        const auto rareTime = ReadTimer(rare, rareOffsets, read);
        if (rareTime)
            return *rareTime;
    }
    return *baseTime;
}
}
