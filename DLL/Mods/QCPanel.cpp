#include "../stdafx.h"
#include "QCPanel.hpp"
#include "QCPanelInput.hpp"
#include "QCOverrides.hpp"
#include "QCPreviewRequest.hpp"
#include "ToneSource.hpp"
#include "ToneSourceAssetLoader.hpp"
#include <future>
#include <mutex>
#include <atomic>
#include <set>

namespace QCAutomation::Panel {
namespace {
using Descriptor = ToneSourceAssetLoader::ManifestArrangementDescriptor;
struct Arrangement { Descriptor data; std::vector<std::string> tones; std::string warning; };
struct Row { std::string name, selectedRole; ToneDescription description; std::vector<std::string> choices; };
struct Snapshot {
    uint64_t generation = 0;
    bool inSong = false, editable = false, enabled = false, stale = false;
    std::string song, message, title, artist, tuning, warning, details;
    uint64_t completedRequest = 0;
    bool saveSucceeded = false;
    std::vector<std::string> arrangements;
    int selected = -1;
    std::vector<Row> rows;
    std::string tone, automationTone, role, target, mode, scene, send;
};
struct Command { uint64_t generation; int selection = -1; uint64_t request = 0; bool reset = false; std::string tone, role; };
std::mutex publicationMutex;
Snapshot published;
std::vector<Command> commands;
// Everything below, except the atomics and Render-local editor state, is runtime-thread owned.
Overrides::Store store;
Snapshot state;
std::vector<Arrangement> arrangements;
std::future<std::vector<Arrangement>> loading;
std::string loadingSong, requestedSong, lastPreview, activeHash, fileContents;
PreviewRequest previewRequest;
uint64_t loadingGeneration = 0;
bool fileInitialized = false, fileExisted = false;
std::chrono::steady_clock::time_point lastFileCheck{}, successUntil{};
std::atomic<bool> expanded{false}, capture{false}, cancelEditor{false}, inputAllowed{false};
void SetCapture(bool value) { if (capture.exchange(value) && !value) Input::EndCapture(); }
std::array<bool, 3> swallowedMouse{};
std::array<bool, 256> swallowed{}; // WndProc thread only
std::filesystem::path OverridePath() {
    wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path() / L"RSMods" / L"QCOverrides.json";
}
void ReloadFile() {
    auto now = std::chrono::steady_clock::now();
    if (fileInitialized && now - lastFileCheck < std::chrono::seconds(1)) return;
    lastFileCheck = now;
    try {
        auto path = OverridePath();
        bool exists = std::filesystem::exists(path);
        if (!exists) {
            if (!fileInitialized || fileExisted) { store = {}; fileContents.clear(); ++state.generation; }
        } else {
            std::ifstream input(path, std::ios::binary);
            if (!input) throw std::runtime_error("Unable to read QC overrides");
            std::string text((std::istreambuf_iterator<char>(input)), {});
            if (!fileInitialized || !fileExisted || text != fileContents || !store.writable) { store.Reload(text); fileContents = text; ++state.generation; }
        }
        fileInitialized = true; fileExisted = exists;
    } catch (const std::exception& ex) { store.writable = false; store.error = ex.what(); }
}
bool WriteFile(const std::string& text) {
    const auto path = OverridePath(); auto temporary = path; temporary += L".tmp";
    try {
        std::filesystem::create_directories(path.parent_path());
        { std::ofstream output(temporary, std::ios::binary | std::ios::trunc); output << text; output.flush(); if (!output) return false; }
        const bool exists = std::filesystem::exists(path);
        std::string current;
        if (exists) {
            std::ifstream input(path, std::ios::binary);
            if (!input) return false;
            current.assign(std::istreambuf_iterator<char>(input), {});
        }
        if (exists != fileExisted || (exists && current != fileContents)) {
            lastFileCheck = {}; ++state.generation;
            return false;
        }
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return false;
        fileContents = text; fileExisted = true; fileInitialized = true; return true;
    } catch (...) { return false; }
}
std::string Identity(const Descriptor& d) { return d.persistentIDNormalized.empty() ? d.arrangementIDNormalized : d.persistentIDNormalized; }
Overrides::Key SelectedKey() {
    if (state.selected < 0 || state.selected >= int(arrangements.size())) return {};
    const auto& d = arrangements[state.selected].data; return {d.songKey, Identity(d)};
}
std::vector<Arrangement> Load(const std::string& song) {
    std::vector<Arrangement> result;
    for (auto& d : ToneSourceAssetLoader::GetDescriptorsForSong(song)) {
        Arrangement a; a.data = d;
        std::set<std::string> seen;
        auto add = [&](const std::string& tone) { if (!tone.empty() && seen.insert(tone).second) a.tones.push_back(tone); };
        for (const auto& tone : {d.toneBase, d.toneA, d.toneB, d.toneC, d.toneD}) add(tone);
        ToneSourceTimeline::ArrangementToneTimeline timeline; std::string error;
        if (!ToneSourceAssetLoader::BuildTimelineFromDescriptor(d, timeline, error)) a.warning = "Timeline unavailable: " + error;
        else {
            for (const auto& event : timeline.toneEvents) if (event.authoredToneName) add(*event.authoredToneName);
            if (timeline.ambiguousToneIdZeroCount || timeline.unresolvedToneNameCount || timeline.ignoredOutOfRangeToneIdCount)
                a.warning = "Timeline contains unresolved or ambiguous events; mappings cannot identify those events.";
        }
        if (a.tones.empty()) a.warning += " No authored tones available.";
        result.push_back(std::move(a));
    }
    return result;
}
void Queue(Command command) { std::lock_guard<std::mutex> lock(publicationMutex); commands.push_back(std::move(command)); }
}

bool WantsInputCapture() { return capture.load(); }
void Tick(bool inSong) {
    const auto oldGeneration = state.generation;
    state.inSong = inSong;
    if (state.saveSucceeded && std::chrono::steady_clock::now() >= successUntil) state.message.clear();
    state.enabled = Settings::ReturnSettingValue("QCAutomationEnabled") == "on";
    ReloadFile();
    std::string preview, runtime;
    const bool fresh = GameState::TryGetPreviewSongKey(preview);
    if (fresh) lastPreview = preview;
    const bool validRuntime = inSong && GameState::TryGetCurrentArrangementID(runtime);
    std::string song = inSong ? GameState::GetSongKey() : (fresh ? preview : lastPreview);
    state.stale = !inSong && !fresh;
    previewRequest.Select(song);
    if (song != requestedSong || runtime != activeHash) {
        requestedSong = song; activeHash = runtime; ++state.generation;
        state.selected = -1; state.rows.clear(); cancelEditor = true;
        if (state.song != song) { arrangements.clear(); state.arrangements.clear(); }
        state.song = song;
    }
    if (loading.valid() && loading.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto result = loading.get();
            if (previewRequest.Complete(loadingGeneration)) { arrangements = std::move(result); ++state.generation; state.selected = -1; } else loadingSong.clear();
        } catch (const std::exception& ex) { state.message = ex.what(); }
    }
    if (!loading.valid() && previewRequest.NeedsLoad()) {
        loadingSong = requestedSong; loadingGeneration = previewRequest.generation;
        loading = std::async(std::launch::async, Load, loadingSong);
    }
    state.arrangements.clear();
    for (const auto& a : arrangements) state.arrangements.push_back(a.data.arrangementName);
    if (inSong) {
        state.selected = -1;
        if (validRuntime) for (int i = 0; i < int(arrangements.size()); ++i) {
            const auto& d = arrangements[i].data;
            if (runtime == d.persistentIDNormalized || runtime == d.arrangementIDNormalized) { state.selected = i; break; }
        }
    } else if (state.selected < 0 && !arrangements.empty()) state.selected = 0;
    const bool paused = GameState::currentMenu.find("Pause") != std::string::npos;
    state.editable = (!inSong || paused) && !state.stale && state.selected >= 0 && store.writable;
    // Paused playback has an authoritative runtime identity even when previews are absent.
    if (inSong && paused && validRuntime && state.selected >= 0 && store.writable) state.editable = true;
    if (state.selected >= 0) {
        auto key = SelectedKey();
        int matches = 0; for (const auto& a : arrangements) if (a.data.songKey == key.song && Identity(a.data) == key.arrangement) ++matches;
        if (matches != 1) state.editable = false;
    }
    if (!Input::Ready()) { state.editable = false; state.message = "Input capture unavailable; editor disabled."; }
    if (!state.editable) cancelEditor = true;
    std::vector<Command> pending;
    { std::lock_guard<std::mutex> lock(publicationMutex); pending.swap(commands); }
    if (oldGeneration != state.generation) { state.message.clear(); state.saveSucceeded = false; }
    for (const auto& command : pending) {
        if (command.request) { state.completedRequest = command.request; state.saveSucceeded = false; }
        if (command.generation != state.generation) { state.message = "Song context changed; selection not saved."; continue; }
        if (command.selection >= 0 && !inSong && !state.stale && command.selection < int(arrangements.size())) {
            state.selected = command.selection; ++state.generation; cancelEditor = true; state.message.clear();
        } else if (command.request) {
            if (!state.editable) { state.message = "Mapping unavailable; selection not saved."; continue; }
            const auto& names = arrangements[state.selected].tones;
            const bool valid = command.reset || std::find(names.begin(), names.end(), command.tone) != names.end();
            const bool saved = valid && (command.reset
                ? store.Save(SelectedKey(), {}, command.generation, state.generation, WriteFile)
                : store.SaveTone(SelectedKey(), command.tone, command.role, command.generation, state.generation, WriteFile));
            if (saved) {
                ++state.generation; state.message = command.reset ? "Arrangement mappings reset." : "Mapping saved.";
                state.saveSucceeded = true; successUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
                LOG_INFO("[QCPanel] Saved arrangement mapping request " << command.request << std::endl);
            } else state.message = valid ? store.error : "Unknown authored tone; selection not saved.";
        }
    }
    state.rows.clear(); state.title.clear(); state.artist.clear(); state.tuning.clear(); state.warning.clear(); state.details.clear();
    if (state.selected >= 0) {
        const auto& a = arrangements[state.selected];
        state.title = a.data.title; state.artist = a.data.artist;
        state.tuning = DescribeTuning(a.data.tuning, a.data.tuningAvailable);
        state.tuning += a.data.pitchAvailable ? " | pitch offset " + std::to_string(a.data.cents) + " cents" : " | pitch offset unavailable";
        state.details = "Song key: " + a.data.songKey + "\nArrangement ID: " + Identity(a.data);
        if (!a.warning.empty()) {
            state.details += "\n" + a.warning;
            state.warning = a.warning.find("Timeline unavailable") == 0
                ? "Automatic tone changes unavailable - using the song's base tone."
                : "Some tone changes cannot be identified. See Details.";
        }
        for (const auto& tone : a.tones) {
            auto role = store.Resolve(SelectedKey(), tone);
            Row row{tone, role, DescribeTone(tone, role)};
            for (const char* choice : Overrides::Roles) {
                auto description = DescribeTone(tone, std::string(choice) == "Automatic" ? "" : choice);
                row.choices.push_back(std::string(choice) + (std::string(choice) == "Automatic" ? " (" + description.effectiveRole + ")" : "") + " - " + description.target);
            }
            state.rows.push_back(std::move(row));
        }
    }
    if (!store.error.empty()) state.message = store.error;
    { std::lock_guard<std::mutex> lock(publicationMutex); published = state; }
}
std::string CurrentOverride(const std::string& tone) {
    std::string song, id, fresh;
    if (!GameState::TryGetCurrentArrangementID(fresh) || !ToneSource::GetActiveIdentity(song, id, fresh)) return {};
    return store.Resolve({song, id}, tone);
}
void PublishStatus(const std::string& tone, const std::string& automationTone, const std::string& role, const std::string& target,
    const std::string& mode, const std::string& scene, const std::string& send) {
    state.tone = tone; state.automationTone = automationTone; state.role = role; state.target = target; state.mode = mode; state.scene = scene; state.send = send;
    std::lock_guard<std::mutex> lock(publicationMutex); published = state;
}
void Render() {
    Snapshot view; { std::lock_guard<std::mutex> lock(publicationMutex); view = published; }
    static uint64_t nextRequest = 0, pendingRequest = 0, confirmationGeneration = 0;
    static std::string pendingTone, pendingRole;
    static bool confirmReset = false;
    if (pendingRequest && view.completedRequest >= pendingRequest) { pendingRequest = 0; pendingTone.clear(); }
    if (cancelEditor.exchange(false) || !expanded || !view.editable || confirmationGeneration != view.generation) confirmReset = false;
    if (!view.enabled && !expanded) { SetCapture(false); return; }
    const bool open = expanded.load();
    inputAllowed = !view.inSong || view.editable;
    SetCapture(open && inputAllowed);
    ImGui::SetNextWindowPos(ImVec2(18, 18), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(open ? 800.0f : 460.0f, 0), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse;
    if (!capture) flags |= ImGuiWindowFlags_NoInputs;
    ImGui::Begin("Quad Cortex  |  F10", nullptr, flags);
    ImGui::TextWrapped("%s%s%s", view.title.empty() ? view.song.c_str() : view.title.c_str(), view.artist.empty() ? "" : " - ", view.artist.c_str());
    if (view.selected >= 0 && view.selected < int(view.arrangements.size())) ImGui::Text("%s: %s", view.inSong ? "Playing arrangement" : "Preview arrangement", view.arrangements[view.selected].c_str());
    ImGui::TextWrapped("%s | Scene %s | %s", view.mode == "auto" ? "Automatic" : (view.mode == "idle" ? "Idle" : "Manual"), view.scene.c_str(), view.enabled ? view.send.c_str() : "Automation disabled");
    ImGui::TextWrapped("QC sound selected: %s", view.target.empty() ? "Waiting for tone" : view.target.c_str());
    if (view.inSong) ImGui::TextWrapped("Song tone now: %s", view.tone.empty() ? "Unavailable" : view.tone.c_str());
    if (open) {
        ImGui::Separator();
        if (view.stale) ImGui::TextWrapped("Last preview - current selection unavailable. Mapping changes disabled.");
        if (view.arrangements.empty()) ImGui::TextWrapped("Loading song information, or metadata unavailable.");
        ImGui::BeginDisabled(pendingRequest != 0);
        if (!view.inSong && !view.stale && ImGui::BeginCombo("Preview arrangement", view.selected < 0 ? "Select" : view.arrangements[view.selected].c_str())) {
            for (int i = 0; i < int(view.arrangements.size()); ++i) if (ImGui::Selectable(view.arrangements[i].c_str(), i == view.selected)) Queue({view.generation, i});
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (!view.warning.empty()) ImGui::TextWrapped("%s", view.warning.c_str());
        if (!view.message.empty()) ImGui::TextWrapped("%s", view.message.c_str());
        if (pendingRequest) ImGui::TextWrapped("Saving %s%s%s...", pendingTone.empty() ? "arrangement mappings" : pendingTone.c_str(), pendingTone.empty() ? "" : " -> ", pendingTone.empty() ? "" : pendingRole.c_str());
        if (view.inSong && !view.editable) ImGui::TextUnformatted("Pause the song to change mappings.");
        ImGui::TextUnformatted("Song tone                         QC sound (changes save automatically)");
        ImGui::BeginChild("tones", ImVec2(0, 230), true);
        for (const auto& row : view.rows) {
            ImGui::PushID(row.name.c_str());
            if (view.inSong && view.mode == "auto" && row.name == view.automationTone)
                ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.75f, 1), "%s  [QC selected]", row.name.c_str());
            else ImGui::TextWrapped("%s", row.name.c_str());
            ImGui::TextUnformatted("->"); ImGui::SameLine();
            int selected = 0;
            for (int i = 1; i <= 5; ++i) if (row.selectedRole == Overrides::Roles[i]) selected = i;
            ImGui::SetNextItemWidth(-1);
            ImGui::BeginDisabled(!view.editable || pendingRequest != 0);
            if (ImGui::BeginCombo("##role", row.choices[selected].c_str())) {
                for (int i = 0; i <= 5; ++i) if (ImGui::Selectable(row.choices[i].c_str(), selected == i) && selected != i) {
                    pendingRequest = ++nextRequest; pendingTone = row.name; pendingRole = Overrides::Roles[i];
                    Queue({view.generation, -1, pendingRequest, false, row.name, pendingRole});
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            if (!row.description.warning.empty()) ImGui::TextWrapped("%s", row.description.warning.c_str());
            ImGui::Separator(); ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::BeginDisabled(!view.editable || pendingRequest != 0 || view.rows.empty());
        if (!confirmReset && ImGui::Button("Reset arrangement mappings")) { confirmReset = true; confirmationGeneration = view.generation; }
        if (confirmReset) {
            ImGui::TextUnformatted("Return every tone in this arrangement to Automatic?");
            if (ImGui::Button("Yes, reset")) { pendingRequest = ++nextRequest; pendingTone.clear(); Queue({view.generation, -1, pendingRequest, true}); confirmReset = false; }
            ImGui::SameLine(); if (ImGui::Button("Keep mappings")) confirmReset = false;
        }
        ImGui::EndDisabled();
        ImGui::TextUnformatted("Automatic changes use a 1-second song-time lead.");
        if (ImGui::CollapsingHeader("Details")) {
            ImGui::TextWrapped("%s", view.tuning.c_str());
            ImGui::TextWrapped("%s", view.details.c_str());
            ImGui::TextWrapped("QC target tone: %s | Role: %s", view.automationTone.c_str(), view.role.c_str());
        }
    }
    ImGui::End();
}
bool HandleInput(HWND window, UINT message, WPARAM key, LPARAM parameter) {
    const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    const bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
    if (message == WM_KILLFOCUS || (message == WM_ACTIVATEAPP && !key)) {
        if (ImGui::GetCurrentContext()) ImGui_ImplWin32_WndProcHandler(window, message, key, parameter);
        Input::Clear(); swallowed.fill(false); swallowedMouse.fill(false); expanded = false; SetCapture(false); cancelEditor = true;
    }
    if ((down || up) && key == VK_F10 && GameState::GameLoaded) {
        if (down && !(parameter & (1L << 30))) { expanded = !expanded; SetCapture(expanded && inputAllowed); cancelEditor = true; }
        return true;
    }
    if (up && key < swallowed.size() && swallowed[key]) { ImGui_ImplWin32_WndProcHandler(window, message, key, parameter); swallowed[key] = false; return true; }
    int mouseButton = message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ? 0 :
        message == WM_RBUTTONDOWN || message == WM_RBUTTONUP ? 1 : message == WM_MBUTTONDOWN || message == WM_MBUTTONUP ? 2 : -1;
    bool mouseUp = message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP;
    if (mouseButton >= 0 && mouseUp) {
        if (!swallowedMouse[mouseButton]) return false;
        swallowedMouse[mouseButton] = false;
        ImGui_ImplWin32_WndProcHandler(window, message, key, parameter); return true;
    }
    if (mouseButton >= 0 && capture) swallowedMouse[mouseButton] = true;
    // Releases for keys pressed before the panel opened still belong to Rocksmith.
    if (up) return false;
    if (down && key < swallowed.size() && swallowed[key]) return true;
    if (!capture) return false;
    ImGui_ImplWin32_WndProcHandler(window, message, key, parameter);
    if (down || up) {
        if (down && key < swallowed.size() && !(parameter & (1L << 30))) swallowed[key] = true;
        if (down && key == VK_ESCAPE) { expanded = false; SetCapture(false); cancelEditor = true; }
        return true;
    }
    return message == WM_CHAR || message == WM_SYSCHAR || (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST);
}
}
