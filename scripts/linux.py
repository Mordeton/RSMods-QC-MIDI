#!/usr/bin/env python3
"""Rocksmith/Proton discovery, backed-up portable installs, and GUI launching."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import shutil
import stat
import subprocess
import zipfile

APP_ID = "221680"
REPO = "Mordeton/RSMods-QC-MIDI"


def steam_roots():
    candidates = [Path.home() / p for p in (
        ".local/share/Steam", ".steam/steam",
        ".var/app/com.valvesoftware.Steam/.local/share/Steam",
    )]
    if os.environ.get("STEAM_DIR"):
        candidates.insert(0, Path(os.environ["STEAM_DIR"]).expanduser())
    return list(dict.fromkeys(p.resolve() for p in candidates if p.is_dir()))


def find_game(explicit=None):
    if explicit:
        game = Path(explicit).expanduser().resolve()
        if not (game / "Rocksmith2014.exe").is_file():
            raise ValueError(f"Rocksmith2014.exe missing in {game}")
        return game
    found = []
    for root in steam_roots():
        libraries = [root]
        vdf = root / "steamapps/libraryfolders.vdf"
        if vdf.exists():
            libraries += [Path(p.replace("\\\\", "\\")) for p in
                          re.findall(r'"path"\s+"([^"]+)"', vdf.read_text())]
        for library in libraries:
            manifest = library / f"steamapps/appmanifest_{APP_ID}.acf"
            if not manifest.exists():
                continue
            match = re.search(r'"installdir"\s+"([^"]+)"', manifest.read_text())
            if match:
                game = (library / "steamapps/common" / match[1]).resolve()
                if (game / "Rocksmith2014.exe").is_file() and game not in found:
                    found.append(game)
    if len(found) != 1:
        raise ValueError(f"Found {len(found)} Rocksmith installations; specify --game-dir")
    return found[0]


def running_game():
    for process in Path("/proc").glob("[0-9]*/comm"):
        try:
            if process.read_text().strip().lower().startswith(("rocksmith", "rsmods")):
                return True
        except (OSError, UnicodeError):
            continue
    return False


def bundle_files(archive):
    """Validate the entire archive before allowing any game-directory writes."""
    entries = {}
    for info in archive.infolist():
        name = info.filename.replace("\\", "/")
        path = PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts or ":" in name:
            raise ValueError(f"Unsafe archive path: {name}")
        if stat.S_ISLNK(info.external_attr >> 16):
            raise ValueError(f"Archive contains a symlink: {name}")
        if info.is_dir() or name.endswith("/"):
            continue
        if name in entries:
            raise ValueError(f"Duplicate archive path: {name}")
        entries[name] = info
    roots = [name.removesuffix("xinput1_3.dll") for name in entries
             if PurePosixPath(name).name == "xinput1_3.dll"]
    if len(roots) != 1:
        raise ValueError("Expected exactly one xinput1_3.dll in the portable archive")
    root = roots[0]
    payload = {name[len(root):]: info for name, info in entries.items()
               if name.startswith(root) and
               (name == root + "xinput1_3.dll" or name.startswith(root + "RSMods/"))}
    if "RSMods/RSMods.exe" not in payload:
        raise ValueError("Portable archive is missing RSMods/RSMods.exe")
    return payload


def install(args, game):
    if running_game():
        raise ValueError("Close Rocksmith and RSMods before installing")
    archive = Path(args.archive).expanduser().resolve()
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    if checksum != args.sha256.lower():
        raise ValueError("SHA256 mismatch; installation cancelled")
    with zipfile.ZipFile(archive) as zipped:
        payload = bundle_files(zipped)
        # Updates must not replace user configuration, profiles or custom mods.
        payload = {name: info for name, info in payload.items()
                   if not (name.lower().endswith('.ini') or
                           name.lower() == 'rsmods/qcoverrides.json' or
                           name.lower().startswith(('rsmods/profile_backups/', 'rsmods/custommods/')))}
        # Also reject existing symlinks that could redirect writes outside the game.
        for name in payload:
            target = game / name
            if not target.resolve().is_relative_to(game):
                raise ValueError(f"Destination escapes game directory: {target}")
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
        backup = game / "RSMods-backups" / stamp
        backup.mkdir(parents=True)
        manifest = {"archive": str(archive), "sha256": checksum,
                    "game": str(game), "files": []}
        for name in payload:
            target = game / name
            record = {"path": name, "existed": target.exists(),
                      "installed_sha256": hashlib.sha256(zipped.read(payload[name])).hexdigest()}
            if target.exists():
                dest = backup / "files" / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(target, dest)
                record['previous_sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
                if hashlib.sha256(dest.read_bytes()).hexdigest() != record['previous_sha256']:
                    raise ValueError('Backup verification failed: ' + name)
            manifest["files"].append(record)
        for name in ("Rocksmith.ini", "RSMods.ini", "D3DX9_42.dll"):
            if (game / name).is_file():
                dest = backup / "reference" / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(game / name, dest)
        (backup / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        try:
            for record in manifest['files']:
                name = record['path']
                target = game / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(zipped.read(payload[name]))
                if hashlib.sha256(target.read_bytes()).hexdigest() != record['installed_sha256']:
                    raise ValueError('Installation verification failed: ' + name)
        except OSError:
            restore_files(backup, game, manifest['files'])
            raise
        except ValueError:
            restore_files(backup, game, manifest['files'])
            raise
        (game / "RSMods/CustomMods").mkdir(exist_ok=True)
    print(f"Installed {len(payload)} files in {game}")
    print(f"Backup and rollback manifest: {backup}")
    return backup


def restore_files(backup, game, records):
    for record in records:
        target = game / record['path']
        if record['existed']:
            shutil.copy2(backup / 'files' / record['path'], target)
            if hashlib.sha256(target.read_bytes()).hexdigest() != record['previous_sha256']:
                raise ValueError('Restoration verification failed: ' + record['path'])
        else:
            target.unlink(missing_ok=True)


def rollback(backup, game):
    if running_game():
        raise ValueError('Close Rocksmith and RSMods before rolling back')
    backup = Path(backup).resolve()
    manifest = json.loads((backup / 'manifest.json').read_text())
    if Path(manifest['game']).resolve() != game.resolve():
        raise ValueError('Backup belongs to a different game directory')
    for record in manifest['files']:
        name = record['path']
        if name != 'xinput1_3.dll' and not name.startswith('RSMods/'):
            raise ValueError('Unexpected backup target: ' + name)
        target, source = game / name, backup / 'files' / name
        if not target.resolve().is_relative_to(game.resolve()) or not source.resolve().is_relative_to(backup):
            raise ValueError('Backup path escapes the installation')
        if not target.is_file() or hashlib.sha256(target.read_bytes()).hexdigest() != record.get('installed_sha256'):
            raise ValueError('Installed file changed after this update; preserve and inspect it before rollback: ' + name)
        if record['existed'] and hashlib.sha256(source.read_bytes()).hexdigest() != record.get('previous_sha256'):
            raise ValueError('Backup verification failed: ' + name)
    restore_files(backup, game, manifest['files'])
    print('Previous mod restored; personal configuration was retained.')


def command_output(argv):
    if not shutil.which(argv[0]):
        return "Command unavailable: " + argv[0]
    result = subprocess.run(argv, capture_output=True, text=True, timeout=20)
    return (result.stdout + result.stderr).strip()


def doctor(game):
    steamapps = game.parent.parent
    compatdata = steamapps / f"compatdata/{APP_ID}"
    if not compatdata.exists():
        compatdata = next((r / f"steamapps/compatdata/{APP_ID}" for r in steam_roots()
                           if (r / f"steamapps/compatdata/{APP_ID}").exists()), compatdata)
    report = {"game": str(game), "compatdata": str(compatdata), "files": {}}
    for name in ("Rocksmith2014.exe", "xinput1_3.dll", "RSMods/RSMods.exe",
                 "RSMods.ini", "RS_ASIO.dll", "avrt.dll", "RS_ASIO.ini", "D3DX9_42.dll"):
        p = game / name
        report["files"][name] = {"exists": p.exists()}
        if p.is_file():
            report["files"][name].update(size=p.stat().st_size,
                sha256=hashlib.sha256(p.read_bytes()).hexdigest())
    config = compatdata / "config_info"
    report["proton"] = config.read_text().splitlines()[0] if config.exists() else None
    report["pipewire"] = command_output(["wpctl", "status"])
    report["midi"] = command_output(["aconnect", "-l"])
    report["alsa_cards"] = Path("/proc/asound/cards").read_text() if Path("/proc/asound/cards").exists() else ""
    report["launch_options"] = 'WINEDLLOVERRIDES="xinput1_3=n,b" %command%'
    print(json.dumps(report, indent=2))


def gui(game):
    if not (game / "RSMods/RSMods.exe").exists():
        raise ValueError("RSMods GUI is not installed")
    os.execvp("protontricks", ["protontricks", "--cwd-app", "-c",
                              'wine "RSMods/RSMods.exe"; rsmods_exit=$?; wineserver -w; exit "$rsmods_exit"', APP_ID])


def midi():
    root = Path(__file__).resolve().parent.parent
    source = root / "scripts/WinMidiProbe.cs"
    output = root / ".local/WinMidiProbe.exe"
    output.parent.mkdir(exist_ok=True)
    def wine_path(path):
        return "Z:" + str(path).replace("/", "\\")
    compiler = ["wine", r"C:\windows\Microsoft.NET\Framework\v4.0.30319\csc.exe",
                "/nologo", "/platform:x86", "/out:" + wine_path(output), wine_path(source)]
    command = shlex.join(compiler) + " && " + shlex.join(["wine", str(output)]) + " && wineserver -w"
    subprocess.run(["protontricks", "-c", command, APP_ID], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", help="Override Steam library discovery")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("doctor", help="Print a read-only diagnostic JSON snapshot")
    sub.add_parser("gui", help="Open RSMods in Rocksmith's Proton prefix")
    sub.add_parser("midi", help="Compile and run a read-only 32-bit WinMM MIDI probe")
    inst = sub.add_parser("install", help="Install a verified portable ZIP with backups")
    inst.add_argument("--archive", required=True)
    inst.add_argument("--sha256", required=True)
    undo = sub.add_parser('rollback', help='Restore a verified mod installation backup')
    undo.add_argument('--backup', required=True)
    args = parser.parse_args()
    try:
        game = find_game(args.game_dir)
        if args.command == "doctor":
            doctor(game)
        elif args.command == "install":
            install(args, game)
        elif args.command == 'rollback':
            rollback(args.backup, game)
        elif args.command == "midi":
            midi()
        else:
            gui(game)
    except (OSError, ValueError, zipfile.BadZipFile, subprocess.TimeoutExpired,
            subprocess.CalledProcessError) as exc:
        parser.exit(1, f"Error: {exc}\n")


if __name__ == "__main__":
    main()
