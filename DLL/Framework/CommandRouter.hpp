#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "CommandTypes.hpp"

namespace Framework {
	class IMod;

	// Answers "is this mod's owner available for a binding that requires this Availability level?"
	// The registry (the single source of lifecycle truth) supplies this at dispatch time; the router
	// no longer caches any per-mod lifecycle state, it only tracks bindings it has faulted itself.
	using OwnerAvailabilityFn = std::function<bool(const IMod*, Availability)>;

	class CommandRouter {
	public:
		CommandRouter();
		~CommandRouter();

		CommandRouter(const CommandRouter&) = delete;
		CommandRouter& operator=(const CommandRouter&) = delete;

		void BindSetting(const IMod* mod, std::string keySetting, KeyEdge edge,
			Availability availability, KeyAction action,
			KeyPredicate predicate = {}, std::string logMessage = {});
		void BindKey(const IMod* mod, std::string name, std::uint32_t virtualKey,
			KeyEdge edge, Availability availability, KeyAction action,
			KeyPredicate predicate = {}, std::string logMessage = {});
		void SetKeyResolver(KeyResolver resolver);

		void RemoveMod(const IMod* mod);

		// Key events are supplied by the caller (drained from the MainThreadInbox), not queued
		// inside the router. Startup input is discarded here when gameLoaded is false. Owner
		// availability is resolved through `ownerAvailable` rather than cached in the router.
		void DispatchPending(ModContext& context, const std::deque<KeyEvent>& events, bool gameLoaded,
			const OwnerAvailabilityFn& ownerAvailable);

		[[nodiscard]] std::vector<const IMod*> TakeFaultedMods();
		void RefreshDiagnostics();

	private:
		struct Impl;
		std::unique_ptr<Impl> impl;
	};

	CommandRouter& Commands();
}
