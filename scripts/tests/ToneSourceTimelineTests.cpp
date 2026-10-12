#include "../../DLL/Mods/ToneSourceTimeline.hpp"
#include <cstdlib>
#include <iostream>

using namespace QCAutomation::ToneSourceTimeline;

void expect(bool result, const char* message) {
    if (!result) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main() {
    ArrangementToneTimeline timeline;
    timeline.toneBase = "Clean";
    timeline.toneA = "Distortion";
    timeline.toneB = "Solo";
    timeline.toneEvents = {{5.f, "Distortion", 0, false}, {10.f, "Solo", 1, false}};
    expect(ResolveCurrentAuthoredToneName(timeline, 0.f).authoredToneName == "Clean", "base tone before first event");
    expect(ResolveCurrentAuthoredToneName(timeline, 5.f).authoredToneName == "Distortion", "tone at exact event boundary");
    expect(ResolveCurrentAuthoredToneName(timeline, 11.f).authoredToneName == "Solo", "last authored tone persists");
    expect(ResolveCurrentAuthoredToneName(timeline, 2.f).authoredToneName == "Clean", "rewind returns to base");
    bool ambiguous = false;
    expect(!ResolveToneNameFromToneId(timeline, 0, ambiguous) && ambiguous, "conflicting base and A remain unresolved");
    expect(ResolveToneNameFromToneId(timeline, 1, ambiguous) == "Solo" && !ambiguous, "tone B resolves unambiguously");
    expect(!ResolveToneNameFromToneId(timeline, 8, ambiguous), "invalid tone IDs are unresolved");
    timeline.toneEvents = {{5.f, std::nullopt, 0, true}};
    auto unresolved = ResolveCurrentAuthoredToneName(timeline, 6.f);
    expect(!unresolved.authoredToneName && unresolved.unresolvedToneId == 0, "unresolved event does not invent a tone");
    std::cout << "QC timeline tests passed\n";
}
