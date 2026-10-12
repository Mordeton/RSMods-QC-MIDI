# Development guide

This is the user's Quad Cortex fork of RSMods for Rocksmith 2014 Remastered.
The Linux target is the Windows game and Windows mod running through Steam Proton
on Bazzite. The injected DLL is still a **32-bit Windows DLL**; do not replace it
with a native Linux shared library.

## Source and architecture

- `origin`: `Mordeton/RSMods-QC-MIDI`; `upstream`: `Lovrom8/RSMods`.
- QC release source: `feature/qc-public-release-1`, tag `qc-v1.0.0-beta3`.
- Linux development: `codex/bazzite-development`, integrating upstream `develop`.
- `DLL/Mods/QCAutomationMod.*`: integration with upstream's main-thread framework.
- `DLL/Mods/QCAutomation.*`: preset classification, bank/program dispatch, transpose scenes.
- `DLL/Mods/ToneSource*`: live context, authored tone timeline, PSARC/SNG loading.
- `DLL/Mods/Midi.*`: Windows WinMM/RtMidi transport, exposed through Wine to ALSA MIDI.
- `GUI/`: .NET Framework 4.7.2 WinForms configuration application.
- `Installer/`: Windows installer embedding the DLL and GUI resources.
- `scripts/linux.py`: Steam discovery, diagnostics, verified portable installation, GUI launch.

## Validation and builds

Run `python3 scripts/test-linux.py` and
`python3 -m unittest discover -s scripts/tests -v` on Linux. These check portable
logic and installation tooling, not the complete injected DLL or hardware behavior.
Full builds require Windows, Visual Studio 2022 C++ v143, the Windows SDK and
.NET Framework 4.7.2 targeting pack. Use `build-rsmods-deliverable.ps1`; the
GitHub `Development builds` workflow provides Windows compilation and artifacts.

## Local game safety

Read `docs/bazzite-development.md` and the ignored `.local/installation.json`
before touching the installed game. Discover paths and USB devices instead of
hardcoding card numbers. Close the game and GUI before replacing binaries; use
the installer script and retain its backup manifest. Never commit game binaries,
Steam configuration, profile saves, personal tokens, or local diagnostic dumps.
Keep CDLC loaders and the user's audio configuration intact unless the task
requires a specific change. Installing RSMods does not configure RSASIO.

Validate DLL loading, Wine-visible MIDI, actual MIDI delivery and audio channels
separately. Do not report hardware success from configuration files or host
enumeration alone. Coordinate audio service restarts with the user because they
interrupt audio.
