"""Explorers of Sky's second-screen panel: where it goes, and when it steps aside (ROM-free)."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
ROOT = Path(__file__).resolve().parents[1]


def sdl_flags():
    if shutil.which('pkg-config'):
        r = subprocess.run(['pkg-config', '--cflags', 'sdl2'], capture_output=True, text=True)
        if r.returncode == 0:
            return r.stdout.split()
    return None


class PmdPanelTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and sdl_flags() is not None, 'C++ compiler and SDL2 headers required')
    def test_panel_place_and_overlap(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'pmd-panel'
            subprocess.run(['c++', '-std=c++17', *sdl_flags(), '-I' + str(ROOT / 'frontend'), '-I' + str(ROOT / 'melonDS/src'),
                            str(ROOT / 'tests/test_pmd_panel.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True, timeout=30)


if __name__ == '__main__':
    unittest.main()
