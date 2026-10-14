#pragma once
#include <array>
#include <cstddef>
#include <deque>
#include <algorithm>
namespace QCAutomation::Panel::Input {
// A press owned by the editor stays hidden until its physical release.
template<size_t N> struct InputGate {
    std::array<bool, N> hidden{}, delivered{};
    bool State(size_t key, bool down, bool capture) {
        if (!down) hidden[key] = false;
        else if (capture) hidden[key] = true;
        return down && !capture && !hidden[key];
    }
    bool Event(size_t key, bool down, bool capture) {
        if (!down) {
            const bool release = delivered[key];
            hidden[key] = delivered[key] = false;
            return release;
        }
        if (capture) hidden[key] = true;
        if (hidden[key]) return false;
        delivered[key] = true;
        return true;
    }
};
template<class Event> struct VisibleQueue {
    std::deque<Event> events;
    void Push(const Event& event) { events.push_back(event); }
    template<class Predicate> void Discard(Predicate predicate) {
        for (auto it = events.begin(); it != events.end();)
            if (predicate(*it)) it = events.erase(it); else ++it;
    }
    size_t Read(size_t requested, bool peek, Event* output) {
        const size_t count = (std::min)(requested, events.size());
        for (size_t i = 0; i < count; ++i) if (output) output[i] = events[i];
        if (!peek) for (size_t i = 0; i < count; ++i) events.pop_front();
        return count;
    }
};
}
