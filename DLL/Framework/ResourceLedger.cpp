#include "ResourceLedger.hpp"

#include <algorithm>

namespace Framework {
	bool ResourceLedger::TryClaim(const void* owner, std::vector<std::string> resources) {
		std::lock_guard lock(mutex);

		const bool blocked = std::ranges::any_of(claims, [&](const auto& entry) {
			const auto& [holder, held] = entry;
			if (holder == owner) return false;
			return std::ranges::any_of(resources, [&](const std::string& wanted) {
				return std::ranges::find(held, wanted) != held.end();
			});
		});

		if (blocked) return false;
		claims.insert_or_assign(owner, std::move(resources));

		return true;
	}

	void ResourceLedger::Release(const void* owner) {
		std::lock_guard lock(mutex);

		claims.erase(owner);
	}

	void ResourceLedger::Publish(const void* owner, std::vector<std::string> resources) {
		std::lock_guard lock(mutex);

		if (resources.empty()) {
			claims.erase(owner);
		} else {
			claims.insert_or_assign(owner, std::move(resources));
		}
	}

	std::unordered_set<std::string> ResourceLedger::HeldExcluding(const void* owner) const {
		std::lock_guard lock(mutex);
		std::unordered_set<std::string> result;

		for (const auto& [holder, held] : claims) {
			if (holder == owner) continue;
			result.insert(held.begin(), held.end());
		}
		
		return result;
	}

	ResourceLedger& Ledger() {
		static ResourceLedger instance;
		return instance;
	}
}
