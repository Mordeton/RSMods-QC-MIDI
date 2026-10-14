# RSMods QC on Bazzite / native Steam

The Linux branch runs the Windows game and the latest Windows QC mod through
Proton. It includes the owner's newer Windows working tree supplied on
2026-10-14: F10 panel, arrangement-specific mappings, preview fixes, improved
transpose handling and scene retry logic. The Windows source is integrated with
the current RSMods framework; `QCAutomationMod` remains the single tick/key owner.

## Current measured support

On the owner's Bazzite/KDE machine, native Steam with Proton Experimental
`experimental-11.0-20261008-x86_64`, RSASIO 0.7.5 and PipeASIO 1.10.0:

- Actual Rocksmith playback reaches the QC headphones, confirmed by the owner.
- The Rocksmith tuner responds to the guitar on QC Input 1, confirmed by the owner.
- RSASIO streams at 48 kHz, with two output/eight input channels and 256 frames.
  This is the configured buffer, not a measured end-to-end latency claim.
- The game uses PipeASIO's experimental 32-bit frontend with Proton's new WoW64.
- The new F10 panel appeared, and an idle MIDI recall to the configured QC preset
  was observed in Cortex Control. This does not verify in-song automation yet.
- A crash just after tuning was traced to an unchecked song-timer float read
  inside RSMods (RVA `0xA7C3C` in build `d43e35b`). The CDLC loader was present
  and applied its startup patches. The timer now guards intermediate and final
  reads, returns zero when unavailable and reads fresh values on the next call.
  The owner confirmed Supersonic CDLC song playback, headphone audio and note
  detection with build `c2f3a74`; the later test used the clean USB 5 route.
- That run exposed an inaccessible base timer alongside an advancing readable
  alternate timer. The guarded reader now also tries the alternate when the
  base is unavailable during a song. QC transpose can use the exact playing
  arrangement's retained tuning metadata if live tuning memory is unavailable;
  it requires a fresh matching identity and six offsets within ±24 semitones. These
  follow-up corrections still need game verification with transpose enabled.
- Sustained song play, the full F10 workflow, automated recalls/transpose, reconnect,
  reboot and other Proton versions still need their own runtime checks.

Original Cortex Control is a separate private compatibility installation. It is
not bundled with this project. Cloud support is deferred.

## Install on this supported setup

Prerequisites: the owned game already launches through native Steam; RSASIO's
`RS_ASIO.dll`, `avrt.dll` and `RS_ASIO.ini` are present; `umu-run`, `pactl`, Python
3.12+ and `desktop-file-validate` are available. Flatpak Steam is not yet qualified.
No system audio driver replacement or system Python changes are needed.

Close the game and RSMods GUI. Obtain the portable ZIP and its SHA256SUMS from
the `codex/linux-release` branch's successful **Development builds** workflow:

```sh
python3 scripts/linux.py install --archive /absolute/path/to/portable.zip --sha256 HASH_FROM_SHA256SUMS
python3 scripts/linux_setup.py audio --input-channel 0 --buffer 256
python3 scripts/linux_setup.py steam-options --close-steam
```

Audio setup backs up the prefix and settings, downloads a pinned checksum-verified
PipeASIO release, registers it in the actual game prefix, and checks its driver,
native library and PipeWire connection before changing game audio settings.
Steam setup closes Steam normally, modifies only Rocksmith's launch options,
preserves existing `%command%` arguments and reopens Steam.

Application menu entries: **Rocksmith with Quad Cortex** and **RSMods QC Settings**.
Normal Steam Play also uses the same wrapper. QC device names are rediscovered at
launch. A changed Proton version requires running audio setup again to check it.
The GUI's QC releases button opens this fork's releases rather than installing
an upstream build that would remove the QC features.

`--input-channel` is zero-based. `0` means USB capture channel 1, which was
verified for the owner's current dry Input 1 route. A pitch-shifted detection
feed on QC USB 3 requires channel `2`, after verifying that preset's routing.
Raw input does not prove transpose detection. Keep the QC's modelled guitar
monitoring direct to headphones and avoid a second guitar monitor in Rocksmith.
The owner's current presets route the clean signal through Transpose to **USB 5**;
the installed game therefore uses `--input-channel 4`. This route was visually
checked in Cortex Control and the owner confirmed in-song note detection. The
earlier tuner test used raw USB 1. Transpose accuracy remains a separate test.

Song-specific mappings live in `Rocksmith2014/RSMods/QCOverrides.json`; copying
source code alone does not migrate them from Windows. Preserve/import the user's
actual file when available. Tone names such as a song title may hit the Clean
fallback, so use the F10 arrangement mapping for those tones. Do not change the
global classifier merely to fix one song. Keep spaces around ` = ` in RSMods.ini:
the existing settings GUI recognizes those exact identifiers.

Audio setup sets `DisableHidraw=1` only in Rocksmith's Wine prefix, recording its
previous value for restoration. This prevents Wine's HID backend from opening
the QC editor interface while Cortex Control is running. Linux USB audio/MIDI
drivers remain attached. Raw-HID game-controller features in this prefix are not
qualified. A per-device `PROTON_DISABLE_HIDRAW` filter was insufficient because
the backend still opened the node; it is no longer used by this setup.

## Update, diagnostics and rollback

```sh
~/.local/bin/rsmods-qc install --archive /absolute/path/to/new-portable.zip --sha256 HASH_FROM_SHA256SUMS
~/.local/bin/rsmods-qc doctor
~/.local/bin/rsmods-qc rollback --backup /absolute/path/printed/by/install
~/.local/bin/rsmods-qc restore-audio
~/.local/bin/rsmods-qc restore-hid
~/.local/bin/rsmods-qc restore-steam-options --close-steam
```

An update verifies the ZIP before writing, preserves personal INIs, QC mappings
and custom mods, verifies copied binaries, and saves the previous files and
hashes in `Rocksmith2014/RSMods-backups`. Rollback verifies both the installed
files and saved originals first; it refuses to overwrite later user changes.
Older backup manifests without hashes require manual review.

`restore-audio` restores the two original audio INIs while retaining the replaced
files for recovery. `restore-steam-options` restores only the previous Rocksmith
launch options and refuses to discard subsequent edits. Driver files remain
inactive in the prefix. Keep the prefix backup for full disaster recovery; do
not automatically replace a prefix after newer saves/settings have accumulated.

Tools/configuration/backups live under `~/.local/share/rsmods-qc`. To remove the
menu entries after restoring launch options, remove `rocksmith-qc.desktop` and
`rsmods-qc-proton.desktop` from `~/.local/share/applications`; an earlier RSMods
desktop file is retained as `rsmods-qc-proton.desktop.before` in the tools folder.
Personal game files and backups should remain in place.

Ordinary launches do not enable Proton traces. Use `rsmods-qc debug on` to enable
focused exception logging for subsequent launches, then `rsmods-qc debug off`
after the check. Logs go to `~/.local/share/rsmods-qc/logs`; copy a useful trace
before relaunching because the next run overwrites it. Never upload personal
logs or saves. CI retains matching debug symbols as a separate artifact.

## Smallest remaining acceptance sequence

1. Launch the fresh mod, verify its version and F10 panel under Proton.
2. With saved QC work and explicit test scope, verify one preset and one scene
   change, then in-song tone transitions and pause/restart/seek behavior.
3. Verify the preset's pitch-shifted clean USB feed and transpose note detection.
4. Play a complete song, then test app coexistence, reconnect and a reboot.

Sources: [PipeASIO](https://github.com/M0n7y5/pipeasio),
[RSASIO](https://github.com/mdias/rs_asio),
[Quad Cortex manual](https://neuraldsp.com/manual/quad-cortex).
