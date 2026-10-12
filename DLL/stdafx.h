#pragma once
#define NOMINMAX

// STL
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Windows
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <endpointvolume.h>
#include <ImageHlp.h>
#include <intrin.h>
#include <mmdeviceapi.h>
#include <process.h>
#include <psapi.h>
#include <Functiondiscoverykeys_devpkey.h>

// External Dependenices
#include "Lib/Detours/detours.h"
#include "Lib/DirectX/d3d9types.h"
#include "Lib/DirectX/d3d9.h"
#include "Lib/DirectX/d3dx9tex.h"
#include "Lib/ImGUI/imgui.h"
#include "Lib/ImGUI/imgui_impl_dx9.h"
#include "Lib/ImGUI/imgui_impl_win32.h"
#include "Lib/ImGUI/RobotoFont.cpp"
#include "Lib/Ini/SimpleIni.h"
#include "Lib/Json/json.hpp"
#include "Lib/Midi/RtMidi.h"

// Crowd Control
#include "CC/CCEffect.hpp"
#include "CC/CCEffectList.hpp"

// Wwise
#include "Wwise/Exports.hpp"
#include "Wwise/Logging.hpp"
#include "Wwise/MemoryMgr.hpp"
#include "Wwise/Monitor.hpp"
#include "Wwise/MusicEngine.hpp"
#include "Wwise/PluginRegistration.hpp"
#include "Wwise/Root.hpp"
#include "Wwise/SoundEngine.hpp"
#include "Wwise/Types.hpp"

// Internal Dependencies
#include "D3D/D3D.hpp"
#include "D3D/D3DHooks.hpp"
#include "Log.hpp"
#include "MemUtil.hpp"
#include "Offsets.hpp"
#include "Settings.hpp"
#include "Resolution.h"
#include "RSColor.h"
#include "Tuning.h"
#include "VersioningStruct.h"
#include "SongTuning.hpp"
#include "GameState.hpp"
#include "SongTimer.hpp"

/// <param name="valueToCheckIfInsideArray"> - Input</param>
/// <param name="vec"> - Is input in list | VECTOR? (NULLABLE)</param>
template <typename T, std::ranges::input_range R>
	requires std::convertible_to<T, std::ranges::range_value_t<R>>
constexpr bool Contains(const T& value, const R& range);

bool Contains(std::string_view text, std::string_view key);