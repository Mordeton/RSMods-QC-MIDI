#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>

#include "CommandTypes.hpp"

namespace Framework {
	// The single main-thread work queue. Foreign threads post here (WndProc key input, the
	// GUI/Twitch/CrowdControl/render-thread settings writes, and the window-close signal), and
	// MainThread blocks in WaitUntil, then drains the two queues at their own cadences: key
	// events every command-dispatch pass, settings closures on the 250 ms maintenance tick.
	//
	// Wake semantics mirror those cadences. Key events wake on a non-empty queue, because they
	// are drained every pass and so the predicate self-clears. Settings and the explicit close
	// signal set a one-shot flag instead, because settings are NOT drained until the next tick
	// and a queue-based predicate would spin until then.
	class MainThreadInbox {
	public:
		MainThreadInbox() = default;

		MainThreadInbox(const MainThreadInbox&) = delete;
		MainThreadInbox& operator=(const MainThreadInbox&) = delete;

		// Producers (any thread).
		void PostKeyEvent(KeyEvent event);
		void PostSettingsUpdate(std::function<void()> apply);
		void Wake();

		// Consumer (MainThread).
		void WaitUntil(std::chrono::steady_clock::time_point deadline);
		[[nodiscard]] std::deque<KeyEvent> DrainKeyEvents();
		[[nodiscard]] std::vector<std::function<void()>> DrainSettings();

	private:
		std::mutex mutex;
		std::condition_variable changed;
		bool wakeRequested = false;
		std::deque<KeyEvent> keyEvents;
		std::vector<std::function<void()>> settingsUpdates;
	};

	MainThreadInbox& Inbox();
}
