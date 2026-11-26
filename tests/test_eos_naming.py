"""Explorers of Sky's naming screen holds one layout while the player waits on it.

On the partner's nickname screen the layout used to jump about twice a second: the game moves its map
window's chunk some frames before it copies the new window into the hardware map, so for those frames
the world beside the screen was not found, and the picture slid between two layouts (pmdground.h).
And the keyboard, where the player is, was the small screen. Needs your own ROM and a headless build
(not part of the usual run):

  SSR_TEST_EOS_ROM=<Explorers of Sky (US) .nds> SSR_TEST_BINARY=<build>/ssrecomp \
    python -m unittest tests.test_eos_naming
"""
from pathlib import Path
import os, re, subprocess, tempfile, unittest

ROOT = Path(__file__).resolve().parents[1]
ROM = os.environ.get('SSR_TEST_EOS_ROM', '')
BINARY = os.environ.get('SSR_TEST_BINARY', '')

# From power-on to the partner's nickname keyboard (about frame 5363), then no input: A through the
# intro, New Game and the questions, the Bow held, then UP, A, START, A every 64 frames (the partner
# chosen), the last press at 5356.
KEYBOARD_FROM, WAIT_UNTIL = 5380, 5800


def script(typed=()):
    lines = [f'{a} {a + 3} A' for a in range(300, 2850, 16)]
    lines.append('2850 3900 touch 128 96')
    lines += [f'{a} {a + 3} A' for a in range(2860, 3900, 32)]
    for a in range(3900, 5360, 64):
        for off, key in ((0, 'UP'), (16, 'A'), (32, 'START'), (48, 'A')):
            if a + off <= 5360:
                lines.append(f'{a + off} {a + off + 3} {key}')
    lines += [f'{a} {a + 3} A' for a in typed]  # (A on the keyboard: a letter typed into the name)
    return '\n'.join(lines) + '\n'


def run(folder, compose, typed=()):
    (Path(folder) / 'input.txt').write_text(script(typed))
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SSR_PMD_PANEL='1')
    return subprocess.run([BINARY, ROM, '--headless', '--no-save', '--jit', '--funcs', str(ROOT / 'tools/pmd/C2SE.txt'),
                           '--frames', str(WAIT_UNTIL), '--input', str(Path(folder) / 'input.txt'),
                           '--compose', compose], capture_output=True, text=True, env=env, timeout=900)


def panel_lines(log):
    """frame -> (big, world, slide, alpha) from the [panel] lines (SSR_PMD_PANEL=1)."""
    out = {}
    for m in re.finditer(r'^\[panel\] frame (\d+) big (\d) world (\d) layers \d panel \d covered \d slide ([\d.]+) alpha (\d+)$', log, re.M):
        out[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), float(m.group(4)), int(m.group(5)))
    return out


class EosNamingTests(unittest.TestCase):
    @unittest.skipUnless(ROM and BINARY and Path(ROM).is_file() and Path(BINARY).is_file(),
                         'SSR_TEST_EOS_ROM and SSR_TEST_BINARY required')
    def test_naming_screen_holds_one_layout(self):
        for compose in ('1600x720', '1280x720'):  # the owner's phone (20:9) and 16:9
            with self.subTest(compose=compose), tempfile.TemporaryDirectory() as folder:
                r = run(folder, compose)
                self.assertEqual(r.returncode, 0, r.stderr[-2000:])
                self.assertRegex(r.stderr, r'\[pmd\] frame 53\d\d: keyboard')
                frames = panel_lines(r.stderr)
                wait = [frames[f] for f in range(KEYBOARD_FROM, WAIT_UNTIL) if f in frames]
                self.assertGreater(len(wait), (WAIT_UNTIL - KEYBOARD_FROM) * 9 // 10)
                # Nothing changes on screen while the player waits, so the layout changes 0 times: the
                # keyboard's screen large, the world beside it, no slide.
                self.assertEqual({w[0] for w in wait}, {1}, 'the keyboard is not the large screen throughout')
                self.assertEqual({w[1] for w in wait}, {1}, 'the world beside the screen came and went')
                self.assertEqual({w[2] for w in wait}, {0.0}, 'the picture slid')
                # The panel never fades in and out again (no ping-pong): its alpha only ever rises, or stays.
                alphas = [w[3] for w in wait]
                self.assertTrue(all(b >= a for a, b in zip(alphas, alphas[1:])), 'the panel faded out and back')

    @unittest.skipUnless(ROM and BINARY and Path(ROM).is_file() and Path(BINARY).is_file(),
                         'SSR_TEST_EOS_ROM and SSR_TEST_BINARY required')
    def test_typing_keeps_the_keyboard_large(self):
        # Letters typed (the game stops asking for the keyboard's status for 50 or more frames each time):
        # the keyboard stays the large screen, as on the owner's phone it did not.
        with tempfile.TemporaryDirectory() as folder:
            r = run(folder, '1600x720', typed=(5400, 5500, 5600))
            self.assertEqual(r.returncode, 0, r.stderr[-2000:])
            frames = panel_lines(r.stderr)
            big = {frames[f][0] for f in range(KEYBOARD_FROM, WAIT_UNTIL) if f in frames}
            self.assertEqual(big, {1}, 'the large screen changed while typing')


if __name__ == '__main__':
    unittest.main()
