#!/usr/bin/env python3
"""Explorers of Sky: looks for glitches in the wide view's margins in composed screenshots.

Usage: check_margins.py <dir> [width height]

<dir> holds frame_*.png written by `ssrecomp ... --compose WxH --shot-every N` (default 1600 x 720, the
owner's phone's 20:9). The DS screen is the middle 4:3 of the picture; the margins are beside it. For each
frame and side it reports:

  black<side>:<rows>  rows where the 40 px of margin next to the screen are near black while the
                      screen's own 40 px edge band is bright (a margin left undrawn)
  jump<side>:<diff>   the margin next to the screen and the screen's edge band differ in brightness by
                      more than 120 (of 765): a fade or a brightness change applied to one but not the other
  flat<side>:<rows>   rows where the whole margin is one colour across while the screen beside it has
                      detail, over more than a quarter of the height (a stretched colour)
  mirror<side>        the margin next to the screen is the screen's edge reflected (visible symmetry)
  seam<side>:<cols>   columns of the margin where the picture changes sharply over 60% of the height
                      (two sources meeting without matching)

Prints nothing for a clean frame; exits 1 if any frame was reported. Screenshots of the game are the
player's own and stay out of the repository.
"""
import glob
import sys

import numpy as np
from PIL import Image


def check(path, width, height):
    lum = np.asarray(Image.open(path).convert('RGB')).astype(int).sum(2)
    screen_w = height * 4 // 3
    left = (width - screen_w) // 2
    right = left + screen_w
    band = max(8, height // 18)
    found = []
    for side, (n0, n1, c0, c1) in enumerate(((left - band, left, left, left + band), (right, right + band, right - band, right))):
        if n0 < 0 or n1 > width:
            continue
        near = lum[:, n0:n1]
        edge = lum[:, c0:c1]
        black = ((near.mean(1) < 15) & (edge.mean(1) > 150)).sum()
        if black > height // 18:
            found.append('black%s:%d' % ('LR'[side], black))
        diff = abs(near.mean() - edge.mean())
        if diff > 120:
            found.append('jump%s:%.0f' % ('LR'[side], diff))

        # The whole margin and the screen's band of the same width next to it.
        m0, m1 = (0, left) if side == 0 else (right, width)
        s0, s1 = (left, left + (m1 - m0)) if side == 0 else (right - (m1 - m0), right)
        margin = lum[:, m0:m1]
        screen = lum[:, s0:s1]
        if margin.shape[1] < 16:
            continue
        # flat: rows where the margin is one colour across (a stretch) while the screen beside it
        # has detail, over more than a quarter of the height.
        flat_rows = ((margin.std(1) < 3) & (screen.std(1) > 12)).sum()
        if flat_rows > height // 4:
            found.append('flat%s:%d' % ('LR'[side], flat_rows))
        # mirror: the margin next to the screen is the screen's edge reflected (a visible symmetry).
        w = min(64, margin.shape[1])
        near_m = lum[:, left - w:left][:, ::-1] if side == 0 else lum[:, right:right + w][:, ::-1]
        near_s = lum[:, left:left + w] if side == 0 else lum[:, right - w:right]
        # (Only where the screen's edge has detail across: horizontal bands of water or sky are the
        # same reflected or not.)
        if near_s.std(1).mean() > 10 and np.abs(near_m - near_s).mean() < 6:
            found.append('mirror%s' % 'LR'[side])
        # seam: a straight vertical line through the margin over most of its height (two sources
        # meeting without matching).
        cols = np.abs(np.diff(margin, axis=1)) > 90
        seams = (cols.mean(0) > 0.6).sum()
        if seams:
            found.append('seam%s:%d' % ('LR'[side], seams))
    return found


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    width, height = (int(sys.argv[2]), int(sys.argv[3])) if len(sys.argv) >= 4 else (1600, 720)
    bad = 0
    for path in sorted(glob.glob(sys.argv[1] + '/frame_*.png')):
        found = check(path, width, height)
        if found:
            bad += 1
            print(path.rsplit('_', 1)[-1][:-4], ' '.join(found))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
