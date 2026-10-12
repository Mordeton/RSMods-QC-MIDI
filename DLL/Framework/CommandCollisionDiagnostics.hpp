#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "CommandTypes.hpp"

namespace Framework::Detail {
	enum class CommandBindingKind {
		Setting,
		FixedKey,
	};

	struct ResolvedCommandBinding {
		KeyEdge edge;
		std::uint32_t virtualKey;
		CommandBindingKind kind;
		std::string name;
	};

	class CommandCollisionDiagnostics {
	public:
		void Refresh(std::vector<ResolvedCommandBinding> bindings);

	private:
		struct Collision {
			KeyEdge edge;
			std::uint32_t virtualKey;
			CommandBindingKind kind;
			std::vector<std::string> labels;

			bool operator==(const Collision& other) const;
		};

		static std::vector<Collision> CollectCollisions(
			std::vector<ResolvedCommandBinding> bindings);
		static void LogNewCollisions(
			std::span<const Collision> collisions,
			std::span<const Collision> previousCollisions);

		std::mutex mutex;
		std::vector<Collision> previousCollisions;
	};
}
