# F10 usability and early tone update — 2026-09-20

Implemented immediate role dropdown saves, confirmed arrangement reset, pending/error UI, separate actual-song/QC target status, diagnostics under Details, and a fixed one-song-second automation lookahead. Timing uses existing polling (normally about 0.75–1 second early at full speed, not a real-time guarantee). Paused-time holding relies on the game timer freezing; visual and hardware behavior have not been rechecked in this update.

## Decoder finding and fix

Both carsjust_m.psarc and carsjust_p.psarc are installed. The loader selected the Mac archive first and used the Windows SNG key through its filename fallback. Independent read-only decoding reproduced both screenshot numbers exactly: 992589182 decrypted-size interpretation and 446917878 raw-size interpretation. Windows assets decode to 466296 bytes. Windows enumeration now excludes _m.psarc, and SNG lookup/fallback only accepts songs/bin/generic entries. AES and allocation limits are unchanged.

The production asset harness verified CarsJust Lead 68D12AFF3F4C77ED71B13799FB6092A2 from carsjust_p.psarc: 119.83s solo, 136.06s drive, 215.36s synth. It also rejected Mac fallback and successfully loaded TheStrokes1251 and IcedEarth-1776 arrangements. No game assets were copied into tracked tests.

## Automated evidence

Final stage: `artifacts/qc/20260920-182627-77ec67`.

- `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-qc.ps1`: passed native automation/override/input-helper regressions, production decoder synthetic/malformed fixtures, Release/Win32 DLL, Release/x64 GUI compatibility build and shared C# fixtures.
- `tests/qc-assets.exe D:/SteamLibrary/steamapps/common/Rocksmith2014`: passed installed archive checks, recorded in installed-assets.log.
- Added tests cover early boundaries, close events, unshifted actual tone, repeated paused-time lookup, seek/restart lookup, single-tone save isolation, failed/stale saves and Automatic removal. Existing tests retain manual precedence, MIDI destination changes, forced scene retries and idle behavior.
- Independent read-only review found no blocker in save ownership, input path preservation, early/actual separation or platform filtering.

The automated harness does not instantiate the render callback. Pending-control appearance, mouse/keyboard operation and audible QC dropout timing require user testing. No screen was acquired. Earlier live-test observations below describe the prior build only.

## Installation and rollback

Only xinput1_3.dll was replaced after observing both apps closed. Backup copies and installed hashes were verified; settings and QCOverrides.json were preserved. Installed GUI hash remains unchanged.

DLL SHA-256: `4C1253E2DC94B56E9316AE8140080714A46C6C1073B61DE8358D2D813BA6AB4E`.

Backup: `D:\dev\rocksmith-qc-dev\workspace\scratch\qc-panel-backup-20260920-182807`.

After normally closing Rocksmith and RSMods, roll back this update with:

```powershell
powershell -ExecutionPolicy Bypass -File "D:\dev\rocksmith-qc-dev\workspace\scratch\qc-panel-backup-20260920-182807\rollback.ps1"
```

User checks: F10 dropdown selection and immediate persistence; pause and reset confirmation; Cars solo near 1:59.83 with QC selection around 1:58.83 (polling tolerance applies); return to drive around 2:16.06; manual 3 then 1; return to idle. Confirm sound and scene on QC separately from Windows acceptance. UI input leakage remains a release blocker if encountered during user testing.

---

# QC panel verification â€” 2026-09-19

Implemented: Release QC HUD/F10 panel, asynchronous arrangement readiness, per-arrangement authored-tone role overrides, pause/menu editor, matching read-only GUI preview, and staged DLL/GUI packaging.

## Automated checks

The native harness uses production QC target/transpose decisions, override store, preview-request generation helper, input ownership/visible-queue helpers, and tone timeline resolver. MIDI is recorded by a stub; the harness never launches Rocksmith or contacts the QC. Coverage includes E-standard, unavailable tuning retry, B/H shifts, transpose enable transitions, device/channel/CC resynchronization, forced-scene retry/deduplication, override identity isolation/reset/write failure/invalid version/last-good retention, global role changes, manual precedence/idle, obsolete A-B-A metadata requests, buffered PEEK release retention, held input suppression/focus reset, timeline ambiguity/seek and restart/arrangement tuning recomputation. C# consumes the same override fixtures.

The harness does not instantiate the complete DX9 render loop or actual archive-worker/thread scheduling. Live observations below supplement the production helper tests.

## Live observations

Pre-final Release tests: F10 expands/collapses independently of debug menu. Enter and arrows stayed captured while expanded; Escape closed panel without leaving the game menu. Mouse initially leaked through polling; DirectInput capture was added and the arrangement dropdown and role editor then operated without moving the selected song. Ctrl+A did not visibly invoke a game action. Numeric 3 while paused/expanded did not select manual slot 3. Switching to the GUI discarded an unsaved reset draft and collapsed the game panel on return.

1251 Lead: selected preview and saved temporary 1251_solo -> Overdrive. Browsing/editing retained Idle 32F/A. Playback resolved the actual Lead arrangement and sent 32B followed by A; panel reported Windows send accepted. Paused editing was available. GUI Song Preview showed automatic Solo/32E and effective Overdrive/32B for exactly that Lead arrangement. Settings hashes remained unchanged. The temporary mapping is removed through Reset/Save before handoff.

The DirectInput interception additionally received independent review of PEEK, buffered close transitions, release ownership, focus loss and other device isolation. Held input cases are covered with the production helpers; sustained physical key holds were not reproduced by the computer-control API.

### Final installed build retest

After restarting with the final DLL, F10, Down, Enter and Escape were tested in the main menu/browser: expanded-panel input did not navigate the game, and normal navigation resumed after closing the panel. Mouse Edit, Reset arrangement and Save removed the temporary 1251 Lead override. The preview returned to Solo/32E while the active idle target remained 32F/A. On-disk QCOverrides.json was verified as version 1 with an empty entries array.

1776 playback replaced the preview Rhythm identity with the actual live Lead arrangement B655B689. Iced_Earth_Dist resolved to Distortion/32C, tuning -1 on all six strings, scene B, Windows send accepted. Expanded playback was read-only; pausing exposed Edit tone mappings. Numeric 3 and Ctrl+A while paused/expanded caused no visible game action or manual selection. After closing the panel, 3 selected manual 32G/B and 1 restored automatic 32C/B. Clicking Exit returned to idle 32F/A. No input leakage was observed in these tested sequences. Sustained physical holds and exhaustive alt-tab/device-reset sequences remain outside this final live retest.

A cosmetic observation: the prior "Mappings saved" message remains visible after changing songs; it does not affect the displayed effective mapping or MIDI target.

## Final build and deployment

Verified stage: artifacts/qc/20260919-194359-2269fd. Native regression and shared C# fixtures passed during this stage; Release/Win32 DLL and Release/x64 GUI were built with deployment events disabled. The installed hashes were checked again after live testing:

- xinput1_3.dll: `840AB84B382CA974968831E618377ACC9E6A033BA11C9C77101F960EAE08AAD2`
- RSMods/RSMods.exe: `C417C59F8F1985F3AAAD150E008CB6F71E9E06DDA913CB0DAAB8BC1C15409FB6`

RSMods.ini and GUI_Settings.ini still match the preserved pre-test hashes. Final replacement backup and deployment records: D:\dev\rocksmith-qc-dev\workspace\scratch\qc-panel-backup-20260919-194514. Rocksmith was left open at the 1776 song screen with idle synchronization accepted; the installed RSMods GUI process is running. Use the original-binaries rollback below to undo the complete feature update.

## Hardware confirmation

No guitar performance or audible QC result has been confirmed. Windows MIDI acceptance and a loaded DLL are software evidence only. User should verify: E-standard A, down-tuned scene, desired Overdrive sound, transpose off/on, manual 2-4 then 1, persistence after restart, MIDI channel correction, idle 32F/A.

## Rollback to original binaries

The complete pre-feature backup is `D:\dev\rocksmith-qc-dev\workspace\scratch\qc-panel-originals-20260919-194009`. It contains the original DLL and GUI/config plus settings copies and hashes. Close Rocksmith and RSMods normally, then run:

```powershell
powershell -ExecutionPolicy Bypass -File "D:\dev\rocksmith-qc-dev\workspace\scratch\qc-panel-originals-20260919-194009\rollback.ps1"
```

Rollback restores verified original binaries and preserves settings and saved mappings.
