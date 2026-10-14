#include "../stdafx.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "QCPanelInput.hpp"
#include "QCInputGate.hpp"
#include <atomic>
#include <mutex>
#include <array>
#include <deque>
#include <map>

namespace QCAutomation::Panel::Input {
namespace {
using StateFn = HRESULT (STDMETHODCALLTYPE*)(IDirectInputDevice8W*, DWORD, LPVOID);
using DataFn = HRESULT (STDMETHODCALLTYPE*)(IDirectInputDevice8W*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
StateFn mouseState = nullptr, keyboardState = nullptr;
DataFn mouseData = nullptr, keyboardData = nullptr;
std::atomic<bool> ready{false};
std::atomic<unsigned> buttons{0}, presses{0};
std::atomic<long> wheel{0};
std::mutex maskMutex;
InputGate<256> stateKeys;
InputGate<8> stateButtons;
std::atomic<unsigned> closeEpoch{0};
unsigned keyEpoch = 0, mouseEpoch = 0;
struct BufferedDevice {
    InputGate<256> keys;
    InputGate<8> mouse;
    unsigned epoch = 0;
    VisibleQueue<DIDEVICEOBJECTDATA> visible;
};
std::map<IDirectInputDevice8W*, BufferedDevice> buffered;
DWORD DeviceType(IDirectInputDevice8W* device) {
    DIDEVCAPS caps{}; caps.dwSize = sizeof(caps);
    return SUCCEEDED(device->GetCapabilities(&caps)) ? GET_DIDEVICE_TYPE(caps.dwDevType) : 0;
}
HRESULT FilterState(StateFn original, IDirectInputDevice8W* device, DWORD size, LPVOID data) {
    const HRESULT result = original(device, size, data);
    if (FAILED(result) || !data) return result;
    const DWORD type = DeviceType(device);
    static std::atomic<bool> loggedMouse{false}, loggedKeyboard{false};
    if (type == DI8DEVTYPE_MOUSE && !loggedMouse.exchange(true)) LOG_INFO("[QCPanel] DirectInput mouse state bytes=" << size << std::endl);
    if (type == DI8DEVTYPE_KEYBOARD && !loggedKeyboard.exchange(true)) LOG_INFO("[QCPanel] DirectInput keyboard state bytes=" << size << std::endl);
    if (!((type == DI8DEVTYPE_KEYBOARD && size == 256) ||
        (type == DI8DEVTYPE_MOUSE && (size == sizeof(DIMOUSESTATE) || size == sizeof(DIMOUSESTATE2))))) return result;
    bool capture = WantsInputCapture();
    std::lock_guard<std::mutex> lock(maskMutex);
    auto& epoch = type == DI8DEVTYPE_KEYBOARD ? keyEpoch : mouseEpoch;
    const unsigned currentEpoch = closeEpoch.load();
    capture = capture || epoch != currentEpoch;
    epoch = currentEpoch;
    if (type == DI8DEVTYPE_KEYBOARD && size == 256) {
        auto bytes = static_cast<BYTE*>(data);
        for (size_t i = 0; i < 256; ++i) {
            if (!stateKeys.State(i, (bytes[i] & 0x80) != 0, capture)) bytes[i] = 0;
        }
    } else if (type == DI8DEVTYPE_MOUSE && (size == sizeof(DIMOUSESTATE) || size == sizeof(DIMOUSESTATE2))) {
        auto mouse = static_cast<DIMOUSESTATE2*>(data);
        unsigned current = 0; const int count = size == sizeof(DIMOUSESTATE) ? 4 : 8;
        for (int i = 0; i < count; ++i) if (mouse->rgbButtons[i] & 0x80) current |= 1u << i;
        const unsigned previous = buttons.exchange(current);
        if (capture) { presses.fetch_or(current & ~previous); wheel.fetch_add(mouse->lZ); }
        for (int i = 0; i < count; ++i) {
            if (!stateButtons.State(i, (current & (1u << i)) != 0, capture)) mouse->rgbButtons[i] = 0;
        }
        if (capture) mouse->lX = mouse->lY = mouse->lZ = 0;
    }
    return result;
}
HRESULT FilterData(DataFn original, IDirectInputDevice8W* device, DWORD size, LPDIDEVICEOBJECTDATA data, LPDWORD count, DWORD flags) {
    const DWORD type = DeviceType(device);
    if (!count || size != sizeof(DIDEVICEOBJECTDATA) || (flags & ~DIGDD_PEEK) ||
        (type != DI8DEVTYPE_MOUSE && type != DI8DEVTYPE_KEYBOARD)) return original(device, size, data, count, flags);
    const DWORD requested = *count;
    if (!requested) return original(device, size, data, count, flags);
    std::lock_guard<std::mutex> lock(maskMutex);
    auto& queue = buffered[device];
    const unsigned epoch = closeEpoch.load();
    const bool capture = WantsInputCapture();
    const bool quarantine = capture || queue.epoch != epoch;
    DWORD available = INFINITE;
    HRESULT result = original(device, size, nullptr, &available, DIGDD_PEEK);
    if (FAILED(result)) { queue = {}; queue.epoch = epoch; return result; }
    // Own a filtered queue so PEEK followed by consume sees identical releases.
    std::vector<DIDEVICEOBJECTDATA> incoming(available);
    if (available) {
        HRESULT read = original(device, size, incoming.data(), &available, 0);
        if (FAILED(read)) return read;
        if (read == DI_BUFFEROVERFLOW) result = read;
    }
    if (quarantine) {
        queue.visible.Discard([&](const DIDEVICEOBJECTDATA& event) {
            const bool button = type == DI8DEVTYPE_KEYBOARD || (event.dwOfs >= DIMOFS_BUTTON0 && event.dwOfs <= DIMOFS_BUTTON7);
            return !button || (event.dwData & 0x80);
        });
    }
    for (DWORD i = 0; i < available; ++i) {
        const auto& event = incoming[i];
        bool keep = !quarantine;
        if (type == DI8DEVTYPE_KEYBOARD && event.dwOfs < 256)
            keep = queue.keys.Event(event.dwOfs, (event.dwData & 0x80) != 0, quarantine);
        else if (type == DI8DEVTYPE_MOUSE) {
            if (event.dwOfs >= DIMOFS_BUTTON0 && event.dwOfs <= DIMOFS_BUTTON7) {
                const unsigned index = event.dwOfs - DIMOFS_BUTTON0, mask = 1u << index;
                keep = queue.mouse.Event(index, (event.dwData & 0x80) != 0, quarantine);
                if (event.dwData & 0x80) { buttons.fetch_or(mask); if (capture) presses.fetch_or(mask); }
                else buttons.fetch_and(~mask);
            } else if (capture && event.dwOfs == DIMOFS_Z) wheel.fetch_add(static_cast<LONG>(event.dwData));
        }
        if (keep) queue.visible.Push(event);
    }
    queue.epoch = epoch;
    *count = static_cast<DWORD>(queue.visible.Read(requested, (flags & DIGDD_PEEK) != 0, data));
    return result;
}
HRESULT STDMETHODCALLTYPE MouseState(IDirectInputDevice8W* d, DWORD n, LPVOID p) { return FilterState(mouseState, d, n, p); }
HRESULT STDMETHODCALLTYPE KeyboardState(IDirectInputDevice8W* d, DWORD n, LPVOID p) { return FilterState(keyboardState, d, n, p); }
HRESULT STDMETHODCALLTYPE MouseData(IDirectInputDevice8W* d, DWORD n, LPDIDEVICEOBJECTDATA p, LPDWORD c, DWORD f) { return FilterData(mouseData, d, n, p, c, f); }
HRESULT STDMETHODCALLTYPE KeyboardData(IDirectInputDevice8W* d, DWORD n, LPDIDEVICEOBJECTDATA p, LPDWORD c, DWORD f) { return FilterData(keyboardData, d, n, p, c, f); }
}
bool Initialize() {
    static bool attempted = false;
    if (attempted) return ready;
    attempted = true;
    HMODULE module = LoadLibraryW(L"dinput8.dll");
    using CreateFn = HRESULT (WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    auto create = module ? reinterpret_cast<CreateFn>(GetProcAddress(module, "DirectInput8Create")) : nullptr;
    IDirectInput8W* input = nullptr; IDirectInputDevice8W* mouse = nullptr; IDirectInputDevice8W* keyboard = nullptr;
    const GUID inputGuid{0xbf798031, 0x483a, 0x4da2, {0xaa,0x99,0x5d,0x64,0xed,0x36,0x97,0x00}};
    const GUID mouseGuid{0x6f1d2b60, 0xd5a0, 0x11cf, {0xbf,0xc7,0x44,0x45,0x53,0x54,0,0}};
    const GUID keyboardGuid{0x6f1d2b61, 0xd5a0, 0x11cf, {0xbf,0xc7,0x44,0x45,0x53,0x54,0,0}};
    if (create && SUCCEEDED(create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, inputGuid, reinterpret_cast<void**>(&input), nullptr)) &&
        SUCCEEDED(input->CreateDevice(mouseGuid, &mouse, nullptr)) && SUCCEEDED(input->CreateDevice(keyboardGuid, &keyboard, nullptr))) {
        auto mv = *reinterpret_cast<void***>(mouse); auto kv = *reinterpret_cast<void***>(keyboard);
        mouseState = reinterpret_cast<StateFn>(mv[9]); mouseData = reinterpret_cast<DataFn>(mv[10]);
        keyboardState = reinterpret_cast<StateFn>(kv[9]); keyboardData = reinterpret_cast<DataFn>(kv[10]);
        mouseState = reinterpret_cast<StateFn>(DetourFunction(reinterpret_cast<PBYTE>(mv[9]), reinterpret_cast<PBYTE>(MouseState)));
        mouseData = reinterpret_cast<DataFn>(DetourFunction(reinterpret_cast<PBYTE>(mv[10]), reinterpret_cast<PBYTE>(MouseData)));
        if (kv[9] != mv[9]) keyboardState = reinterpret_cast<StateFn>(DetourFunction(reinterpret_cast<PBYTE>(kv[9]), reinterpret_cast<PBYTE>(KeyboardState)));
        if (kv[10] != mv[10]) keyboardData = reinterpret_cast<DataFn>(DetourFunction(reinterpret_cast<PBYTE>(kv[10]), reinterpret_cast<PBYTE>(KeyboardData)));
        ready = mouseState && mouseData && keyboardState && keyboardData;

    }
    if (keyboard) keyboard->Release(); if (mouse) mouse->Release(); if (input) input->Release();
    LOG_INFO("[QCPanel] DirectInput capture " << (ready ? "ready" : "unavailable; editor disabled") << std::endl);
    return ready;
}
bool Ready() { return ready; }
void BeforeFrame() {
    if (!WantsInputCapture()) { presses = 0; wheel = 0; return; }
    ImGuiIO& io = ImGui::GetIO();
    unsigned down = buttons.load() | presses.exchange(0);
    for (int i = 0; i < 5; ++i) io.MouseDown[i] = (down & (1u << i)) != 0;
    io.MouseWheel += float(wheel.exchange(0)) / WHEEL_DELTA;
}
void EndCapture() { ++closeEpoch; }
void Clear() {
    std::lock_guard<std::mutex> lock(maskMutex);
    buttons = 0; presses = 0; wheel = 0;
    stateKeys = {}; stateButtons = {}; buffered.clear(); ++closeEpoch;
}
}
