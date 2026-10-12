# Bazzite development and Proton setup

The development target is Rocksmith 2014 Remastered's Windows executable running
under Steam Proton. RSMods remains a Windows x86 DLL named `xinput1_3.dll`. Its
WinMM MIDI calls need to reach the Quad Cortex through Wine's MIDI support.
RSASIO is a separate audio layer and needs a working **32-bit Wine-compatible
ASIO driver**; the Quad Cortex Windows driver cannot simply be installed as a
Linux driver.

## Source baseline

On 2026-10-12 the newest QC source was `origin/feature/qc-public-release-1` at
`4fa3395` (2026-04-25), also the source of `qc-v1.0.0-beta3`. The default fork
branch `develop` did not contain QC automation. The newest upstream `develop`
was `072a670` (2026-09-29). The `codex/bazzite-development` branch combines these
sources, adapts the QC loop and tone-slot keys to the current upstream framework,
and provides Linux tooling and Windows build automation.

The upstream integration is substantial. Passing portable tests or compilation
does not establish that all mods work at runtime. The Linux port uses Proton;
a full native Linux GUI or a native replacement for the game is outside this setup.

## Workspace map

| Component | Responsibility |
| --- | --- |
| `DLL/Framework` | Mod lifecycle, input routing, scheduling, conflict resolution |
| `DLL/Mods/QCAutomationMod.*` | Main-thread integration of the QC logic |
| `DLL/Mods/QCAutomation.*` | Tone roles, preset recalls and transpose scene CCs |
| `DLL/Mods/ToneSource*` | Song/arrangement context and authored tone timeline |
| `DLL/Mods/Midi.*` | WinMM/RtMidi output transport |
| `GUI/UI.cs`, `ReadSettings.cs`, `WriteSettings.cs` | QC settings UI and INI persistence |
| `Installer` | Packaging and Windows installation |

## Development commands

From the repository root:

```sh
python3 scripts/test-linux.py
python3 -m unittest discover -s scripts/tests -v
python3 scripts/linux.py doctor
python3 scripts/linux.py midi
python3 scripts/linux.py gui
```

The first command compiles and runs six upstream portable test suites and the
QC authored-tone timeline tests with a C++20 compiler. It does not build the DLL.
No system package layering or reboot is required for these tools on this machine.

The full build requires Windows with Visual Studio 2022's Desktop development
with C++ workload, v143, a Windows SDK and the .NET Framework 4.7.2 targeting pack:

```powershell
./DLL/Framework/Tests/BuildAndRun.ps1
./build-rsmods-deliverable.ps1 -Config Release -Platform Win32 -VersionTag linux-dev
```

The GitHub `Development builds` workflow produces a portable ZIP and checksums
as the `rsmods-qc-development` artifact. It does not publish a release. It can
also be dispatched manually on the development branch. GUI dependencies are
resolved from checked-in `GUI/Lib`; packaging seeds those dependencies before
building the installer, without deploying into a Windows Steam directory.

## Installing and loading the mod

Close Rocksmith and the RSMods GUI. Download a portable ZIP and its published or
build-generated `SHA256SUMS.txt`, then run:

```sh
python3 scripts/linux.py install --archive /absolute/path/to/portable.zip --sha256 HASH_FROM_SHA256SUMS
```

Steam libraries are discovered automatically; use `--game-dir /absolute/path`
before `install`, `doctor` or `gui` if necessary. The script understands the
backslash-separated filenames in the Windows-generated ZIP, rejects unsafe
archive paths, verifies SHA256 before writing, and backs up existing destination
files in `Rocksmith2014/RSMods-backups/<UTC timestamp>/`. Its manifest records
which files existed before the install. It also copies the existing Rocksmith
INI, RSMods INI and CDLC loader into the backup's `reference` directory.

In Steam > Rocksmith > Properties > General > Launch Options, use:

```text
WINEDLLOVERRIDES="xinput1_3=n,b" %command%
```

Preserve any existing launch options and DLL overrides. If adding RSASIO later,
its `avrt` override may be needed as well; do not activate it before an audio
driver and routing are ready. A per-executable Wine registry override can also
select the RSMods DLL without editing Steam's configuration while Steam is open:

```sh
protontricks -c 'wine reg add "HKCU\Software\Wine\AppDefaults\Rocksmith2014.exe\DllOverrides" /v xinput1_3 /t REG_SZ /d native,builtin /f && wineserver -w' 221680
```

Use `scripts/linux.py gui` to run the GUI in the same prefix, rather than a
separate Wine installation. The GUI may need its game/save paths set to the
corresponding `Z:\...` paths. It also creates default settings and profile backups
on first run. Its upstream updater currently targets upstream RSMods; replacing
this fork with an upstream release removes the QC extensions, so use development
artifacts for updates until a fork-aware updater is implemented.

For rollback, close the game, restore the files listed as `existed: true` from
the backup's `files` directory, and remove the installed files listed as
`existed: false`. Restore the previous per-executable registry override from the
installation record if one was changed. Keep user-created settings and profile
backups. The portable installer does not modify `cache.psarc`, song DLC or saves.

## Machine observations on 2026-10-12

| Item | Observed state |
| --- | --- |
| OS | Bazzite 44, Kinoite, NVIDIA open variant |
| Game | `/var/home/Mordeton/.local/share/Steam/steamapps/common/Rocksmith2014` |
| Steam app | `221680`, Remastered, build `16576867` |
| Prefix | `/var/home/Mordeton/.local/share/Steam/steamapps/compatdata/221680/pfx` |
| Proton | Experimental, Wine 11.0 |
| Default output | Schiit Modi Multibit |
| Input | Quad Cortex, currently exposed as an 8-channel surround source |
| QC USB format | 8 input and 8 output channels, 48 kHz, S32_LE container with 24 valid bits |
| QC MIDI | ALSA `Quad Cortex MIDI 1` |
| Wine MIDI | The 32-bit WinMM probe also enumerated `Quad Cortex MIDI 1` |
| Initial RSASIO | Not installed |

Card and node numbers are transient. Linux seeing the MIDI port does not prove
that Wine enumerates it; the separate WinMM probe verified enumeration through
this Proton prefix. An actual QC preset recall still needs testing. The existing
Rocksmith configuration initially had `ExclusiveMode=0` and
`Win32UltraLowLatencyMode=0`; RSASIO's normal configuration uses both set to `1`,
but those changes belong with the ASIO driver setup and testing.

## Next integration checks

1. Confirm the installed development DLL writes `RSMods_debug.txt` when launched
   normally through Steam, with the expected game-version checksum.
2. Confirm WinMM exposes a QC MIDI output in the actual Rocksmith prefix. Select
   that exact name; it may differ from the Windows guide's device name.
3. Verify one MIDI preset recall with the configured QC channel and bank map.
4. Choose the audio topology: QC for both playback/capture, or QC capture with
   the existing Schiit output. Verify the actual dry-guitar USB channel before
   choosing a zero-based RSASIO input channel.
5. Install/test a 32-bit WineASIO bridge over PipeWire JACK, or assess another
   documented 32-bit-compatible bridge. Check compatibility with the selected
   Proton version before changing its files.
6. Test 48 kHz capture, playback, note detection and latency. Only then enable
   automatic QC tones and transpose scenes and run `qc-phase1-smoke-test.md`.

References: [upstream Proton instructions](https://github.com/Lovrom8/RSMods#running-under-wine--proton),
[RSASIO Linux guidance](https://github.com/mdias/rs_asio#using-rs-asio-on-linux),
[QC beta3](https://github.com/Mordeton/RSMods-QC-MIDI/releases/tag/qc-v1.0.0-beta3).
