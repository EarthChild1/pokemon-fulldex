// Pokemon Mystery Dungeon: Explorers of Sky (North America, C2SE): more of a dungeon floor drawn
// beside the screen, for the wide view.
//
// A dungeon floor is drawn on background 3 of the screen the game is played on: a map 512 pixels
// (64 tiles of 8) wide, scrolled with the camera, which the game fills only where the screen
// shows it. In the dungeon overlay (29) UpdateCamera draws the column or row of tiles that comes
// into view as the camera moves (two unnamed functions: a column at the camera's x plus 0 or 255,
// a row of 33 tiles from the camera's x), and a third function draws all 33 visible columns at once
// (on a new floor, after a jump). Beside the screen the map is left as it was: old tiles or none.
//
// Here those three are changed in memory so that they draw 12 more columns on each side
// (57 of the map's 64): the column drawn as the camera moves is the one 96 pixels further out, the
// row and the whole floor start 12 columns to the left and are 24 columns longer. 12 columns
// (96 pixels) are enough for a 20:9 view. The game's tables of a column's dungeon tile reach
// exactly that far for any camera position on a floor, and the tile it looks up for a place off the
// floor (GetTile) is the floor's default; nothing else reads the map. So the screen, the game's
// logic, its saves and its random numbers are as they were: only the map's columns beside the
// screen, which the DS does not show, now hold the floor there.
//
// Each word is checked before it is changed (the overlay is loaded at addresses other overlays
// use too), and the change is made again whenever the overlay is loaded again.
//
// Development aid: SSR_PMD_WIDE=0 leaves the game alone.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "NDS.h"

class PmdWide
{
public:
    // Extra 8-pixel columns drawn on each side of the screen.
    static constexpr int kExtraColumns = 12;

    struct Word
    {
        uint32_t addr, orig, patched;
    };

    // `code`: the ROM's game code. Only the version whose addresses are below.
    void Init(const char* code, bool enabled)
    {
        const char* env = getenv("SSR_PMD_WIDE");
        on = enabled && !(env && !strcmp(env, "0")) && !strncmp(code, "C2SE", 4);
    }

    static const Word* Words(int& n)
    {
        // (ARM code in overlay 29, North America.)
        static const Word w[] = {
            // UpdateCamera: the column drawn when the camera moves right, at x + 352 instead of
            // x + 255 (mov r0, #0x160)...
            { 0x022E31D8, 0xE3A000FF, 0xE3A00F58 },
            // ... and when it moves left, at x - 96 instead of x (mvn r0, #0x5F; mov r1, #0).
            { 0x022E31EC, 0xE3A00000, 0xE3E0005F },
            { 0x022E31F0, 0xE1A01000, 0xE3A01000 },
            // The row drawn when the camera moves up or down: from x - 96 (likewise)...
            { 0x022E3208, 0xE3A00000, 0xE3E0005F },
            { 0x022E321C, 0xE3A00000, 0xE3E0005F },
            { 0x022E3220, 0xE1A01000, 0xE3A01000 },
            // ... 57 tiles long instead of 33 (cmp r0, #0x39).
            { 0x023373FC, 0xE3500021, 0xE3500039 },
            // The whole floor: the first column's dungeon tile 12 columns (4 tiles) to the left
            // (add r0, sb, #0x12: the table index, the column plus 30, less 12)...
            { 0x02336F88, 0xE289001E, 0xE2890012 },
            // ... each column written 12 to the left in the map (where the game only sign-extends
            // a column number already in 0-63: add r0, sb, #0x34; and r0, r0, #0x3F)...
            { 0x02337038, 0xE1A00809, 0xE2890034 },
            { 0x0233703C, 0xE1A00840, 0xE200003F },
            // ... and 57 columns instead of 33 (cmp r0, #0x39).
            { 0x023370EC, 0xE3500021, 0xE3500039 },
        };
        n = (int)(sizeof(w) / sizeof(w[0]));
        return w;
    }

    // Once a frame. Makes the change if the overlay has just been loaded (every word as the game
    // has it). Returns true while the dungeon overlay is in memory with the change made.
    bool Update(melonDS::NDS& nds)
    {
        if (!on) return false;
        int n;
        const Word* w = Words(n);
        int orig = 0, patched = 0;
        for (int i = 0; i < n; i++)
        {
            uint32_t v;
            memcpy(&v, nds.MainRAM + (w[i].addr & nds.MainRAMMask), 4);
            orig += v == w[i].orig;
            patched += v == w[i].patched;
        }
        if (patched == n) return true;
        if (orig != n) return false; // not this overlay (or not all of it as expected): left alone
        // (Through the console's own write, so that code already compiled by the JIT is dropped.)
        for (int i = 0; i < n; i++) nds.ARM9Write32(w[i].addr, w[i].patched);
        patches++;
        return true;
    }

    bool on = false;
    int patches = 0; // times the change was made (each time the overlay was loaded)
};
