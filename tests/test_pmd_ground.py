"""Explorers of Sky's maps in RAM: the hardware map checked against the window the game names and the
ones a chunk to each side (ROM-free)."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
ROOT = Path(__file__).resolve().parents[1]


class PmdGroundTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_window_a_chunk_back(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'pmd-ground'
            subprocess.run(['c++', '-std=c++17', '-O2', '-I' + str(ROOT / 'frontend'), '-I' + str(ROOT / 'melonDS/src'), '-pthread',
                            str(ROOT / 'tests/test_pmd_ground.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True, timeout=60)


if __name__ == '__main__':
    unittest.main()
