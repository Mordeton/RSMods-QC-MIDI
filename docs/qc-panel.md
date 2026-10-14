# QC panel and arrangement overrides

Press F10 to expand the in-game Quad Cortex panel. The compact display shows the authored tone, effective role/address, mode, scene and Windows MIDI result. Windows send accepted does not mean the QC acknowledged or audibly applied the command.

In song menus choose the explicitly labelled Preview arrangement. Every tone has a dropdown: Automatic, Clean, Overdrive, Distortion, Mod or Solo, each showing its configured QC address. Selections save immediately; controls show a pending state until the atomic save completes. Failed saves retain the previous mapping and display an error. Reset arrangement mappings requires inline confirmation and immediately restores Automatic. During playback dropdowns are disabled; pause to change them. Previewing or editing a preview never selects a QC preset. The playing arrangement replaces the preview choice during playback.

The QC automatic target looks one song second ahead. The panel separately shows Song tone now and highlights the QC-selected tone, so they can differ around transitions. Existing polling means roughly 0.75–1 second advance at normal speed, potentially later during blocking work. At 50% practice speed the lookahead is about two real seconds. Paused game time holds the target; paused seeks and restarts recompute it. Manual and idle commands are not advanced. Full diagnostics and IDs are under Details; unavailable timelines show a prominent base-tone fallback warning.

Mappings are stored in the game directory at RSMods/QCOverrides.json, version 1. Identity comprises song key, normalized persistent arrangement ID (arrangement ID fallback), and exact authored tone name. Only roles are persisted; their addresses still come from the global settings. Manual slots 2â€“4 take precedence, slot 1 returns to automatic processing, and idle behavior is unchanged. Automatic removes the local mapping. Unknown versions and malformed files retain the last good mappings and cannot be saved over. Concurrent external changes reject a save.

The RSMods QC Automation arrangement-aware Song Preview reads these mappings. Its generic tone-name preview remains global-only. The game editor is the supported writer.

The runtime thread owns mapping state and queued commands. Render receives a mutex-protected value snapshot and never loads archives, writes files or sends MIDI. A serialized asset loader runs asynchronous preview work; obsolete song results are discarded. Per-tone save/reset commands carry a context generation and request ID. Active mappings publish only after persistence succeeds. The retained tuner preview is labelled stale and cannot be edited.

Input is captured both at WndProc and DirectInput keyboard/mouse polling. Held editor presses remain suppressed until release. Other DirectInput device types are untouched. Editing is unavailable if capture initialization fails. DX9 resources continue to use the existing ImGui device reset lifecycle.

Run tools/build-qc.ps1 for isolated Release/Win32 DLL and Release/x64 GUI builds, native regression checks and shared C++/C# override fixtures. Build manifests record source fingerprints, artifact hashes and GUI dependency hashes. Deployment events are disabled. Never use the combined legacy deployment script for this feature.

Hardware acceptance after installation: E-standard scene A; a down-tuned song's expected scene; an Overdrive mapping; transpose off/on; manual slots and return-to-auto; restart persistence; corrected MIDI channel; return to idle. Verify the actual QC screen and sound separately from the panel's Windows result.

Windows asset discovery excludes Mac _m.psarc archives; SNG lookup accepts only songs/bin/generic entries. The asset regression executable optionally accepts the installed game path to verify CarsJust and prior known arrangements without starting the game or sending MIDI.
