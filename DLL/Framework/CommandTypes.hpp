#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

namespace Framework {
	struct ModContext;

	enum class KeyEdge {
		Down,
		Up,
	};

	enum class Availability {
		// Strictly ModState::Active; unavailable the moment the mod leaves Active.
		Active,
		// Available after successful initialization until fault/shutdown, even while disabled or suppressed.
		// Never use this for a command that mutates a resource returned by ClaimsExclusive().
		Initialized,
	};

	struct KeyEvent {
		std::uint32_t virtualKey = 0;
		KeyEdge edge = KeyEdge::Up;
		bool control = false;
		bool shift = false;
		bool alt = false;
		bool repeat = false;
	};

	using KeyPredicate = std::function<bool(const ModContext&, const KeyEvent&)>;
	using KeyAction = std::function<void(ModContext&, const KeyEvent&)>;
	using KeyResolver = std::function<unsigned int(std::string_view)>;
}
