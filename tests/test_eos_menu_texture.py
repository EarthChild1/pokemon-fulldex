"""Explorers of Sky: the New Game menu's Grovyle picture is carried on beside the screen, not smeared.

The menu puts its picture straight into the hardware map, one screen of it, after the scene before has
ended: no map in RAM gives more of it, and beside the screen there was only each row's edge colour drawn
out, a smear of streaks. pmdground.h's HardwarePicture makes the hardware map the layer's map, so the
picture is carried on from its own cells; a plain one (the Nintendo logo on white) is left plain. Needs
your own ROM and a headless build (not part of the usual run):

  SSR_TEST_EOS_ROM=<Explorers of Sky (US) .nds> SSR_TEST_BINARY=<build>/ssrecomp \
    python -m unittest tests.test_eos_menu_texture
"""
from pathlib import Path
import os, subprocess, tempfile, unittest

try:
    from tests.test_eos_new_game import read_png
except ImportError:  # (run from tests/, as unittest discover does)
    from test_eos_new_game import read_png

ROOT = Path(__file__).resolve().parents[1]
ROM = os.environ.get('SSR_TEST_EOS_ROM', '')
BINARY = os.environ.get('SSR_TEST_BINARY', '')
LOGO, MENU = 150, 7700  # from power-on, no input


def along_rows(path, x0, x1):
    """The mean, over the rows, of how much a row changes along x0..x1 (mean |difference| of neighbours,
    0-255 a channel): 0 for a smear of one colour a row, more for a picture."""
    w, h, rows, bpp = read_png(path)
    total, count = 0, 0
    for y in range(0, h, 4):
        r = rows[y]
        for x in range(x0, x1 - 2, 2):
            a, b = r[x * bpp:x * bpp + 3], r[(x + 2) * bpp:(x + 2) * bpp + 3]
            total += sum(abs(a[i] - b[i]) for i in range(3)) / 3
            count += 1
    return total / count


class EosMenuTextureTests(unittest.TestCase):
    @unittest.skipUnless(ROM and BINARY and Path(ROM).is_file() and Path(BINARY).is_file(),
                         'SSR_TEST_EOS_ROM and SSR_TEST_BINARY required')
    def test_menu_picture_carried_on(self):
        with tempfile.TemporaryDirectory() as folder:
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
            r = subprocess.run([BINARY, ROM, '--headless', '--no-save', '--jit', '--funcs', str(ROOT / 'tools/pmd/C2SE.txt'),
                                '--frames', str(MENU + 5), '--shot-dir', folder, '--shot-at', f'{LOGO},{MENU}',
                                '--compose', '1600x720'], capture_output=True, text=True, env=env, timeout=900)
            self.assertEqual(r.returncode, 0, r.stderr[-2000:])
            # The left margin (1600 x 720: the screen is x 320-1279) beside the Grovyle picture: a picture's
            # changes along each row, not a smear (which measures under 1).
            # (Measured: 0.0 smeared, 5.3 carried on.)
            self.assertGreater(along_rows(Path(folder) / f'frame_{MENU:06d}.png', 0, 300), 3, 'the menu picture is smeared beside the screen')
            # The logo on white: its margins stay plain white.
            self.assertLess(along_rows(Path(folder) / f'frame_{LOGO:06d}.png', 0, 300), 1, 'something beside the Nintendo logo')


if __name__ == '__main__':
    unittest.main()
