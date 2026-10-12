#include <iomanip>

#include "../CommandRouter.hpp"
#include "../IMod.hpp"
#include "../ModContext.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace Framework;

namespace {
	int failures = 0;

	class TestMod final : public IMod {
	public:
		std::string_view Id() const override { return "test-mod"; }
	};

	void Expect(bool condition, const std::string& name) {
		std::cout << (condition ? "  PASS  " : "  FAIL  ") << name << '\n';
		if (!condition) ++failures;
	}

	void UseKeys(CommandRouter& router, std::map<std::string, unsigned int> keys) {
		router.SetKeyResolver([keys = std::move(keys)](std::string_view name) {
			const auto it = keys.find(std::string(name));
			return it == keys.end() ? 0u : it->second;
		});
	}

	KeyEvent Event(unsigned int key, KeyEdge edge = KeyEdge::Up, bool control = false, bool repeat = false) {
		KeyEvent event;
		event.virtualKey = key;
		event.edge = edge;
		event.control = control;
		event.repeat = repeat;
		return event;
	}

	// Stands in for the registry as the source of owner availability, mirroring its lifecycle rules:
	// Active satisfies both binding kinds; Initialized satisfies only Initialized bindings; anything
	// else (unregistered / gone) satisfies neither. Passed to DispatchPending as the OwnerAvailabilityFn.
	enum class Life { Gone, Initialized, Active };

	struct Owners {
		std::map<const IMod*, Life> life;

		bool operator()(const IMod* mod, Availability required) const {
			const auto it = life.find(mod);
			const Life state = it == life.end() ? Life::Gone : it->second;
			if (state == Life::Active) return true;
			if (state == Life::Initialized) return required == Availability::Initialized;
			return false;
		}
	};
}

static void Test_FirstPhysicalMatchDoesNotFallThrough() {
	CommandRouter router;
	ModContext context;
	TestMod modA;
	TestMod modB;
	int callsA = 0;
	int callsB = 0;
	UseKeys(router, { { "ACommand", 42 }, { "BCommand", 42 } });

	router.BindSetting(&modA, "ACommand", KeyEdge::Up, Availability::Active,
		[&](ModContext&, const KeyEvent&) { ++callsA; },
		[](const ModContext&, const KeyEvent&) { return false; });
	router.BindSetting(&modB, "BCommand", KeyEdge::Up, Availability::Active,
		[&](ModContext&, const KeyEvent&) { ++callsB; });
	Owners owner;
	owner.life[&modA] = Life::Active;
	owner.life[&modB] = Life::Active;
	router.DispatchPending(context, { Event(42) }, true, owner);

	Expect(callsA == 0 && callsB == 0, "false predicate on name-first collision does not fall through");
}

static void Test_UnavailableOwnerVanishesFromCollisionOrder() {
	CommandRouter router;
	ModContext context;
	TestMod modA;
	TestMod modB;
	int callsB = 0;
	UseKeys(router, { { "ACommand", 42 }, { "BCommand", 42 } });

	router.BindSetting(&modA, "ACommand", KeyEdge::Up, Availability::Active,
		[](ModContext&, const KeyEvent&) {});
	router.BindSetting(&modB, "BCommand", KeyEdge::Up, Availability::Active,
		[&](ModContext&, const KeyEvent&) { ++callsB; });
	Owners owner;
	owner.life[&modA] = Life::Initialized; // initialized-only: its Active binding is unavailable
	owner.life[&modB] = Life::Active;
	router.DispatchPending(context, { Event(42) }, true, owner);

	Expect(callsB == 1, "inactive mod is removed before physical collision resolution");
}

static void Test_ModifierSnapshotAndFifoArePreserved() {
	CommandRouter router;
	ModContext context;
	TestMod mod;
	std::vector<std::string> received;
	UseKeys(router, { { "Held", 9 } });

	router.BindSetting(&mod, "Held", KeyEdge::Down, Availability::Active,
		[&](ModContext&, const KeyEvent& event) {
			received.push_back(std::string(event.control ? "ctrl" : "plain") + (event.repeat ? ":repeat" : ":first"));
		});
	router.BindSetting(&mod, "Held", KeyEdge::Up, Availability::Active,
		[&](ModContext&, const KeyEvent&) { received.push_back("up"); });
	Owners owner;
	owner.life[&mod] = Life::Active;

	router.DispatchPending(context, {
		Event(9, KeyEdge::Down, true, false),
		Event(9, KeyEdge::Down, false, true),
		Event(9, KeyEdge::Up),
	}, true, owner);

	Expect(received == std::vector<std::string>{ "ctrl:first", "plain:repeat", "up" },
		"modifier snapshots, repeats, and down/up FIFO survive deferred delivery");
}

static void Test_VolumeBindingsUseNormalCollisionOrdering() {
	CommandRouter router;
	ModContext context;
	TestMod mod;
	std::vector<std::string> received;
	UseKeys(router, { { "Primary", 7 }, { "MasterVolumeKey", 7 }, { "SongVolumeKey", 7 } });

	router.BindSetting(&mod, "Primary", KeyEdge::Down, Availability::Active,
		[&](ModContext&, const KeyEvent&) { received.push_back("primary"); });
	router.BindSetting(&mod, "MasterVolumeKey", KeyEdge::Down, Availability::Active,
		[&](ModContext&, const KeyEvent&) { received.push_back("master"); });
	router.BindSetting(&mod, "SongVolumeKey", KeyEdge::Down, Availability::Active,
		[&](ModContext&, const KeyEvent&) { received.push_back("song"); });
	Owners owner;
	owner.life[&mod] = Life::Active;
	router.DispatchPending(context, { Event(7, KeyEdge::Down) }, true, owner);

	Expect(received == std::vector<std::string>{ "master" },
		"volume bindings share the normal setting-name collision order");
}

static void Test_OwnerScopedFixedKeyRunsAfterSettingCommands() {
	CommandRouter router;
	ModContext context;
	TestMod settingMod;
	TestMod fixedMod;
	std::vector<std::string> received;
	UseKeys(router, { { "SettingCommand", 46 } });

	router.BindSetting(&settingMod, "SettingCommand", KeyEdge::Up,
		Availability::Active,
		[&](ModContext&, const KeyEvent&) { received.push_back("setting"); });
	router.BindKey(&fixedMod, "FixedCommand", 46, KeyEdge::Up,
		Availability::Active,
		[&](ModContext&, const KeyEvent&) { received.push_back("fixed"); },
		[](const ModContext&, const KeyEvent&) { return true; });
	Owners owner;
	owner.life[&settingMod] = Life::Active;
	owner.life[&fixedMod] = Life::Active;
	router.DispatchPending(context, { Event(46) }, true, owner);

	Expect(received == std::vector<std::string>{ "setting", "fixed" },
		"mod-scoped fixed key preserves the post-setting host shortcut pass");

	received.clear();
	owner.life[&fixedMod] = Life::Initialized; // deactivation: its Active binding is no longer available
	router.DispatchPending(context, { Event(46) }, true, owner);
	Expect(received == std::vector<std::string>{ "setting" },
		"active gating removes a fixed-key mod during suppression");
}

static void Test_AvailabilityPolicies() {
	CommandRouter router;
	ModContext context;
	TestMod mod;
	int activeCalls = 0;
	int initializedCalls = 0;
	UseKeys(router, { { "Active", 1 }, { "Initialized", 2 } });

	router.BindSetting(&mod, "Active", KeyEdge::Up, Availability::Active,
		[&](ModContext&, const KeyEvent&) { ++activeCalls; });
	router.BindSetting(&mod, "Initialized", KeyEdge::Up, Availability::Initialized,
		[&](ModContext&, const KeyEvent&) { ++initializedCalls; });
	Owners owner;
	owner.life[&mod] = Life::Initialized;

	router.DispatchPending(context, { Event(1), Event(2) }, true, owner);
	Expect(activeCalls == 0 && initializedCalls == 1, "initialized policy works while active policy is unavailable");

	owner.life[&mod] = Life::Active;
	router.DispatchPending(context, { Event(1) }, true, owner);
	owner.life[&mod] = Life::Initialized; // leaving Active drops the active-only binding immediately.
	router.DispatchPending(context, { Event(1) }, true, owner);
	Expect(activeCalls == 1, "active binding is unavailable as soon as deactivation begins");
}

static void Test_LoadingEventsAreDropped() {
	CommandRouter router;
	ModContext context;
	TestMod mod;
	int calls = 0;
	UseKeys(router, { { "LoadedOnly", 3 } });
	router.BindSetting(&mod, "LoadedOnly", KeyEdge::Up, Availability::Active,
		[&](ModContext&, const KeyEvent&) { ++calls; });
	Owners owner;
	owner.life[&mod] = Life::Active;

	router.DispatchPending(context, { Event(3) }, false, owner); // discarded: game not loaded yet
	router.DispatchPending(context, {}, true, owner);            // nothing retained to replay
	Expect(calls == 0, "startup input is discarded rather than replayed after GameLoaded");
}

static void Test_CollisionDiagnosticsOnlyLogChanges() {
	CommandRouter router;
	UseKeys(router, { { "First", 8 }, { "Second", 8 } });
	router.BindSetting(nullptr, "First", KeyEdge::Up, Availability::Active,
		[](ModContext&, const KeyEvent&) {});
	router.BindSetting(nullptr, "Second", KeyEdge::Up, Availability::Active,
		[](ModContext&, const KeyEvent&) {});

	std::ostringstream captured;
	auto* previousBuffer = std::cerr.rdbuf(captured.rdbuf());
	router.RefreshDiagnostics();
	const std::string firstRefresh = captured.str();
	captured.str({});
	captured.clear();
	router.RefreshDiagnostics();
	const std::string secondRefresh = captured.str();
	std::cerr.rdbuf(previousBuffer);

	Expect(firstRefresh.find("key collision") != std::string::npos &&
		firstRefresh.find("has first priority when available in the settings pass") != std::string::npos &&
		secondRefresh.empty(),
		"same-pass collisions describe availability-sensitive priority and only log once");
}

static void Test_CollisionDiagnosticsDoNotMergeDispatchPasses() {
	CommandRouter router;
	UseKeys(router, { { "SettingCommand", 8 } });
	router.BindSetting(nullptr, "SettingCommand", KeyEdge::Up, Availability::Active,
		[](ModContext&, const KeyEvent&) {});
	router.BindKey(nullptr, "FixedCommand", 8, KeyEdge::Up, Availability::Active,
		[](ModContext&, const KeyEvent&) {});

	std::ostringstream captured;
	auto* previousBuffer = std::cerr.rdbuf(captured.rdbuf());
	router.RefreshDiagnostics();
	std::cerr.rdbuf(previousBuffer);

	Expect(captured.str().find("key collision") == std::string::npos,
		"bindings in independent dispatch passes are not reported as a collision");
}

static void Test_CommandFaultStopsRemainingBatch() {
	CommandRouter router;
	ModContext context;
	TestMod mod;
	int calls = 0;
	UseKeys(router, { { "Fault", 4 } });
	router.BindSetting(&mod, "Fault", KeyEdge::Up, Availability::Active,
		[&](ModContext&, const KeyEvent&) {
			++calls;
			throw std::runtime_error("boom");
		});
	Owners owner;
	owner.life[&mod] = Life::Active;
	router.DispatchPending(context, { Event(4), Event(4) }, true, owner);
	const auto faultedMods = router.TakeFaultedMods();

	Expect(calls == 1 && faultedMods.size() == 1 && faultedMods.front() == &mod,
		"throwing command faults its mod and suppresses its remaining queued events");
}

int main() {
	std::cout << "CommandRouter tests\n";
	Test_FirstPhysicalMatchDoesNotFallThrough();
	Test_UnavailableOwnerVanishesFromCollisionOrder();
	Test_ModifierSnapshotAndFifoArePreserved();
	Test_VolumeBindingsUseNormalCollisionOrdering();
	Test_OwnerScopedFixedKeyRunsAfterSettingCommands();
	Test_AvailabilityPolicies();
	Test_LoadingEventsAreDropped();
	Test_CollisionDiagnosticsOnlyLogChanges();
	Test_CollisionDiagnosticsDoNotMergeDispatchPasses();
	Test_CommandFaultStopsRemainingBatch();

	std::cout << (failures == 0 ? "\nALL PASS\n" : "\nFAILURES: " + std::to_string(failures) + "\n");
	return failures == 0 ? 0 : 1;
}
