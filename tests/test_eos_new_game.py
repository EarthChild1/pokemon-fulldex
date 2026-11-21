"""A fresh New Game in Explorers of Sky shows something, not a black view.

The personality test opens with lines of text on the touch screen while the top screen is black
("Before you depart for adventure, ..."); with the top screen large the player saw only black and
took the game for stuck. Needs your own ROM and a headless build (not part of the usual run):

  SSR_TEST_EOS_ROM=<Explorers of Sky (US) .nds> SSR_TEST_BINARY=<build>/ssrecomp \
    python -m unittest tests.test_eos_new_game
"""
from pathlib import Path
import os, struct, subprocess, tempfile, unittest, zlib

ROOT = Path(__file__).resolve().parents[1]
ROM = os.environ.get('SSR_TEST_EOS_ROM', '')
BINARY = os.environ.get('SSR_TEST_BINARY', '')


def read_png(path):
    """8-bit RGB or RGBA, not interlaced (what WritePNG writes): (width, height, rows of bytes, bytes a pixel)."""
    data = Path(path).read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    pos, idat, w = 8, b'', 0
    while pos < len(data):
        n, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if kind == b'IHDR':
            w, h, depth, colour = struct.unpack('>IIBB', body[:10])
            assert depth == 8 and colour in (2, 6)
            bpp = 3 if colour == 2 else 4
        elif kind == b'IDAT':
            idat += body
        pos += 12 + n
    raw, stride, rows, prev = zlib.decompress(idat), w * bpp, [], bytearray(w * bpp)
    for y in range(h):
        f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b, c = prev[i], prev[i - bpp] if i >= bpp else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line))
        prev = line
    return w, h, rows, bpp


def bright_pixels(path):
    w, h, rows, bpp = read_png(path)
    return sum(1 for r in rows for x in range(w) if max(r[x * bpp:x * bpp + 3]) > 160)


class EosNewGameTests(unittest.TestCase):
    @unittest.skipUnless(ROM and BINARY and Path(ROM).is_file() and Path(BINARY).is_file(),
                         'SSR_TEST_EOS_ROM and SSR_TEST_BINARY required')
    def test_personality_test_opening_lines_are_shown(self):
        with tempfile.TemporaryDirectory() as folder:
            script = Path(folder) / 'input.txt'
            # From power-on: A every 16 frames (the title, New Game, then the opening lines).
            script.write_text(''.join(f'{a} {a + 3} A\n' for a in range(300, 1700, 16)))
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
            shots = [1350, 1450, 1550]
            r = subprocess.run([BINARY, ROM, '--headless', '--no-save', '--jit', '--funcs', str(ROOT / 'tools/pmd/C2SE.txt'),
                                '--frames', '1600', '--input', str(script), '--shot-dir', folder,
                                '--shot-at', ','.join(map(str, shots)), '--compose', '1600x720'],
                               capture_output=True, text=True, env=env, timeout=600)
            self.assertEqual(r.returncode, 0, r.stderr[-2000:])
            self.assertIn('personality test', r.stderr)
            for f in shots:
                # (The lines are white letters on black: a few hundred bright pixels at 1600x720.)
                self.assertGreater(bright_pixels(Path(folder) / f'frame_{f:06d}.png'), 200, f'frame {f}: a black view')


if __name__ == '__main__':
    unittest.main()
