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

// The layers of engine `unitNum`'s screen that the game's maps account for: out[b] for background b
// (ChunkSource::on). Returns how many were found.
inline int Find(melonDS::NDS& nds, int unitNum, Wide2D::ChunkSource out[4])
{
    for (int b = 0; b < 4; b++) out[b] = Wide2D::ChunkSource();
    out[0].windowCol = kWindowCol;
    out[0].windowCols = kWindowCols;
    Wide2D::Layer layers[4];
    const int n = Wide2D::TextLayers(nds, unitNum, layers);
    Wide2D::BgVram v{ &nds, unitNum };
    int found = 0;
    for (uint32_t ptr : kStatePtrs)
    {
        const uint32_t bg = Word(nds, ptr);
        if (!InRam(bg)) continue;
        for (uint32_t at : kRenderAt)
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
                s.originX = cx * 24 - kWindowCol * 8;
                s.originY = cy * 24;
                // Which background shows it: every entry of the window the same, and not all empty.
                for (int i = 0; i < n; i++)
                {
                    const Wide2D::Layer& l = layers[i];
                    if (l.mapW != 512 || out[l.num].on) continue;
                    int same = 0, filled = 0, total = 0;
                    for (int row = 0; row < kWindowRows; row++)
                        for (int col = kWindowCol; col < kWindowCol + kWindowCols; col++)
                        {
                            uint16_t e;
                            const int tx = cx * 3 + col - kWindowCol, ty = cy * 3 + row;
                            if (!s.Entry(tx, ty, e)) continue;
                            total++;
                            if (e == Wide2D::MapEntry(v, l, col, row))
                            {
                                same++;
                                filled += (e & 0x3FF) != 0;
                            }
                        }
                    if (total >= kWindowCols * kWindowRows / 2 && same * 50 >= total * 49 && filled >= 32)
                    {
                        s.on = true;
                        s.windowCol = kWindowCol;
                        s.windowCols = kWindowCols;
                        out[l.num] = s;
                        found++;
                        break;
                    }
                }
            }
        }
    }
    return found;
}

} // namespace PmdGround
