#include "../MainThreadInbox.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace Framework;

namespace {
	int failures = 0;

	void Expect(bool condition, const std::string& name) {
		std::cout << (condition ? "  PASS  " : "  FAIL  ") << name << '\n';
		if (!condition) ++failures;
	}

	KeyEvent Key(unsigned int virtualKey) {
		KeyEvent event;
		event.virtualKey = virtualKey;
		return event;
	}
}

// A posted key event must wake the waiter promptly rather than blocking to the deadline.
static void Test_KeyEventWakesWaiter() {
	MainThreadInbox inbox;
	std::atomic<bool> returned{ false };
	const auto started = std::chrono::steady_clock::now();

	std::thread waiter([&] {
		inbox.WaitUntil(started + std::chrono::seconds(2));
		returned.store(true, std::memory_order_release);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	inbox.PostKeyEvent(Key(5));
	waiter.join();

	const auto elapsed = std::chrono::steady_clock::now() - started;
	Expect(returned.load(std::memory_order_acquire) && elapsed < std::chrono::seconds(1),
		"posted key event wakes the waiter without waiting for the deadline");
}

// A posted settings update must also wake the waiter (one-shot flag).
static void Test_SettingsUpdateWakesWaiter() {
	MainThreadInbox inbox;
	std::atomic<bool> returned{ false };
	const auto started = std::chrono::steady_clock::now();

	std::thread waiter([&] {
		inbox.WaitUntil(started + std::chrono::seconds(2));
		returned.store(true, std::memory_order_release);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	inbox.PostSettingsUpdate([] {});
	waiter.join();

	const auto elapsed = std::chrono::steady_clock::now() - started;
	Expect(returned.load(std::memory_order_acquire) && elapsed < std::chrono::seconds(1),
		"posted settings update wakes the waiter");
}

// An explicit Wake() (the window-close signal) must return the waiter.
static void Test_ExplicitWakeReturnsWaiter() {
	MainThreadInbox inbox;
	std::atomic<bool> returned{ false };
	const auto started = std::chrono::steady_clock::now();

	std::thread waiter([&] {
		inbox.WaitUntil(started + std::chrono::seconds(2));
		returned.store(true, std::memory_order_release);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	inbox.Wake();
	waiter.join();

	const auto elapsed = std::chrono::steady_clock::now() - started;
	Expect(returned.load(std::memory_order_acquire) && elapsed < std::chrono::seconds(1),
		"explicit Wake returns the waiter promptly");
}

// Both queues drain in FIFO order and are independently emptied.
static void Test_DrainsAreFifoAndIndependent() {
	MainThreadInbox inbox;

	inbox.PostKeyEvent(Key(1));
	inbox.PostKeyEvent(Key(2));
	inbox.PostKeyEvent(Key(3));

	std::vector<int> settingsOrder;
	inbox.PostSettingsUpdate([&] { settingsOrder.push_back(10); });
	inbox.PostSettingsUpdate([&] { settingsOrder.push_back(20); });

	const auto keys = inbox.DrainKeyEvents();
	const bool keysFifo = keys.size() == 3 &&
		keys[0].virtualKey == 1 && keys[1].virtualKey == 2 && keys[2].virtualKey == 3;

	// Draining key events must not disturb the settings queue.
	auto settings = inbox.DrainSettings();
	for (auto& apply : settings) apply();
	const bool settingsFifo = settingsOrder == std::vector<int>{ 10, 20 };

	// Both queues are now empty.
	const bool cleared = inbox.DrainKeyEvents().empty() && inbox.DrainSettings().empty();

	Expect(keysFifo && settingsFifo && cleared,
		"key and settings queues drain FIFO and independently, then clear");
}

// A key event that self-clears (drained each pass) must not keep the predicate hot: after
// draining, the waiter blocks to the deadline instead of spinning.
static void Test_DrainedQueueDoesNotKeepWaking() {
	MainThreadInbox inbox;
	inbox.PostKeyEvent(Key(7));
	(void)inbox.DrainKeyEvents(); // consume it, as a dispatch pass would

	const auto started = std::chrono::steady_clock::now();
	inbox.WaitUntil(started + std::chrono::milliseconds(60));
	const auto elapsed = std::chrono::steady_clock::now() - started;

	Expect(elapsed >= std::chrono::milliseconds(50),
		"a drained key queue lets the waiter block to its deadline (no spin)");
}

// The settings wake is one-shot. Settings are NOT drained between the wake and the tick, so the
// queue stays non-empty across waits: the first wait consumes the flag and returns, but the second
// must still block to its deadline. This is the guarantee that keeps `!settingsUpdates.empty()`
// out of the predicate; putting it there would pass every other test yet spin the main loop.
static void Test_SettingsWakeIsOneShot() {
	MainThreadInbox inbox;
	inbox.PostSettingsUpdate([] {});

	// First wait: the one-shot flag is already set, so it returns promptly.
	const auto firstStart = std::chrono::steady_clock::now();
	inbox.WaitUntil(firstStart + std::chrono::seconds(2));
	const auto firstElapsed = std::chrono::steady_clock::now() - firstStart;

	// The settings closure is deliberately left un-drained. The second wait must block to its
	// deadline rather than spin on the still-non-empty settings queue.
	const auto secondStart = std::chrono::steady_clock::now();
	inbox.WaitUntil(secondStart + std::chrono::milliseconds(60));
	const auto secondElapsed = std::chrono::steady_clock::now() - secondStart;

	Expect(firstElapsed < std::chrono::seconds(1) && secondElapsed >= std::chrono::milliseconds(50),
		"settings wake is one-shot: consumed once, then the next wait blocks without draining");
}

int main() {
	std::cout << "MainThreadInbox tests\n";
	Test_KeyEventWakesWaiter();
	Test_SettingsUpdateWakesWaiter();
	Test_ExplicitWakeReturnsWaiter();
	Test_DrainsAreFifoAndIndependent();
	Test_DrainedQueueDoesNotKeepWaking();
	Test_SettingsWakeIsOneShot();

	std::cout << (failures == 0 ? "\nALL PASS\n" : "\nFAILURES: " + std::to_string(failures) + "\n");
	return failures == 0 ? 0 : 1;
}
