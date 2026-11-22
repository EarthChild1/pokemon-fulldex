#!/usr/bin/env python3
"""Function table for Pokemon Mystery Dungeon: Explorers of Sky (North America, C2SE).

Writes the few functions frontend/pmd.h watches to tell the game's modes apart (the dungeon,
its menus, ground mode, the personality test, the naming keyboard), in frontend/funcwatch.h's
format:

    <address> a <module> <first bytes> <name>

Overlay functions have ? for their first bytes: they are read from the player's ROM when the
game starts (FuncWatch::FillFirstBytes), so the table holds addresses only and no game code.

The addresses come from pmdsky-debug (https://github.com/UsernameFodder/pmdsky-debug), whose
symbol tables are under the MIT licence (symbols/LICENSE.txt there):

    python3 tools/pmd/funcmap.py <pmdsky-debug>/symbols tools/pmd/C2SE.txt

The table made from it is kept in the repository (tools/pmd/C2SE.txt), so a build needs neither
pmdsky-debug nor a network connection.
"""
import re
import sys
from pathlib import Path

# The functions watched, and the symbol file each is in (overlayNN.yml: module ovNN).
WATCHED = [
    ("overlay29", "AdvanceFrame"),                # the dungeon, every frame
    ("overlay29", "DisplayMonstersAndItems"),     # the dungeon's picture
    ("overlay29", "UpdateCamera"),                # the dungeon's camera
    ("overlay31", "DungeonMenuLoop"),             # the dungeon's main menu
    ("overlay31", "StairsMenuLoop"),              # the stairs' question
    ("overlay11", "RunNextOpcode"),               # ground mode: its scripts (towns, the guild, cutscenes)
    ("overlay11", "FuncThatCallsRunNextOpcode"),
    ("overlay13", "PersonalityTestFrameUpdate"),  # the personality test, every frame
    ("arm9", "GetKeyboardStatus"),                # the naming keyboard (on the touch screen), every frame it is up
]


def na_address(text, name):
    """The NA address of function `name` in a pmdsky-debug symbol file (the first one if a list)."""
    m = re.search(r"^    - name: " + re.escape(name) + r"\n((?:      .*\n)+)", text, re.M)
    if not m:
        raise SystemExit(f"{name}: not in the symbol file")
    block = m.group(1)
    a = re.search(r"^        NA:\s*\[?\s*(0x[0-9A-Fa-f]+)", block, re.M)
    if not a:
        raise SystemExit(f"{name}: no NA address")
    return int(a.group(1), 16)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    symbols, out = Path(sys.argv[1]), Path(sys.argv[2])
    lines = [
        "# Pokemon Mystery Dungeon: Explorers of Sky (North America, C2SE): functions frontend/pmd.h watches.",
        "# Made by tools/pmd/funcmap.py from pmdsky-debug's symbol tables (MIT licence,",
        "# https://github.com/UsernameFodder/pmdsky-debug). Addresses only: an overlay function's first",
        "# bytes (?) are read from the player's ROM.",
    ]
    for file, name in WATCHED:
        text = (symbols / f"{file}.yml").read_text(encoding="utf-8")
        addr = na_address(text, name)
        if addr & 1:
            raise SystemExit(f"{name}: odd address {addr:#x} (Thumb functions are not expected here)")
        module = "ov" + file[len("overlay"):] if file.startswith("overlay") else "main"
        first = "?" if module != "main" else "-"
        lines.append(f"{addr:08x} a {module} {first} {name}")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{len(WATCHED)} functions -> {out}")


if __name__ == "__main__":
    main()
