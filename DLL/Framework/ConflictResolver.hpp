#pragma once

// Requested candidates are considered by priority then ID. Winners reserve their resources
// immediately, so conflicts are checked against winners rather than all requested candidates.

#include <algorithm>
#include <numeric>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace Framework::Resolver {

	struct Candidate {
		std::string_view id;
		int priority = 0;
		bool requested = false;
		const std::vector<std::string>* exclusiveResources = nullptr;

		const std::vector<std::string>& Resources() const {
			static const std::vector<std::string> Empty;
			return exclusiveResources ? *exclusiveResources : Empty;
		}
	};

	// One win/lose flag per candidate, positionally indexed alongside `candidates`.
	using SelectionMask = std::vector<char>;

	[[nodiscard]] inline SelectionMask Resolve(const std::vector<Candidate>& candidates, const std::unordered_set<std::string>& initiallyReserved = {}) {
		SelectionMask selected(candidates.size(), 0);

		std::vector<std::size_t> priorityOrder(candidates.size());
		std::iota(priorityOrder.begin(), priorityOrder.end(), std::size_t{ 0 });
		std::ranges::sort(priorityOrder, [&](std::size_t leftIndex, std::size_t rightIndex) {
			const Candidate& left = candidates[leftIndex];
			const Candidate& right = candidates[rightIndex];

			if (left.priority != right.priority) {
				return left.priority > right.priority;
			}

			return left.id < right.id; // Stable across TUs, unlike registration order.
		});

		std::unordered_set<std::string> reservedResources(initiallyReserved);
		for (std::size_t candidateIndex : priorityOrder) {
			const Candidate& candidate = candidates[candidateIndex];
			if (!candidate.requested) continue;

			const auto& resources = candidate.Resources();
			const bool blocked = std::ranges::any_of(resources,
				[&](const std::string& resource) {
					return reservedResources.contains(resource);
				});
			if (blocked) continue;

			selected[candidateIndex] = 1;
			reservedResources.insert(resources.begin(), resources.end());
		}

		return selected;
	}
}
