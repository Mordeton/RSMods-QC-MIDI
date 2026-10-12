import importlib.util
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
