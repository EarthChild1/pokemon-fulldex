"""Explorers of Sky: a message box on a screen fading to white is not mirrored beside the screen.

After the personality test, "Waaaaah!" is shown in a message box while the black screen fades to white.
The sides carry a plain screen on in its colour, leaving out the rows through a window's dark grey
body; the fade made the body lighter than that grey, so its rows counted, the colour fell under 90%
and the picture was mirrored instead: the box and its letters the wrong way round in the left margin
(the owner's video, 6.7 s). Needs your own ROM and a headless build (not part of the usual run):

  SSR_TEST_EOS_ROM=<Explorers of Sky (US) .nds> SSR_TEST_BINARY=<build>/ssrecomp \
    python -m unittest tests.test_eos_fade_box
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
FADE = [6860, 6890]  # the box over the fade, at about a third and two thirds of the way to white


def script(last):
    # From power-on: A through the intro, New Game and the questions, the Bow held, then UP, A, START, A
    # every 64 frames (the partner, the naming screens, the story's messages).
    lines = [f'{a} {a + 3} A' for a in range(300, 2850, 16)]
    lines.append('2850 3900 touch 128 96')
    lines += [f'{a} {a + 3} A' for a in range(2860, 3900, 32)]
    for a in range(3900, last, 64):
        lines += [f'{a} {a + 3} UP', f'{a + 16} {a + 19} A', f'{a + 32} {a + 35} START', f'{a + 48} {a + 51} A']
    return '\n'.join(lines) + '\n'


def margin_spread(path):
    """The largest difference of any channel from the margin's first pixel, over both 160-px margins of
    the box's rows (1600 x 720: the screen is x 320-1279; the box is in its lower quarter)."""
    w, h, rows, bpp = read_png(path)
    worst = 0
    for y in range(int(h * 0.75), int(h * 0.95)):
        r = rows[y]
        for x0, x1 in ((0, 160), (w - 160, w)):
            ref = r[x0 * bpp:x0 * bpp + 3]
            for x in range(x0, x1, 2):
                p = r[x * bpp:x * bpp + 3]
                worst = max(worst, max(abs(p[i] - ref[i]) for i in range(3)))
    return worst


class EosFadeBoxTests(unittest.TestCase):
    @unittest.skipUnless(ROM and BINARY and Path(ROM).is_file() and Path(BINARY).is_file(),
                         'SSR_TEST_EOS_ROM and SSR_TEST_BINARY required')
    def test_box_not_mirrored_while_fading(self):
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder) / 'input.txt').write_text(script(6900))
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
            r = subprocess.run([BINARY, ROM, '--headless', '--no-save', '--jit', '--funcs', str(ROOT / 'tools/pmd/C2SE.txt'),
                                '--frames', '6900', '--input', str(Path(folder) / 'input.txt'), '--shot-dir', folder,
                                '--shot-at', ','.join(map(str, FADE)), '--compose', '1600x720'],
                               capture_output=True, text=True, env=env, timeout=900)
            self.assertEqual(r.returncode, 0, r.stderr[-2000:])
            for f in FADE:
                # The margins beside the box are the fade's one colour (a mirrored box: its bright frame and
                # letters, 100 and more from the ground).
                self.assertLess(margin_spread(Path(folder) / f'frame_{f:06d}.png'), 24, f'frame {f}: something beside the box')


if __name__ == '__main__':
    unittest.main()
