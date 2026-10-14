import importlib.util
import argparse
import hashlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location("linux", Path(__file__).parents[1] / "linux.py")
linux = importlib.util.module_from_spec(spec)
spec.loader.exec_module(linux)


class PortableArchiveTests(unittest.TestCase):
    def archive(self, names):
        data = io.BytesIO()
        with zipfile.ZipFile(data, "w") as z:
            for name in names:
                z.writestr(name, b"test")
        return zipfile.ZipFile(data)

    def test_windows_zip_separators(self):
        with self.archive([r"bundle\xinput1_3.dll", r"bundle\RSMods\RSMods.exe"]) as z:
            self.assertEqual(set(linux.bundle_files(z)), {"xinput1_3.dll", "RSMods/RSMods.exe"})

    def test_rejects_traversal_before_installing(self):
        with self.archive(["bundle/xinput1_3.dll", "bundle/RSMods/RSMods.exe", "../outside"]) as z:
            with self.assertRaises(ValueError):
                linux.bundle_files(z)

    def test_rejects_symlinks(self):
        data = io.BytesIO()
        with zipfile.ZipFile(data, "w") as z:
            link = zipfile.ZipInfo("bundle/RSMods/link")
            link.external_attr = 0o120777 << 16
            z.writestr(link, "../../outside")
        with zipfile.ZipFile(data) as z, self.assertRaises(ValueError):
            linux.bundle_files(z)

    def test_discovers_secondary_steam_library(self):
        with tempfile.TemporaryDirectory() as tmp:
            root, library = Path(tmp) / "steam", Path(tmp) / "other library"
            (root / "steamapps").mkdir(parents=True)
            (library / "steamapps/common/Rocksmith2014").mkdir(parents=True)
            (root / "steamapps/libraryfolders.vdf").write_text(f'"path" "{library}"')
            (library / "steamapps/appmanifest_221680.acf").write_text('"installdir" "Rocksmith2014"')
            game = library / "steamapps/common/Rocksmith2014"
            (game / "Rocksmith2014.exe").touch()
            with patch.object(linux, "steam_roots", return_value=[root]):
                self.assertEqual(linux.find_game(), game)

    def test_install_and_verified_rollback_preserve_personal_settings(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(linux, 'running_game', return_value=False):
            root = Path(tmp)
            game = root / 'game'
            (game / 'RSMods').mkdir(parents=True)
            (game / 'xinput1_3.dll').write_bytes(b'old dll')
            (game / 'RSMods/GUI_Settings.ini').write_bytes(b'personal settings')
            (game / 'RSMods/QCOverrides.json').write_bytes(b'personal mappings')
            archive = root / 'update.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                for name in ['xinput1_3.dll', 'RSMods/RSMods.exe', 'RSMods/GUI_Settings.ini', 'RSMods/QCOverrides.json']:
                    z.writestr('bundle/' + name, b'new binary or default')
            args = argparse.Namespace(archive=archive, sha256=hashlib.sha256(archive.read_bytes()).hexdigest())
            backup = linux.install(args, game)
            self.assertEqual((game / 'RSMods/GUI_Settings.ini').read_bytes(), b'personal settings')
            self.assertEqual((game / 'RSMods/QCOverrides.json').read_bytes(), b'personal mappings')
            (game / 'xinput1_3.dll').write_bytes(b'newer user change')
            with self.assertRaises(ValueError):
                linux.rollback(backup, game)
            self.assertTrue((game / 'RSMods/RSMods.exe').exists())
            (game / 'xinput1_3.dll').write_bytes(b'new binary or default')
            linux.rollback(backup, game)
            self.assertEqual((game / 'xinput1_3.dll').read_bytes(), b'old dll')
            self.assertFalse((game / 'RSMods/RSMods.exe').exists())
