"""Explorers of Sky's wide view: a map carried on past its picture (frontend/widefill.h), synthetic maps only."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WideFillTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_fill_continues_pictures(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'widefill'
            subprocess.run(['c++', '-std=c++17', '-O2', '-I' + str(ROOT / 'frontend'), '-I' + str(ROOT / 'melonDS/src'), '-pthread',
                            str(ROOT / 'tests/test_widefill.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True, timeout=60)


if __name__ == '__main__':
    unittest.main()
