#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Framework {
	class ResourceLedger {
	public:
		// Atomic test-and-hold. Returns false and claims nothing if any resource in `resources`
		// is already held by an owner other than `owner`. Used by CC effects on Start.
		bool TryClaim(const void* owner, std::vector<std::string> resources);

		// Drop everything `owner` holds. Safe if it holds nothing. Used by CC on Stop.
		void Release(const void* owner);

		// Overwrite `owner`'s entire held set in one shot. Previously-held resources not in
		// `resources` are freed. Empty `resources` is equivalent to Release. Used by the registry
		// each tick.
		void Publish(const void* owner, std::vector<std::string> resources);

		// Every resource held by anyone other than `owner`. The registry seeds its greedy resolve
		// from this so it honours live CC claims.
		[[nodiscard]] std::unordered_set<std::string> HeldExcluding(const void* owner) const;

	private:
		mutable std::mutex mutex;
		std::unordered_map<const void*, std::vector<std::string>> claims;
	};

	ResourceLedger& Ledger();
}
