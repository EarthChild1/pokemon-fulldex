from pathlib import Path
import shutil, subprocess, tempfile, unittest
ROOT = Path(__file__).resolve().parents[1]


def sdl_flags():
    if shutil.which('pkg-config'):
        r = subprocess.run(['pkg-config', '--cflags', 'sdl2'], capture_output=True, text=True)
        if r.returncode == 0:
            return r.stdout.split()
    return None


class GenericGameTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and sdl_flags() is not None, 'C++ compiler and SDL2 headers required')
    def test_generic_game_layout_and_focus(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'generic-game'
            subprocess.run(['c++', '-std=c++17', *sdl_flags(), str(ROOT / 'tests/test_generic_game.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True, timeout=30)
