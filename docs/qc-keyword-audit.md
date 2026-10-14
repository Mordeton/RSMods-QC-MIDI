# QC keyword audit — 2026-09-24

Scanned 696 Windows/base song archives. Read 3,675 arrangement-tone rows and 2,236 distinct authored names across 940 song keys. The typed reference reader could not parse rs1compatibilitydlc_p.psarc due to a nullable numeric field; raw manifest extraction recovered its 688 rows. Black Hole Sun_p.psarc could not be opened (reported array size error), so its tones are excluded. No screen/game control or MIDI was used.

## Changes

Added only corpus-supported aliases:

- Distortion: `_hg` (Iris_hg).
- Clean: `acou` (sticktoyourguns_acou).
- Mod: `lofi,bitcrush,flang,vibrato,_vib,_talk,pitch,_rot`.

These are name-based inferences, not audible validation. The five global QC roles cannot reproduce each named effect individually. Kept Solo and Overdrive keywords unchanged: extra generic words such as rhythm, heavy, metal, intro or guitar do not establish which sound should be used. Avoided plain `talk`, which would steal acdcmoneytalks_lead; `_talk` matches the observed effect suffix.

Source defaults updated in the native classifier, both native settings defaults and both GUI defaults. Installed DistKeywords/CleanKeywords/ModKeywords updated after a verified backup. No binaries were replaced. Preset addresses, custom priorities, IgnoreBass, transpose and per-song overrides were preserved. Running Rocksmith needs a normal restart to load the new lists; it was not restarted or tested.

With code-default priorities: 19 names change role, plus one Clean fallback becomes an explicit acoustic match. With this installation's OD > Mod > Dist > Clean > Solo priorities: 20 names change role, plus that explicit Clean match. The additional change is MuseStock_distlofi from Distortion to Mod, following the user's existing Mod priority.

## Actual configured-priority changes

| Authored tone | Before | After | Trigger |
|---|---|---|---|
| `AlterRise_lofi` | Clean (fallback) | Mod | `lofi` |
| `BatCountry_lofi` | Clean (fallback) | Mod | `lofi` |
| `BottomsUp_lofi` | Clean (fallback) | Mod | `lofi` |
| `BrandNew_rot` | Clean (fallback) | Mod | `_rot` |
| `Brick By Boring Brick Pitch` | Clean (fallback) | Mod | `pitch` |
| `Earphone_lofi` | Clean (fallback) | Mod | `lofi` |
| `EightBall_bitcrush` | Clean (fallback) | Mod | `bitcrush` |
| `GoneAway_rotary` | Clean (fallback) | Mod | `_rot` |
| `IncLove_rot` | Clean (fallback) | Mod | `_rot` |
| `Iris_hg` | Clean (fallback) | Dist | `_hg` |
| `Juicebox_rotary` | Clean (fallback) | Mod | `_rot` |
| `Knights_vib` | Clean (fallback) | Mod | `_vib` |
| `LegendaryChild_vib` | Clean (fallback) | Mod | `_vib` |
| `MuseStock_distlofi` | Dist (dist) | Mod | `lofi` |
| `Paralyzer_lofi` | Clean (fallback) | Mod | `lofi` |
| `RageKnow_lofi` | Clean (fallback) | Mod | `lofi` |
| `SatchBoogie_flang` | Clean (fallback) | Mod | `flang` |
| `Vibrato` | Clean (fallback) | Mod | `vibrato` |
| `itsmylife_talk` | Clean (fallback) | Mod | `_talk` |
| `motlkick_talk` | Clean (fallback) | Mod | `_talk` |
| `sticktoyourguns_acou` | Clean (fallback) | Clean | `acou` |

## Known limits retained

The existing unrestricted contains matching can misclassify song-name prefixes: Today_solo matches od, Bodysnatchers_acoustic matches od, Disarm_acoustic matches dis, and CliffsOfDover_lead matches over. Expanding aliases does not solve these. A separate boundary/suffix-aware classifier design with migration for custom keywords is needed; use per-song overrides in the meantime. No role-priority changes or speculative artist-based matches were introduced here.

## Evidence and recovery

Native regression exercises new names, negatives, OD/MOD guard, case handling, custom priorities/lists and existing override/manual behavior. All 2,236 audited names were checked against the production classifier using the generated corpus fixture. A source parity check verifies all five copies of the C++/C# default lists. Audit data and before/after tables are in artifacts/tone-audit (ignored; song assets are not committed).

Settings backup: `D:\dev\rocksmith-qc-dev\workspace\scratch\qc-keywords-backup-20260924-184445`. Run `rollback-keywords.ps1` there to restore only these three lists, preserving other settings and refusing to overwrite later keyword edits.

Verified stage: `artifacts/qc/20260924-184401-ecc4e5` (native regression, asset regression, GUI shared fixtures, Release/Win32 DLL and Release/x64 GUI all passed). Staged DLL SHA-256: `3BEA815EBED2296DAE645FF3F826E9D1FC0FA180F122C252836625E71AF4C2BA`. Staged binaries were not installed: the existing installed classifier already accepts the new configured keywords. Independent read-only review found no blocking addition collisions in the scanned corpus.
