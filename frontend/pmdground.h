// Pokemon Mystery Dungeon: Explorers of Sky (North America, C2SE): the maps of its towns,
// cutscenes and title screen (ground mode), for drawing them beside the screen (wide2d.h).
//
// Ground mode keeps each map whole in main RAM, as 3x3-tile chunks: a chunk map 64 chunks across
// (one u16 chunk number each) and the chunks' tiles (9 u16 map entries each), for up to two layers.
// Its renderer (overlay 11, 0x022EED98) copies a window of 12 x 9 chunks round the camera's chunk
// into the hardware's 512-pixel map, from column 2 (pixel 16), and scrolls within it; the rest of
// the hardware map is never written. So beside the screen the hardware map holds nothing, while the
// game's own map does: here it is read from there instead.
//
// What is read (pmdsky-debug names where it has them): GROUND_STATE_MAP (0x02324CC0) and
// GROUND_STATE_WEATHER (0x02324CC4) point to a struct ground_bg each; at 0x210 and 0x264 of it a
// renderer's state (pmdsky-debug's map_render, longer than its header has it): +6 the map repeats
// across (wrap_around), +8 and +C its width and height in chunks, +1C and +20 the chunks' tiles of
// layers 0 and 1, +24 and +28 their chunk maps, +44 and +48 the chunk the window starts at.
//
// None of that is trusted on its own: a layer is only taken for a hardware background whose map,
// in the window the game drew, holds exactly the entries the layer gives there (98% of them, checked every
// frame, so a scene being loaded, another overlay at those addresses, or a different game version
// is simply not used). Nothing is written.
#pragma once
#include <cstdint>
#include <cstring>
#include "NDS.h"
#include "wide2d.h"

namespace PmdGround
{

constexpr uint32_t kStatePtrs[2] = { 0x02324CC0, 0x02324CC4 };
constexpr uint32_t kRenderAt[2] = { 0x210, 0x264 };
constexpr int kWindowCol = 2, kWindowCols = 36, kWindowRows = 27;

inline uint32_t Word(const melonDS::NDS& nds, uint32_t a)
{
    uint32_t v;
    memcpy(&v, nds.MainRAM + (a & nds.MainRAMMask & ~3u), 4);
    return v;
}

inline bool InRam(uint32_t a) { return a >= 0x02000000 && a < 0x02400000; }

// Where to look for the window the game drew: the chunk it names first, then a chunk to each side
// (see Find).
constexpr int kNearWindows[5][2] = { { 0, 0 }, { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };

// Whether background `l`'s hardware map holds, in the window the game draws (from column kWindowCol),
// exactly what layer `s` gives with the window starting at chunk wx, wy: 98% of the entries the same,
// and at least 32 of them not empty.
inline bool WindowMatches(const Wide2D::ChunkSource& s, const Wide2D::BgVram& v, const Wide2D::Layer& l, int wx, int wy)
{
    int same = 0, filled = 0, total = 0;
    for (int row = 0; row < kWindowRows; row++)
        for (int col = kWindowCol; col < kWindowCol + kWindowCols; col++)
        {
            uint16_t e;
            const int tx = wx * 3 + col - kWindowCol, ty = wy * 3 + row;
            if (!s.Entry(tx, ty, e)) continue;
            total++;
            if (e == Wide2D::MapEntry(v, l, col, row))
            {
                same++;
                filled += (e & 0x3FF) != 0;
            }
        }
    return total >= kWindowCols * kWindowRows / 2 && same * 50 >= total * 49 && filled >= 32;
}

// The window, of kNearWindows round chunk cx, cy, that background `l` holds for layer `s`: its chunk
// in wx, wy. False if none.
inline bool NearWindow(const Wide2D::ChunkSource& s, const Wide2D::BgVram& v, const Wide2D::Layer& l, int cx, int cy, int& wx, int& wy)
{
    for (const int (&d)[2] : kNearWindows)
        if (WindowMatches(s, v, l, cx + d[0], cy + d[1]))
        {
            wx = cx + d[0];
            wy = cy + d[1];
            return true;
        }
    return false;
}

// The layers of engine `unitNum`'s screen that the game's maps account for: out[b] for background b
// (ChunkSource::on). Returns how many were found. `prev`: a copy of last frame's, or null.
inline int Find(melonDS::NDS& nds, int unitNum, Wide2D::ChunkSource out[4], const Wide2D::ChunkSource* prev = nullptr)
{
    for (int b = 0; b < 4; b++) out[b] = Wide2D::ChunkSource();
    out[0].windowCol = kWindowCol;
    out[0].windowCols = kWindowCols;
    Wide2D::Layer layers[4];
    const int n = Wide2D::TextLayers(nds, unitNum, layers);
    Wide2D::BgVram v{ &nds, unitNum };
    int found = 0;
    for (const uint32_t& ptr : kStatePtrs)
    {
        const uint32_t bg = Word(nds, ptr);
        if (!InRam(bg)) continue;
        for (const uint32_t& at : kRenderAt)
        {
            const uint32_t r = bg + at;
            const int wChunks = (int)Word(nds, r + 8), hChunks = (int)Word(nds, r + 0xC);
            const int cx = (int)Word(nds, r + 0x44), cy = (int)Word(nds, r + 0x48);
            if (wChunks < 1 || wChunks > 64 || hChunks < 1 || hChunks > 64 || cx < -64 || cx > 64 || cy < -64 || cy > 64) continue;
            for (int layer = 0; layer < 2; layer++)
            {
                Wide2D::ChunkSource s;
                s.ram = nds.MainRAM;
                s.mask = nds.MainRAMMask;
                s.chunkTiles = Word(nds, r + 0x1C + 4 * layer);
                s.chunkMap = Word(nds, r + 0x24 + 4 * layer);
                if (!InRam(s.chunkTiles) || !InRam(s.chunkMap)) continue;
                s.wrap = (Word(nds, r + 4) >> 16) & 0xFF;
                s.widthTiles = wChunks * 3;
                s.heightTiles = hChunks * 3;
                // Which background shows it: every entry of the window the same, and not all empty.
                // (The window the game names first, then the one a chunk to each side: the game moves
                // the window's chunk before it copies the new window into the hardware map, on the
                // personality test's naming screen 22 frames before, its backdrop a chunk further every
                // 48 frames. Until then the hardware map holds the window a chunk back, and the world
                // beside the screen comes from that one; without it the world went away for those
                // frames and the layout jumped twice a second.)
                for (int i = 0; i < n; i++)
                {
                    const Wide2D::Layer& l = layers[i];
                    int wx, wy;
                    if (l.mapW != 512 || out[l.num].on || !NearWindow(s, v, l, cx, cy, wx, wy)) continue;
                    s.on = true;
                    s.originX = wx * 24 - kWindowCol * 8;
                    s.originY = wy * 24;
                    s.renderer = (int)(&ptr - kStatePtrs) * 2 + (int)(&at - kRenderAt);
                    s.windowCol = kWindowCol;
                    s.windowCols = kWindowCols;
                    out[l.num] = s;
                    found++;
                    break;
                }
            }
        }
    }
    // A background none of them accounts for, that had a source last frame (`prev`, a copy): that one,
    // if the hardware map still holds what it gives. When a scene changes its backdrop the renderer
    // names the new map a frame or two before the hardware has it, and meanwhile the screen still
    // shows the old one; without this the world went away for those frames (the sides black, or a band,
    // for a frame). A map in RAM that has been written over no longer matches, and is let go.
    if (prev)
        for (int i = 0; i < n; i++)
        {
            const Wide2D::Layer& l = layers[i];
            const Wide2D::ChunkSource& p = prev[l.num];
            if (l.mapW != 512 || out[l.num].on || !p.on || p.ram != nds.MainRAM) continue;
            const int wx = (p.originX + kWindowCol * 8) / 24, wy = p.originY / 24;
            if (!WindowMatches(p, v, l, wx, wy)) continue;
            Wide2D::ChunkSource keep = p;
            keep.viewTx = INT32_MIN; // (set again as it is drawn, as for a source found this frame)
            keep.distrusted = false;
            keep.fill = nullptr;
            out[l.num] = keep;
            found++;
        }
    return found;
}

} // namespace PmdGround
