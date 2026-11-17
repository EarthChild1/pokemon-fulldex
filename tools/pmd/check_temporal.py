#!/usr/bin/env python3
"""Explorers of Sky: looks for glitches over time in the wide view's margins.

Usage: check_temporal.py <dir> [width height]

<dir> holds frame_*.png from `ssrecomp ... --compose WxH --shot-every 1` (every frame; default 1600 x 720).
The DS screen is the middle 4:3; the margins are beside it. For each pair of consecutive frames it
compares how much each margin changed with how much the screen changed, and reports:

  pop<side>:<d>/<c>      the margin changed by d (mean of |dR|+|dG|+|dB|, 0-765) while the screen
                         changed by only c: the sides switched source, or jumped, on a still scene
  flicker<side>:<d>      the margin differs from the frames before and after, which agree with each
                         other: a one-frame glitch

Prints nothing for a clean frame; exits 1 if any frame was reported. Screenshots of the game are the
player's own and stay out of the repository.
"""
import glob
import sys

import numpy as np
from PIL import Image


def load(path, width, height):
    # Quarter size: enough for changes of a few DS pixels (one DS pixel is ~3.75 output pixels at 720).
    im = Image.open(path).convert('RGB').resize((width // 4, height // 4), Image.BOX)
    return np.asarray(im).astype(np.int16)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    width, height = (int(sys.argv[2]), int(sys.argv[3])) if len(sys.argv) >= 4 else (1600, 720)
    files = sorted(glob.glob(sys.argv[1] + '/frame_*.png'))
    w4, h4 = width // 4, height // 4
    screen_w = h4 * 4 // 3
    left = (w4 - screen_w) // 2
    right = left + screen_w
    parts = {'L': (0, left), 'R': (right, w4), 'C': (left, right)}
    prev2 = prev = None
    prev_name = None
    bad = 0
    for path in files:
        cur = load(path, width, height)
        name = path.rsplit('_', 1)[-1][:-4]
        found = []
        if prev is not None:
            c0, c1 = parts['C']
            centre = np.abs(cur[:, c0:c1] - prev[:, c0:c1]).sum(2).mean()
            for side in 'LR':
                a, b = parts[side]
                if b - a < 4:
                    continue
                d = np.abs(cur[:, a:b] - prev[:, a:b]).sum(2).mean()
                if d > 30 and d > 3 * centre + 15:
                    found.append('pop%s:%.0f/%.0f' % (side, d, centre))
        if prev2 is not None:
            for side in 'LR':
                a, b = parts[side]
                if b - a < 4:
                    continue
                around = np.abs(cur[:, a:b] - prev2[:, a:b]).sum(2).mean()
                mid = min(np.abs(prev[:, a:b] - prev2[:, a:b]).sum(2).mean(), np.abs(prev[:, a:b] - cur[:, a:b]).sum(2).mean())
                if mid > 30 and mid > 3 * around + 15:
                    # (Reported for the middle frame, the one that stands out.)
                    print(prev_name, 'flicker%s:%.0f' % (side, mid))
                    bad += 1
        if found:
            bad += 1
            print(name, ' '.join(found))
        prev2, prev, prev_name = prev, cur, name
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
