import importlib.util
from pathlib import Path
import sys
import unittest

SCRIPTS = Path(__file__).parents[1]
sys.path.insert(0, str(SCRIPTS))
import linux_setup
import steam_options


class ConfigurationTests(unittest.TestCase):
    def test_audio_edit_preserves_unrelated_sections_and_crlf(self):
        ini = '[Audio]\r\n; keep comment\r\nExclusiveMode=0\r\nLatencyBuffer=4\r\n[Renderer.Win32]\r\nFullscreen=1\r\n'
        result = linux_setup.patch_ini(ini, 'Audio', {'ExclusiveMode': 1, 'EnableMicrophone': 0})
        self.assertIn('ExclusiveMode=1\r\nLatencyBuffer=4\r\n', result)
        self.assertIn('; keep comment\r\n', result)
        self.assertTrue(result.endswith('[Renderer.Win32]\r\nFullscreen=1\r\n'))

    def test_duplicate_ini_key_refuses_instead_of_guessing(self):
        with self.assertRaises(ValueError):
            linux_setup.patch_ini('[Audio]\nExclusiveMode=0\nExclusiveMode=1\n', 'Audio', {'ExclusiveMode': 1})

    def test_only_selected_asio_input_enabled(self):
        _, ini = linux_setup.audio_configs('[Audio]\n', '[Asio.Input.1]\nDriver=old\n[Asio.Input.Mic]\nDriver=old\n', 2)
        self.assertIn('[Asio.Input.0]\nDriver=PipeASIO\nChannel=2\n', ini)
        self.assertNotIn('Driver=old', ini)


class SteamOptionsTests(unittest.TestCase):
    def fixture(self, entry):
        return '"UserLocalConfigStore" { "Software" { "Valve" { "Steam" { "apps" { "221680" { ' + entry + ' } "42" { "LaunchOptions" "keep" } } } } } }'

    def test_updates_only_game_value_and_preserves_escaped_options(self):
        before = self.fixture('"LaunchOptions" "old" "unrelated" "{keep}"')
        new = 'env X="a b" /home/user/wrapper %command%'
        after = steam_options.set_options(before, new)
        self.assertEqual(steam_options.get_options(after), new)
        self.assertEqual(after.replace(steam_options.quote(new), '"old"'), before)

    def test_adds_missing_key_in_existing_game(self):
        before = self.fixture('"LastPlayed" "123"')
        after = steam_options.set_options(before, 'wrapper %command%')
        self.assertEqual(steam_options.get_options(after), 'wrapper %command%')
        self.assertIn('"42" { "LaunchOptions" "keep" }', after)

    def test_duplicate_key_refused(self):
        with self.assertRaises(ValueError):
            steam_options.get_options(self.fixture('"LaunchOptions" "a" "LaunchOptions" "b"'))


if __name__ == '__main__':
    unittest.main()
