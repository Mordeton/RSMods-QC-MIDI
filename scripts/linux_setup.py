#!/usr/bin/env python3
"""Install and manage the RSMods QC desktop tools and per-game PipeASIO setup."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request

import linux
import steam_options

APP = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) / 'rsmods-qc'
CONFIG = APP / 'config.json'
BIN = Path.home() / '.local/bin'
VERSION = '1.10.0'
DRIVER_SHA = '54f959381e1a60f05d0e3b662ed09b08d444d3161a248cd9964c23f47b057a5b'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(config):
    APP.mkdir(parents=True, exist_ok=True, mode=0o700)
    temp = CONFIG.with_suffix('.tmp')
    temp.write_text(json.dumps(config, indent=2) + '\n')
    temp.replace(CONFIG)


def load():
    return json.loads(CONFIG.read_text())


def message(text, error=False):
    print(text, file=sys.stderr if error else sys.stdout)
    if os.environ.get('RSMODS_QC_DIALOGS') == '1' and shutil.which('kdialog'):
        subprocess.run(['kdialog', '--title', 'RSMods QC', '--error' if error else '--msgbox', text])


def patch_ini(text, section, updates):
    """Preserve unrelated comments/settings and the existing line-ending style."""
    ending = '\r\n' if '\r\n' in text else '\n'
    lines = text.splitlines()
    output, seen, in_section, found = [], set(), False, False
    for line in lines:
        match = re.match(r'\s*\[([^]]+)\]', line)
        if match:
            if in_section:
                output.extend(k + '=' + str(v) for k, v in updates.items() if k.casefold() not in seen)
            in_section = match[1].casefold() == section.casefold()
            if in_section and found:
                raise ValueError('Duplicate INI section: ' + section)
            found |= in_section
        if in_section and '=' in line and not line.lstrip().startswith((';', '#')):
            key = line.split('=', 1)[0].strip()
            target = next((k for k in updates if k.casefold() == key.casefold()), None)
            if target:
                if key.casefold() in seen:
                    raise ValueError('Duplicate INI key: ' + key)
                line = key + '=' + str(updates[target])
                seen.add(key.casefold())
        output.append(line)
    if in_section:
        output.extend(k + '=' + str(v) for k, v in updates.items() if k.casefold() not in seen)
    if not found:
        output += ['', '[' + section + ']'] + [k + '=' + str(v) for k, v in updates.items()]
    return ending.join(output) + ending


def audio_configs(rocksmith, asio, channel):
    rocksmith = patch_ini(rocksmith, 'Audio', {'ExclusiveMode': 1, 'Win32UltraLowLatencyMode': 1, 'EnableMicrophone': 0})
    asio = patch_ini(asio, 'Config', {'EnableWasapiOutputs': 0, 'EnableWasapiInputs': 0, 'EnableAsio': 1})
    asio = patch_ini(asio, 'Asio', {'BufferSizeMode': 'driver'})
    asio = patch_ini(asio, 'Asio.Output', {'Driver': 'PipeASIO', 'BaseChannel': 0})
    asio = patch_ini(asio, 'Asio.Input.0', {'Driver': 'PipeASIO', 'Channel': channel})
    for section in ['Asio.Input.1', 'Asio.Input.Mic']:
        asio = patch_ini(asio, section, {'Driver': ''})
    return rocksmith, asio


def qc_nodes():
    selected = []
    for kind, prefix in [('sinks', 'alsa_output.'), ('sources', 'alsa_input.')]:
        items = json.loads(subprocess.check_output(['pactl', '-f', 'json', 'list', kind]))
        matches = [s for s in items if s['name'].startswith(prefix + 'usb-Neural_DSP_Quad_Cortex-')]
        if len(matches) != 1 or matches[0]['sample_specification'].split()[1] != '8ch':
            raise ValueError('Connect one Quad Cortex and select its 8-channel playback/capture profile in KDE Audio.')
        selected.append(matches[0]['name'])
    return selected


def audio_env(config, connect=True):
    env = dict(os.environ)
    env.update(PROTON_USE_WOW64='1', WINEDLLPATH=str(Path(config['driver']) / 'lib/wine'),
               PIPEASIO_NUMBER_INPUTS='8', PIPEASIO_NUMBER_OUTPUTS='2',
               PIPEASIO_PREFERRED_BUFFERSIZE=str(config['buffer']), PIPEASIO_FIXED_BUFFERSIZE='on',
               PIPEASIO_SAMPLE_RATE='0', PIPEASIO_CONNECT_TO_HARDWARE='on' if connect else 'off',
               PIPEASIO_REALTIME='off', PIPEASIO_CLIENT_NAME='Rocksmith-QC')
    if connect:
        env['PIPEASIO_OUTPUT_DEVICE'], env['PIPEASIO_INPUT_DEVICE'] = qc_nodes()
    overrides = [p for p in env.get('WINEDLLOVERRIDES', '').split(';')
                 if p and p.split('=', 1)[0].casefold() not in ['xinput1_3', 'avrt']]
    env['WINEDLLOVERRIDES'] = ';'.join(overrides + ['xinput1_3=n,b', 'avrt=n,b'])
    # Rocksmith uses QC audio/MIDI, never its editor HID interface. Leave it
    # available to Cortex Control, without disabling other game controllers.
    ignored = env.get('PROTON_DISABLE_HIDRAW', '')
    if '0x152a/0x880a' not in ignored.casefold():
        env['PROTON_DISABLE_HIDRAW'] = ','.join(filter(None, [ignored, '0x152A/0x880A']))
    return env


def runner_env(config):
    env = audio_env(config, connect=False)
    env.update(WINEPREFIX=config['prefix'], PROTONPATH=config['proton'], GAMEID='umu-221680',
               STORE='steam', STEAM_COMPAT_INSTALL_PATH=config['game'], WINEDEBUG='-all,err+all')
    return env


def payload():
    dest = APP / 'pipeasio' / VERSION
    stamp = dest / '.verified-sha256'
    if stamp.exists() and stamp.read_text().strip() == DRIVER_SHA:
        return dest
    APP.mkdir(parents=True, exist_ok=True, mode=0o700)
    with tempfile.TemporaryDirectory(dir=APP) as tmp:
        tmp = Path(tmp)
        archive = tmp / 'driver.tar.gz'
        url = f'https://github.com/M0n7y5/pipeasio/releases/download/v{VERSION}/pipeasio-v{VERSION}-archlinux-x86_64.tar.gz'
        with urllib.request.urlopen(url, timeout=45) as response:
            archive.write_bytes(response.read())
        if digest(archive) != DRIVER_SHA:
            raise ValueError('PipeASIO archive checksum mismatch')
        unpacked = tmp / 'payload'
        with tarfile.open(archive) as tar:
            for entry in tar:
                if entry.issym() or entry.islnk():
                    raise ValueError('Unexpected link in PipeASIO archive')
            tar.extractall(unpacked, filter='data')
        if dest.exists():
            for file in unpacked.rglob('*'):
                if file.is_file() and (not (dest / file.relative_to(unpacked)).is_file() or digest(file) != digest(dest / file.relative_to(unpacked))):
                    raise ValueError('Existing PipeASIO payload differs; preserve it and inspect before updating.')
        else:
            dest.parent.mkdir(parents=True, exist_ok=True)
            unpacked.rename(dest)
        stamp.write_text(DRIVER_SHA + '\n')
    return dest


def find_context(game):
    for steam in linux.steam_roots():
        prefix = steam / 'steamapps/compatdata/221680/pfx'
        if prefix.is_dir():
            if '.var/app/' in str(steam):
                raise ValueError('This first release supports native Steam. Flatpak Steam needs separate qualification.')
            info = prefix.parent / 'config_info'
            lines = info.read_text().splitlines() if info.exists() else []
            candidates = [Path(line.split('/files/', 1)[0]) for line in lines if '/files/' in line]
            proton = next((p for p in candidates if (p / 'proton').is_file()), None)
            if proton and (proton / 'proton').is_file():
                return steam, prefix, proton
    raise ValueError('Run Rocksmith through Proton once, then install RSMods QC.')


def desktop_quote(value):
    return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"').replace('$', '\\$').replace('`', '\\`').replace('%', '%%') + '"'


def install_desktop(config):
    scripts = APP / 'scripts'
    scripts.mkdir(parents=True, exist_ok=True)
    for name in ['linux.py', 'linux_setup.py', 'steam_options.py']:
        source = Path(__file__).with_name(name)
        if source.resolve() != (scripts / name).resolve():
            shutil.copy2(source, scripts / name)
    BIN.mkdir(parents=True, exist_ok=True)
    for name, command in [('rsmods-qc-session', 'wrap'), ('rsmods-qc', '')]:
        path = BIN / name
        path.write_text('#!/bin/sh\nexec /usr/bin/python3 ' + shlex.quote(str(scripts / 'linux_setup.py')) + ' ' + command + ' "$@"\n')
        path.chmod(0o755)
    applications = APP.parent / 'applications'
    applications.mkdir(exist_ok=True)
    for name, title, action in [('rsmods-qc-proton', 'RSMods QC Settings', 'gui'), ('rocksmith-qc', 'Rocksmith with Quad Cortex', 'play')]:
        path = applications / (name + '.desktop')
        if path.exists() and not (APP / (name + '.desktop.before')).exists():
            shutil.copy2(path, APP / (name + '.desktop.before'))
        path.write_text('[Desktop Entry]\nType=Application\nName=' + title + '\nExec=/usr/bin/env RSMODS_QC_DIALOGS=1 ' + desktop_quote(BIN / 'rsmods-qc') + ' ' + action + '\nIcon=applications-games\nTerminal=false\nCategories=Game;\n')
        subprocess.run(['desktop-file-validate', str(path)], check=True)
    if shutil.which('update-desktop-database'):
        subprocess.run(['update-desktop-database', str(applications)], check=True)


def configure(args):
    if linux.running_game():
        raise ValueError('Close Rocksmith and RSMods before changing the setup.')
    for tool in ['umu-run', 'pactl', 'steam', 'desktop-file-validate']:
        if not shutil.which(tool):
            raise ValueError('Required tool missing: ' + tool)
    game = linux.find_game(args.game_dir)
    steam, prefix, proton = find_context(game)
    driver = payload()
    config = load() if CONFIG.exists() else {'game': str(game), 'prefix': str(prefix), 'proton': str(proton), 'steam': str(steam)}
    if config['game'] != str(game):
        raise ValueError('Existing installation belongs to a different game directory.')
    config.update(driver=str(driver), buffer=args.buffer, input_channel=args.input_channel,
                  proton_version=(proton / 'version').read_text().strip())
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup = APP / 'backups' / stamp
    backup.mkdir(parents=True)
    for name in ['Rocksmith.ini', 'RS_ASIO.ini', 'RSMods.ini', 'RSMods/GUI_Settings.ini']:
        source = game / name
        if source.exists():
            target = backup / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            if digest(target) != digest(source):
                raise ValueError('Backup verification failed: ' + name)
    if not config.get('prefix_backup'):
        target = backup / 'prefix'
        subprocess.run(['cp', '-a', '--reflink=auto', str(prefix), str(target)], check=True)
        config['prefix_backup'] = str(target)
    # Validate both edits before any game-file write.
    current = [(game / name).read_bytes().decode('utf-8-sig') for name in ['Rocksmith.ini', 'RS_ASIO.ini']]
    edited = audio_configs(*current, args.input_channel)
    save(config)
    env = runner_env(config)
    env.update(WINE='umu-run', PIPEASIO_PREFIX=str(driver), PIPEASIO_CHECK_ISOLATED='1')
    if not args.skip_register:
        with (backup / 'driver-register.log').open('w') as log:
            subprocess.run([str(driver / 'bin/pipeasio-register')], env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=300)
    result = backup / 'driver-check.json'
    with (backup / 'driver-check.log').open('w') as log:
        subprocess.run(['umu-run', str(driver / 'share/pipeasio/manager/pipeasio-check32.exe'), '--result', 'Z:' + str(result).replace('/', '\\')], env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=100)
    checked = json.loads(result.read_text())
    if not all(checked.get(k) for k in ['registered', 'unixlib', 'pipewire']):
        raise ValueError('PipeASIO did not pass the actual-prefix runtime check')
    config.setdefault('audio_backups', []).append(str(backup))
    if not config.get('original_audio_backup'):
        config['original_audio_backup'] = str(backup)
    save(config)
    for name, content in zip(['Rocksmith.ini', 'RS_ASIO.ini'], edited):
        (game / name).write_bytes(content.encode('utf-8'))
    install_desktop(config)
    print('Audio configured; input channel', args.input_channel + 1, '(USB channel numbering). Backup:', backup)
    print('Next: rsmods-qc steam-options --close-steam')


def steam_running():
    return subprocess.run(['pgrep', '-x', 'steam'], stdout=subprocess.DEVNULL).returncode == 0


def steam_options_install(args, restore=False):
    config = load()
    if linux.running_game():
        raise ValueError('Close Rocksmith and RSMods first.')
    was_running = steam_running()
    if was_running:
        if not args.close_steam:
            raise ValueError('Close Steam, or use --close-steam to close it normally while saving launch options.')
        subprocess.run(['steam', '-shutdown'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        deadline = time.monotonic() + 45
        while steam_running() and time.monotonic() < deadline:
            time.sleep(0.5)
        if steam_running():
            raise ValueError('Steam is still open; launch options were not changed.')
    candidates = []
    for path in (Path(config['steam']) / 'userdata').glob('*/config/localconfig.vdf'):
        text = path.read_text()
        try:
            old = steam_options.get_options(text)
        except KeyError:
            continue
        candidates.append((path, text, old))
    if len(candidates) != 1:
        raise ValueError('Could not identify one Steam account with Rocksmith launch options.')
    path, text, old = candidates[0]
    wrapper = shlex.quote(str(BIN / 'rsmods-qc-session'))
    if restore:
        if 'steam_previous_options' not in config:
            raise ValueError('No previous Steam launch options were recorded.')
        expected = config.get('steam_installed_options', wrapper + ' %command%')
        if old != expected:
            raise ValueError('Launch options changed since setup; keep the current options and review them manually.')
        changed = steam_options.set_options(text, config['steam_previous_options'])
        temporary = path.with_suffix('.rsmods-qc.tmp')
        temporary.write_text(changed)
        temporary.replace(path)
    elif str(BIN / 'rsmods-qc-session') not in old:
        if old and '%command%' not in old:
            raise ValueError('Existing custom launch options need manual review; no changes made.')
        new = old.replace('%command%', wrapper + ' %command%') if old else wrapper + ' %command%'
        config.setdefault('steam_previous_options', old)
        config['steam_options_file'] = str(path)
        config['steam_installed_options'] = new
        save(config)
        backup = APP / 'steam-localconfig.before.vdf'
        if not backup.exists():
            shutil.copy2(path, backup)
        changed = steam_options.set_options(text, new)
        if steam_options.get_options(changed) != new:
            raise ValueError('Steam option verification failed')
        temporary = path.with_suffix('.rsmods-qc.tmp')
        temporary.write_text(changed)
        temporary.replace(path)
    if was_running:
        subprocess.Popen(['steam', '-silent'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    print('Previous Steam launch options restored.' if restore else 'Steam launch options configured for Rocksmith only.')


def wrap(command):
    config = load()
    if not command:
        raise ValueError('The session wrapper must receive Steam’s %command%.')
    actual = (Path(config['proton']) / 'version').read_text().strip()
    if actual != config['proton_version']:
        raise ValueError('The selected Proton runtime changed. Re-run audio setup to verify it before playing.')
    env = audio_env(config)
    logs = APP / 'logs'
    logs.mkdir(exist_ok=True)
    if os.environ.get('RSMODS_QC_DEBUG') == '1':
        env.update(PROTON_LOG='1', PROTON_LOG_DIR=str(logs))
    os.execvpe(command[0], command, env)


def restore_audio():
    if linux.running_game():
        raise ValueError('Close Rocksmith and RSMods before restoring audio settings.')
    config = load()
    backup = Path(config['original_audio_backup'])
    game = Path(config['game'])
    for name in ['Rocksmith.ini', 'RS_ASIO.ini']:
        source, current = backup / name, game / name
        # Keep any settings changed since installation for review/recovery.
        shutil.copy2(current, APP / (name + '.before-restore'))
        shutil.copy2(source, current)
    print('Original game audio files restored. Saved the replaced files for recovery.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest='action', required=True)
    setup = subs.add_parser('audio')
    setup.add_argument('--game-dir')
    setup.add_argument('--input-channel', type=int, choices=range(8), default=0)
    setup.add_argument('--buffer', type=int, choices=[128, 256, 512, 1024], default=256)
    setup.add_argument('--skip-register', action='store_true', help='Reuse an already registered driver; runtime check still runs')
    options = subs.add_parser('steam-options')
    options.add_argument('--close-steam', action='store_true')
    restore = subs.add_parser('restore-steam-options')
    restore.add_argument('--close-steam', action='store_true')
    update = subs.add_parser('install', help='Install or update from a verified portable ZIP, preserving settings')
    update.add_argument('--archive', required=True)
    update.add_argument('--sha256', required=True)
    undo = subs.add_parser('rollback', help='Restore a verified mod backup while retaining settings')
    undo.add_argument('--backup', required=True)
    session = subs.add_parser('wrap')
    session.add_argument('command', nargs=argparse.REMAINDER)
    for name in ['play', 'gui', 'doctor', 'restore-audio', 'desktop-install']:
        subs.add_parser(name)
    args = parser.parse_args()
    try:
        if args.action == 'audio':
            configure(args)
        elif args.action == 'steam-options':
            steam_options_install(args)
        elif args.action == 'restore-steam-options':
            steam_options_install(args, restore=True)
        elif args.action == 'install':
            config = load()
            backup = linux.install(args, Path(config['game']))
            config['last_mod_backup'] = str(backup)
            save(config)
        elif args.action == 'rollback':
            linux.rollback(args.backup, Path(load()['game']))
        elif args.action == 'wrap':
            wrap(args.command)
        elif args.action == 'restore-audio':
            restore_audio()
        else:
            config = load()
            if args.action == 'play':
                qc_nodes()
                subprocess.Popen(['steam', '-applaunch', '221680'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
            elif args.action == 'gui':
                env = runner_env(config)
                subprocess.run(['umu-run', str(Path(config['game']) / 'RSMods/RSMods.exe')], cwd=config['game'], env=env, check=True)
            elif args.action == 'doctor':
                linux.doctor(Path(config['game']))
            else:
                install_desktop(config)
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
        message(str(exc), error=True)
        raise SystemExit(1)


if __name__ == '__main__':
    main()
