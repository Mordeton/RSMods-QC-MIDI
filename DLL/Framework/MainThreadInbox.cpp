#include "MainThreadInbox.hpp"

#include <utility>

namespace Framework {
	void MainThreadInbox::PostKeyEvent(KeyEvent event) {
		{
			std::lock_guard<std::mutex> lock(mutex);
			keyEvents.push_back(event);
		}

		// No wake flag: the waiter's predicate sees the non-empty queue, and DrainKeyEvents
		// on the next dispatch pass clears it, so this cannot spin.
		changed.notify_one();
	}

	void MainThreadInbox::PostSettingsUpdate(std::function<void()> apply) {
		{
			std::lock_guard<std::mutex> lock(mutex);
			settingsUpdates.push_back(std::move(apply));

			// One-shot: settings are drained on the maintenance tick, not this pass, so a
			// queue-based predicate would spin. The flag wakes the waiter exactly once.
			wakeRequested = true;
		}

		changed.notify_one();
	}

	void MainThreadInbox::Wake() {
		{
			std::lock_guard<std::mutex> lock(mutex);
			wakeRequested = true;
		}

		changed.notify_one();
	}

	void MainThreadInbox::WaitUntil(std::chrono::steady_clock::time_point deadline) {
		std::unique_lock<std::mutex> lock(mutex);

		changed.wait_until(lock, deadline, [&] {
			return wakeRequested || !keyEvents.empty();
		});
		wakeRequested = false;
	}

	std::deque<KeyEvent> MainThreadInbox::DrainKeyEvents() {
		std::lock_guard<std::mutex> lock(mutex);

		std::deque<KeyEvent> drained;
		drained.swap(keyEvents);

		return drained;
	}

	std::vector<std::function<void()>> MainThreadInbox::DrainSettings() {
		std::lock_guard<std::mutex> lock(mutex);

		std::vector<std::function<void()>> drained;
		drained.swap(settingsUpdates);

		return drained;
	}

	MainThreadInbox& Inbox() {
		static MainThreadInbox instance;

		return instance;
	}
}
