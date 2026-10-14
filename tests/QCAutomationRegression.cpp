#include "../DLL/Mods/QCPreviewRequest.hpp"
#include "../DLL/Mods/QCInputGate.hpp"
#define QC_AUTOMATION_TEST
#include "../DLL/Mods/QCAutomation.cpp"
#include "../DLL/Mods/ToneSourceTimeline.cpp"
#include <stdexcept>
#include <fstream>

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Reset() {
    ResetAutomationState();
    QCAutomation::Panel::testOverride.clear();
    Settings::values = {{"QCAutomationEnabled", "on"}, {"QCAutomationTransposeEnabled", "on"},
        {"QCAutomationDevice", "QC"}, {"QCAutomationMidiChannel", "0"},
        {"QCAutomationSceneCC", "43"}, {"QCAutomationIdleScene", "A"}};
    SongTuning::available = true; SongTuning::tuning = {};
    QCAutomation::ToneSource::metadataAvailable = false;
    QCAutomation::ToneSource::metadataSong = "test-song";
    QCAutomation::ToneSource::metadataArrangement = "lead";
    QCAutomation::ToneSource::metadataTuning = {};
    Midi::available = true; Midi::failScene = false; Midi::failPreset = false;
    Midi::messages.clear(); SongTimer::time = 10;
    GameState::song = "test-song"; GameState::arrangement = "lead";
    GameState::arrangementAvailable = true;
    QCAutomation::ToneSource::manualSlot = 0; QCAutomation::ToneSource::tone = "clean";
}
int Count(int controller) {
    return int(std::count_if(Midi::messages.begin(), Midi::messages.end(),
        [controller](const Midi::Message& m) { return m.controller == controller; }));
}
void Tick() { QCAutomation::HandleInSongState(); }
void Idle() { QCAutomation::HandleOutOfSongState(); }
void Steady(bool idle = false) {
    const auto count = Midi::messages.size();
    for (int i = 0; i < 10; ++i) { if (idle) Idle(); else Tick(); }
    Check(Midi::messages.size() == count, "steady state repeated MIDI");
}
void OverrideTests() {
    using namespace QCAutomation::Overrides;
    std::ifstream input("tests/qc-overrides-fixtures.json");
    nlohmann::json fixture; input >> fixture;
    Store store; Check(store.Reload(fixture.dump()), "fixture load failed");
    for (const auto& test : fixture["cases"]) {
        Check(store.Resolve({test["song"], NormalizeId(test["id"])}, test["tone"]) == test["expected"], "shared fixture resolution mismatch");
    }
    const Key key{"IcedEarth-1776", "B655B6893F984ACFB054F35D73386A77"};
    for (const auto& invalid : fixture["invalid"]) {
        Check(!store.Reload(invalid.dump()), "invalid file accepted");
        Check(store.Resolve(key, "Iced_Earth_Dist") == "Overdrive", "last good mappings lost");
        Check(!store.Save(key, {}, 1, 1, [](const std::string&) { return true; }), "invalid file overwritten");
    }
    Check(store.Reload(fixture.dump()), "valid reload failed");
    Check(!store.Save(key, {}, 1, 2, [](const std::string&) { return true; }), "stale save accepted");
    Check(!store.Save(key, {}, 2, 2, [](const std::string&) { return false; }), "failed write published");
    Check(store.Resolve(key, "Iced_Earth_Dist") == "Overdrive", "failed write lost mappings");
    std::string saved;
    Check(store.Save(key, {}, 2, 2, [&](const std::string& text) { saved = text; return true; }), "reset failed");
    Check(store.Resolve(key, "Iced_Earth_Dist").empty(), "Automatic did not remove override");
    Store restarted; Check(restarted.Reload(saved), "restart reload failed");
    Check(restarted.Resolve(key, "Iced_Earth_Dist").empty(), "reset not persisted");
    Check(store.SaveTone(key, "first", "Solo", 3, 3, [](const std::string&) { return true; }), "single save failed");
    Check(store.SaveTone(key, "second", "Clean", 4, 4, [](const std::string&) { return true; }), "second save failed");
    Check(store.Resolve(key, "first") == "Solo", "single save overwrote another tone");
    Check(!store.SaveTone(key, "first", "Mod", 4, 5, [](const std::string&) { return true; }), "stale single save accepted");
    Check(!store.SaveTone(key, "first", "Mod", 5, 5, [](const std::string&) { return false; }), "failed single save accepted");
    Check(store.Resolve(key, "first") == "Solo", "failed single save changed active role");
    Check(store.SaveTone(key, "first", "Automatic", 5, 5, [](const std::string&) { return true; }), "single Automatic failed");
    Check(store.Resolve(key, "first").empty() && store.Resolve(key, "second") == "Clean", "single Automatic isolation");
    Reset(); QCAutomation::ToneSource::tone = "Iced_Earth_Dist";
    QCAutomation::Panel::testOverride = "Overdrive"; Tick();
    Check(lastSentTarget == QcTarget::AutoOd, "override ignored by production decision"); Steady();
    Settings::values["QCAutomationAutoODTarget"] = "MyPresets:31B"; Tick();
    Check(lastSentTargetSpec == "MyPresets:31B", "global role address change ignored");
    QCAutomation::ToneSource::manualSlot = 3; Tick();
    Check(lastSentTarget == QcTarget::Manual3, "override beat manual slot");
    QCAutomation::ToneSource::manualSlot = 0; Tick();
    Check(lastSentTarget == QcTarget::AutoOd, "return to auto lost override");
    QCAutomation::Panel::testOverride.clear(); Tick();
    Check(lastSentTarget == QcTarget::AutoDist, "Automatic reset did not reclassify");
    Check(QCAutomation::DescribeTone("Iced_Earth_Dist", "Overdrive").target == "MyPresets:31B", "panel target disagrees with runtime");
    Idle(); Check(lastSentTarget == QcTarget::Idle, "override changed idle behavior");
}
void KeywordTests(const char* fixturePath) {
    Reset();
    std::ifstream input(fixturePath); nlohmann::json cases; input >> cases;
    for (const auto& test : cases) {
        std::string keyword;
        auto role = ClassifyAutoToneBucket(test["tone"].get<std::string>(), keyword);
        Check(ToString(role) == test["expected"].get<std::string>(), "default keyword fixture mismatch");
        if (test.contains("keyword")) Check(keyword == test["keyword"].get<std::string>(), "keyword evidence mismatch");
    }
    Settings::values["QCAutomationModPriority"] = "2";
    Settings::values["QCAutomationDistPriority"] = "3";
    Settings::values["QCAutomationCleanPriority"] = "4";
    std::string keyword;
    Check(ClassifyAutoToneBucket("MuseStock_distlofi", keyword) == AutoToneBucket::Mod, "custom priorities ignored");
    Settings::values["QCAutomationModKeywords"] = "custom_effect";
    Check(ClassifyAutoToneBucket("AlterRise_lofi", keyword) == AutoToneBucket::Clean, "custom list replaced by defaults");
    Check(ClassifyAutoToneBucket("custom_effect", keyword) == AutoToneBucket::Mod, "custom keyword ignored");
}
int main(int argc, char** argv) {
    try {
        KeywordTests("tests/qc-keyword-fixtures.json");
        if (argc > 1) KeywordTests(argv[1]);
        OverrideTests();
        using namespace QCAutomation::ToneSourceTimeline;
        ArrangementToneTimeline timeline; timeline.toneBase = "clean"; timeline.toneA = "dist";
        bool ambiguous = false;
        Check(!ResolveToneNameFromToneId(timeline, 0, ambiguous) && ambiguous, "ambiguous tone guessed");
        timeline.toneEvents = {{10, std::string("dist"), 1, false}, {20, {}, 0, true}};
        Check(ResolveCurrentAuthoredToneName(timeline, 15).authoredToneName == "dist", "timeline target wrong");
        Check(!ResolveCurrentAuthoredToneName(timeline, 25).authoredToneName, "unresolved event inherited tone");
        Check(ResolveCurrentAuthoredToneName(timeline, 2).authoredToneName == "clean", "seek/restart retained future tone");
        Check(ResolveAutomationTone(timeline, 8.99f).authoredToneName == "clean", "early before boundary");
        Check(ResolveAutomationTone(timeline, 9).authoredToneName == "dist", "one second lead missing");
        Check(ResolveCurrentAuthoredToneName(timeline, 9).authoredToneName == "clean", "actual tone shifted");
        Check(!ResolveAutomationTone(timeline, 19).authoredToneName, "early ambiguous tone guessed");
        Check(ResolveAutomationTone(timeline, 0).authoredToneName == "clean", "restart retained early tone");
        timeline.toneEvents = {{10, std::string("dist"), 1, false}, {10.5f, std::string("solo"), 2, false}};
        Check(ResolveAutomationTone(timeline, 9).authoredToneName == "dist", "close event skipped");
        Check(ResolveAutomationTone(timeline, 9.5f).authoredToneName == "solo", "close event ordering");
        Check(ResolveAutomationTone(timeline, 9.5f).authoredToneName == "solo", "paused timer changed target");
        Check(ResolveAutomationTone(timeline, 2).authoredToneName == "clean", "paused seek wrong target");
        Reset(); Tick();
        GameState::arrangement = "rhythm"; SongTuning::tuning.fill(byte(-1)); Tick();
        Check(activeSongSceneValue == 1, "arrangement change retained old tuning");
        SongTimer::time = 30; Tick(); SongTimer::time = 0; SongTuning::tuning.fill(byte(-2)); Tick();
        Check(activeSongSceneValue == 2, "restart did not recompute tuning");
        QCAutomation::Panel::PreviewRequest preview;
        Check(!preview.NeedsLoad(), "missing selection started loader");
        preview.Select("A"); const auto oldRequest = preview.generation;
        preview.Select("B"); preview.Select("A");
        Check(!preview.Complete(oldRequest) && preview.NeedsLoad(), "obsolete A-B-A request accepted");
        Check(preview.Complete(preview.generation) && !preview.NeedsLoad(), "fresh metadata rejected");
        preview.Select(""); preview.Select("A");
        Check(preview.NeedsLoad(), "same song did not reload after missing selection");
        QCAutomation::Panel::Input::InputGate<256> gate;
        Check(gate.Event(1, true, false), "ordinary press hidden");
        Check(gate.Event(1, false, true), "pre-editor release lost");
        Check(!gate.Event(2, true, true), "editor press leaked");
        Check(!gate.Event(2, true, false), "held editor key leaked after close");
        Check(!gate.Event(2, false, false), "editor release leaked");
        Check(gate.Event(2, true, false), "new press remained suppressed");
        Check(!gate.State(3, true, true) && !gate.State(3, true, false), "held polled key leaked");
        QCAutomation::Panel::Input::VisibleQueue<int> events;
        events.Push(128); events.Push(0);
        events.Discard([](int event) { return event == 128; });
        int output = -1;
        Check(events.Read(1, true, &output) == 1 && output == 0, "peek lost game-owned release");
        Check(events.Read(1, false, &output) == 1 && output == 0, "peek consumed release");
        Check(events.Read(1, false, &output) == 0, "release replayed");
        gate = {};
        Check(gate.Event(2, true, false), "focus reset retained suppression");
        gate.State(3, false, false);
        Check(gate.State(3, true, false), "polled key failed after release");
        for (int shift : {0, -1, -7}) {
            Reset(); SongTuning::tuning.fill(byte(shift)); Tick();
            Check(activeSongSceneValue == -shift, "valid tuning did not resolve");
            Check(lastSentSceneValue == -shift, "valid tuning scene not sent"); Steady();
        }
        Reset(); SongTuning::available = false; Tick();
        Check(!activeSongSceneValue && Count(43) == 0, "invalid tuning sent scene");
        SongTuning::available = true; Tick();
        Check(lastSentSceneValue == 0, "available E standard not retried");

        Reset(); SongTuning::available = false; Tick();
        QCAutomation::ToneSource::metadataAvailable = true;
        GameState::arrangementAvailable = false; Tick();
        Check(!activeSongSceneValue && Count(43) == 0, "cached identity accepted while fresh identity unavailable");
        GameState::arrangementAvailable = true; Tick();
        Check(lastSentSceneValue == 0, "matched E-standard metadata did not resolve scene A");
        Steady();
        GameState::arrangement = "rhythm"; Tick();
        Check(!activeSongSceneValue, "previous arrangement tuning leaked across transition");
        QCAutomation::ToneSource::metadataArrangement = "rhythm";
        QCAutomation::ToneSource::metadataTuning.fill(-1); Tick();
        Check(lastSentSceneValue == 1, "fresh metadata did not recover scene B");
        Reset(); SongTuning::available = false;
        QCAutomation::ToneSource::metadataAvailable = true;
        QCAutomation::ToneSource::metadataTuning.fill(255); Tick();
        Check(!activeSongSceneValue && Count(43) == 0, "out-of-range metadata wrapped to valid tuning");
        Reset(); QCAutomation::ToneSource::metadataAvailable = true;
        QCAutomation::ToneSource::metadataTuning.fill(-1); Tick();
        Check(lastSentSceneValue == 0, "metadata overrode available live E-standard tuning");

        Reset(); Settings::values["QCAutomationTransposeEnabled"] = "off"; Tick();
        Check(Count(43) == 0, "disabled transpose sent scene");
        Settings::values["QCAutomationTransposeEnabled"] = "on"; Tick();
        Check(lastSentSceneValue == 0, "mid-song enable did not resolve");
        Settings::values["QCAutomationTransposeEnabled"] = "off"; Tick();
        const int beforeEnable = Count(43);
        Settings::values["QCAutomationTransposeEnabled"] = "on"; Tick();
        Check(Count(43) > beforeEnable, "re-enable did not force cached scene"); Steady();

        for (bool idle : {false, true}) {
            Reset(); if (idle) Idle(); else Tick();
            for (const auto& setting : {std::pair<const char*, const char*>{"QCAutomationMidiChannel", "3"},
                    {"QCAutomationDevice", "QC second"}}) {
                Midi::messages.clear(); Settings::values[setting.first] = setting.second;
                if (idle) Idle(); else Tick();
                Check(Count(-1) == 1 && Count(43) >= 1, "destination did not resync");
                for (const auto& m : Midi::messages) {
                    Check(m.device == Settings::values["QCAutomationDevice"], "wrong destination");
                    Check(m.channel == 3, "wrong channel");
                }
                Steady(idle);
            }
            Midi::messages.clear(); Settings::values["QCAutomationSceneCC"] = "44";
            if (idle) Idle(); else Tick();
            Check(Count(-1) == 0 && Count(44) == 1, "scene CC change incorrect"); Steady(idle);
        }

        Reset(); Tick(); Midi::messages.clear(); Idle();
        Check(Count(-1) == 1 && Count(43) == 1, "idle recall did not force cached A");
        Check(Midi::messages.back().controller == 43, "idle scene sent before preset"); Steady(true);

        for (bool idle : {false, true}) {
            Reset(); Tick(); Midi::messages.clear(); Midi::failScene = true;
            if (idle) Idle(); else { QCAutomation::ToneSource::tone = "distortion"; Tick(); }
            Check(Count(-1) == 1 && sceneSendPending, "failed force not retained");
            Midi::messages.clear(); Midi::failScene = false;
            if (idle) Idle(); else Tick();
            Check(Count(-1) == 0 && Count(43) == 1 && !sceneSendPending, "failed force not retried"); Steady(idle);
        }

        Reset(); Tick(); Midi::messages.clear();
        Settings::values["QCAutomationDevice"] = "missing"; Midi::available = false; Tick();
        Check(!lastSentTarget, "failed destination marked sent");
        Midi::available = true; QCAutomation::ToneSource::tone.clear(); Tick();
        Check(!lastSentTarget, "missing tone consumed preset sync");
        QCAutomation::ToneSource::tone = "clean"; Tick();
        Check(Count(-1) == 1 && lastSentSceneValue == 0, "pending preset not synchronized");

        Reset(); Tick(); Midi::messages.clear(); Midi::failPreset = true;
        QCAutomation::ToneSource::tone = "distortion"; Tick();
        Check(lastSentTarget == QcTarget::AutoClean, "failed preset marked delivered");
        Midi::failPreset = false; Tick();
        Check(lastSentTarget == QcTarget::AutoDist, "preset not retried");
        for (int slot : {2, 3, 4}) {
            QCAutomation::ToneSource::manualSlot = slot; Tick();
            Check(lastSentTarget == (slot == 2 ? QcTarget::Manual2 : slot == 3 ? QcTarget::Manual3 : QcTarget::Manual4),
                "manual slot incorrect");
        }
        // Slot 1 clears the ToneSource override; exercise QC's resulting auto path.
        QCAutomation::ToneSource::manualSlot = 0; Tick();
        Check(lastSentTarget == QcTarget::AutoDist, "return to auto failed");
        Idle(); Check(lastSentTarget == QcTarget::Idle, "song exit did not select idle");
        std::cout << "PASS: QC tuning, synchronization, overrides, shared fixtures, preview generations, input gates, timeline seeks, restart, targets and idle\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
