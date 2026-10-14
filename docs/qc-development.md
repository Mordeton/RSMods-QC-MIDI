# Windows QC development reference

Imported from the owner's latest Windows working tree on 2026-10-14. For this
Linux/Proton branch, follow AGENTS.md and bazzite-development.md; Windows paths
and local agent configuration below describe the original Windows workspace.
Build the Windows DLL in the repository's Windows CI workflow.

This setup is specific to the QC product. RSMods hosts it; RockSniffer provides reference code. Keep player behavior, source evidence and hardware results connected.

## Start here

Open this repository or the outer `rocksmith-qc-dev` workspace in Codex. `AGENTS.md` holds the stable project facts. Three local skills provide repeatable workflows:

| Request | Skill | Result |
| --- | --- | --- |
| Diagnose/fix a wrong tone, scene or MIDI state | `$qc-debug` | Reproduction, targeted fix and regression evidence |
| Explore or develop a new player-facing capability | `$qc-feature` | Feasibility or an implemented increment, according to the request |
| Verify/build, or install when requested | `$qc-build-test` | Fresh logs and a staged DLL; backup and rollback for installation |

Examples: “Use $qc-debug to investigate why scene B disappears after a tone change.” “Use $qc-feature to compare per-song overrides with better keyword matching; don't implement yet.” “Use $qc-build-test to build and update my game; close and reopen it normally.”

The primary agent implements and integrates. `qc_explorer` traces code, `qc_reviewer` independently checks changes, and `qc_researcher` checks protocol/API feasibility. Use a bounded helper when it can run alongside useful work, not a team for every edit. Models and reasoning effort inherit the user's choice. Read-only role instructions do not replace the host's permission policy.

## Commands and evidence

Run from the RSMods repository:

```powershell
# Fast offline logic checks
powershell -ExecutionPolicy Bypass -File tests/run-qc-regression.ps1
# Checks plus isolated Release/Win32 DLL; never installs
powershell -ExecutionPolicy Bypass -File tools/build-qc.ps1
# Same recorded workflow without compiling the full DLL
powershell -ExecutionPolicy Bypass -File tools/build-qc.ps1 -RegressionOnly
```

The staged helper stores a unique run under ignored `artifacts/qc/`, including logs, source fingerprints, commands, revision, architecture validation and SHA-256. Keep a relevant manifest path in the handoff rather than committing build output. Use [the handoff template](templates/qc-handoff.md) for substantive work. A failed run is never a verified deployable artifact.

Fingerprints cover repository DLL sources, tests, tools and the solution; they are not a hermetic toolchain lock. If compiler/SDK versions, external MSBuild properties, or new build inputs change, rebuild rather than treating an earlier manifest as current.

The regression harness runs the real QC runtime against substituted inputs. It covers E-standard validity, enable transitions, destination/scene synchronization, failed-send retries and target selection. It does not validate the keyboard hook, actual Windows memory reads, USB delivery, QC loading latency or audio. Add targeted coverage at those boundaries when changing them; do not equate mocks with hardware tests.

This machine's game is `D:\SteamLibrary\steamapps\common\Rocksmith2014`. Install only within an authorized task. Preserve configuration and use backup + verified DLL-only replacement unless the requested feature also requires GUI delivery. Old `dist/` artifacts and the combined deployment scripts are not evidence of current source or a safe default update. The September 19 four-fix deployment is recorded in the outer workspace's `workspace/scratch/qc-four-fixes/deployment.json`; hardware acceptance remains unconfirmed until the user reports it.

## Feature exploration queue

These are candidates, not approved implementation work. Confirm their current state before planning:

- **QC diagnostics:** show song tone, resolved target, desired scene and MIDI send outcome; test/resync controls would make hardware troubleshooting easier.
- **Bass filtering:** investigate arrangement identity instead of matching `bass` in a tone name, and define whether transpose follows the same filter.
- **Explicit physical tuning:** replace implicit E/Drop-D assumptions with a player-selected baseline and validate all six string offsets.
- **MIDI connection lifecycle:** evaluate persistent QC-owned transport and reconnect handling, preserving shared RSMods callers.
- **Per-song overrides:** explore a reversible mapping UI and stable arrangement identifiers before adding storage.

For each candidate, capture player benefit, current evidence, affected boundaries, smallest experiment, acceptance scenarios and hardware prerequisites. Prefer official Neural DSP documentation for QC commands and firmware assumptions. Microsoft Learn MCP is configured locally for Windows/C++ references; if unavailable, use official web documentation. GitHub integration can assist issue/CI work when connected. Do not add tools without a missing capability.

## Local discovery and maintenance

Canonical skills live in `.agents/skills/`; roles and project settings live in `.codex/`. On this machine the outer workspace links `.agents` and `.codex` to these repository directories, so they are discoverable from either launch location without maintaining duplicate definitions. Its pre-existing empty `.codex` file is preserved in `workspace/legacy-codex-placeholder`. When cloning elsewhere, opening `RSMods/` needs no junctions. Project settings may require an already-trusted project; do not silently change global trust or permission settings.

New tasks load project instructions; skills may refresh automatically, but start a new task/restart Codex if new roles or skills do not appear. Keep these files short and update them after an observed workflow failure, rather than adding generic rules. Global model, sandbox, approval and MCP settings are not modified by this setup.

Local validation on 2026-09-19: the outer `D:\dev\rocksmith-qc-dev` workspace loaded the project agent configuration, and all three skills were enabled from both that location and `RSMods/`. The nested repository was not separately trusted, so opening it directly did not activate its project configuration. Continue using the outer workspace, or explicitly trust the nested project through Codex if you choose to switch. Agent files were syntax-validated; the independent review used their written instructions, not a newly loaded named-agent invocation in this existing session.

## Basis for this setup

Official sources were checked on 2026-09-19:

- [AGENTS.md discovery and scope](https://learn.chatgpt.com/docs/agent-configuration/agents-md): stable repository context in a small instruction file.
- [Local skills](https://learn.chatgpt.com/docs/build-skills): focused workflows in `.agents/skills`, with details loaded when needed.
- [Custom subagents](https://learn.chatgpt.com/docs/agent-configuration/subagents): bounded roles, inherited settings and explicit orchestration.

Community advice was used as experience reports, not configuration specifications: [start with a small repeatable workflow](https://www.reddit.com/r/codex/comments/1v1i57b/what_are_your_workflow_and_skills/) and [record completion evidence instead of relying only on reminders](https://www.reddit.com/r/codex/comments/1wdx5jx/have_agent_respect_the_agentsmd/). The executable staged build and handoff are the project-specific application of that advice; no third-party prompt packs were installed.
