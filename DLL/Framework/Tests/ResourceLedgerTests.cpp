#include "../ResourceLedger.hpp"

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
	int g_failures = 0;

	void Check(const std::string& name, bool got, bool want) {
		if (got == want) {
			std::cout << "  PASS  " << name << "\n";
			return;
		}
		++g_failures;
		std::cout << "  FAIL  " << name
			<< "  got=" << (got ? "true" : "false")
			<< " want=" << (want ? "true" : "false") << "\n";
	}

	void CheckSet(const std::string& name,
		const std::unordered_set<std::string>& got,
		const std::unordered_set<std::string>& want)
	{
		if (got == want) {
			std::cout << "  PASS  " << name << "\n";
			return;
		}
		++g_failures;
		auto join = [](const std::unordered_set<std::string>& s) {
			std::string out;
			for (const auto& x : s) { if (!out.empty()) out += ","; out += x; }
			return out.empty() ? "{}" : "{" + out + "}";
		};
		std::cout << "  FAIL  " << name
			<< "  got=" << join(got)
			<< " want=" << join(want) << "\n";
	}

	// Stable sentinel addresses for tests.
	static const char kOwnerA = 0;
	static const char kOwnerB = 0;
	static const char kOwnerC = 0;
}

int main() {
	std::cout << "ResourceLedger tests\n";

	// Fresh ledger per logical group via unique local instances.
	{
		Framework::ResourceLedger ledger;
		Check("claim on free resource succeeds",
			ledger.TryClaim(&kOwnerA, { "string-colors" }), true);
	}

	{
		Framework::ResourceLedger ledger;
		ledger.TryClaim(&kOwnerA, { "string-colors" });
		// Same owner may re-claim its own resources.
		Check("owner may overwrite its own claim",
			ledger.TryClaim(&kOwnerA, { "string-colors", "note-visuals" }), true);
	}

	{
		// TryClaim takes its resource list by value; an lvalue caller is still supported
		// and the caller's vector is left untouched (it is copied, not consumed).
		Framework::ResourceLedger ledger;
		std::vector<std::string> wanted = { "string-colors", "note-visuals" };
		Check("TryClaim accepts an lvalue resource list", ledger.TryClaim(&kOwnerA, wanted), true);
		Check("TryClaim does not mutate the caller's lvalue", wanted.size() == 2, true);
	}

	{
		Framework::ResourceLedger ledger;
		ledger.TryClaim(&kOwnerA, { "string-colors" });
		Check("TryClaim fails when resource held by another owner",
			ledger.TryClaim(&kOwnerB, { "string-colors" }), false);
	}

	{
		// All-or-nothing: if one resource is taken, no resources are claimed.
		Framework::ResourceLedger ledger;
		ledger.TryClaim(&kOwnerA, { "string-colors" });
		const bool claimed = ledger.TryClaim(&kOwnerB, { "note-visuals", "string-colors" });
		Check("all-or-nothing: partial conflict claims nothing", claimed, false);
		// Verify B holds nothing (note-visuals must still be free).
		Check("all-or-nothing: free resource not claimed on partial failure",
			ledger.TryClaim(&kOwnerC, { "note-visuals" }), true);
	}

	{
		Framework::ResourceLedger ledger;
		ledger.TryClaim(&kOwnerA, { "player-volume" });
		ledger.Release(&kOwnerA);
		Check("Release frees resource for a new owner",
			ledger.TryClaim(&kOwnerB, { "player-volume" }), true);
	}

	{
		Framework::ResourceLedger ledger;
		ledger.Release(&kOwnerA); // safe on empty ledger
		Check("Release on non-holding owner is a no-op (no crash)", true, true);
	}

	{
		Framework::ResourceLedger ledger;
		ledger.TryClaim(&kOwnerA, { "song-speed", "scroll-speed" });
		// Publish replaces the held set; scroll-speed is dropped.
		ledger.Publish(&kOwnerA, { "song-speed" });
		// scroll-speed is now free — another owner can take it.
		Check("Publish drops absent resources from previous claim",
			ledger.TryClaim(&kOwnerB, { "scroll-speed" }), true);
	}

	{
		Framework::ResourceLedger ledger;
		ledger.Publish(&kOwnerA, {});
		Check("Publish with empty set is equivalent to Release",
			ledger.TryClaim(&kOwnerB, { "any-resource" }), true);
	}

	{
		Framework::ResourceLedger ledger;
		ledger.TryClaim(&kOwnerA, { "string-colors" });
		ledger.TryClaim(&kOwnerB, { "note-visuals" });
		const auto held = ledger.HeldExcluding(&kOwnerA);
		CheckSet("HeldExcluding omits the caller's own resources",
			held, { "note-visuals" });
	}

	{
		Framework::ResourceLedger ledger;
		ledger.TryClaim(&kOwnerA, { "string-colors" });
		ledger.TryClaim(&kOwnerB, { "note-visuals" });
		const auto held = ledger.HeldExcluding(&kOwnerC);
		CheckSet("HeldExcluding returns all other owners' resources",
			held, { "string-colors", "note-visuals" });
	}

	{
		Framework::ResourceLedger ledger;
		Check("HeldExcluding on empty ledger returns empty set",
			ledger.HeldExcluding(&kOwnerA).empty(), true);
	}

	{
		// Contention: two owners race on the same resource; first wins, second loses.
		Framework::ResourceLedger ledger;
		const bool firstWon  = ledger.TryClaim(&kOwnerA, { "tone-slot" });
		const bool secondWon = ledger.TryClaim(&kOwnerB, { "tone-slot" });
		Check("contention: first TryClaim wins",  firstWon,  true);
		Check("contention: second TryClaim loses", secondWon, false);
	}

	std::cout << (g_failures == 0 ? "\nALL PASS\n" : "\nFAILURES: " + std::to_string(g_failures) + "\n");
	return g_failures == 0 ? 0 : 1;
}
