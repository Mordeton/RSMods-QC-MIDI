#include "../DLL/SongTimerRead.hpp"
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
#include <type_traits>

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        constexpr std::array<unsigned int, 2> offsets{0x10, 0x20};
        std::map<uintptr_t, uintptr_t> pointers{{0x100, 0x200}, {0x210, 0x300},
                                               {0x400, 0x500}, {0x510, 0x600}};
        std::map<uintptr_t, float> values{{0x320, 12.f}, {0x620, 42.f}};
        auto read = [&](uintptr_t address, auto& value) {
            if constexpr (std::is_same_v<std::remove_reference_t<decltype(value)>, float>) {
                const auto it = values.find(address);
                if (it == values.end()) return false;
                value = it->second;
            } else {
                const auto it = pointers.find(address);
                if (it == pointers.end()) return false;
                value = it->second;
            }
            return true;
        };
        auto time = [&](bool inSong = true) {
            return SongTimer::Detail::ReadSongTime(inSong, 0x100, offsets, 0x400, offsets, read);
        };
        Check(time() == 12.f, "base timer lost");
        pointers.erase(0x100);
        Check(time() == 0.f, "unreadable root reused previous song time");
        pointers[0x100] = 0x200;
        pointers.erase(0x210);
        Check(time() == 0.f, "unreadable intermediate pointer accepted");
        // Actual crash: valid chain reads leave an unreadable final float.
        pointers[0x210] = 0x24748ae8;
        Check(time() == 0.f, "invalid final timer accepted");
        pointers[0x210] = 0;
        Check(time() == 0.f, "null intermediate pointer accepted");
        pointers[0x210] = (std::numeric_limits<uintptr_t>::max)();
        Check(time() == 0.f, "offset overflow accepted");
        pointers[0x210] = 0x300;
        values[0x320] = 13.f;
        Check(time() == 13.f, "fresh timer did not recover after loading");
        values[0x320] = 0.f;
        Check(time() == 42.f, "rare timer fallback lost");
        Check(time(false) == 0.f, "rare timer used outside song mode");
        values.erase(0x620);
        Check(time() == 0.f, "unreadable rare timer accepted");
        values[0x320] = -0.5f;
        Check(time() == -0.5f, "negative count-in time changed");
        values[0x320] = std::numeric_limits<float>::quiet_NaN();
        Check(time() == 0.f, "NaN timer accepted");
        values[0x320] = std::numeric_limits<float>::infinity();
        Check(time() == 0.f, "infinite timer accepted");
        values[0x320] = 1.f;
        auto disappearing = [&](uintptr_t address, auto& value) {
            const bool ok = read(address, value);
            if (address == 0x210) values.erase(0x320);
            return ok;
        };
        Check(SongTimer::Detail::ReadSongTime(true, 0x100, offsets, 0x400,
            offsets, disappearing) == 0.f, "released final page accepted");
        values[0x320] = 2.f;
        Check(time() == 2.f, "timer failed to recover from released final page");
        std::cout << "PASS: production song timer loading, fallback and recovery\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
