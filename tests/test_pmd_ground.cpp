// ROM-free: Explorers of Sky's town and cutscene maps in RAM (pmdground.h). Synthetic maps only (no game
// data): the hardware map is checked against the window the game names, and against the window a chunk to
// each side, since the game moves the window's chunk some frames before it copies the new window into the
// hardware map (on the personality test's naming screen 22 frames before, every 48 frames; the world beside
// the screen then went away for those frames and the layout jumped twice a second).
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "../frontend/pmdground.h"

namespace
{

constexpr uint32_t kChunkMap = 0x02010000, kChunkTiles = 0x02020000;
constexpr int kMapChunksW = 16, kMapChunksH = 9;

// A map of 16 x 9 chunks in a RAM picture: chunk c's 9 tiles are numbered 1 + c * 9 + i, so every map
// entry is different from every other (no window looks like its neighbour).
struct Ram
{
    std::vector<uint8_t> bytes = std::vector<uint8_t>(0x400000);
    void W16(uint32_t a, uint16_t v) { memcpy(bytes.data() + (a & 0x3FFFFF), &v, 2); }
    Ram()
    {
        for (int cy = 0; cy < kMapChunksH; cy++)
            for (int cx = 0; cx < kMapChunksW; cx++)
            {
                const int c = cx + cy * kMapChunksW;
                W16(kChunkMap + (cx + cy * 64) * 2, (uint16_t)c);
                for (int i = 0; i < 9; i++) W16(kChunkTiles + (c * 9 + i) * 2, (uint16_t)((1 + c * 9 + i) & 0x3FF));
            }
    }
    Wide2D::ChunkSource Source() const
    {
        Wide2D::ChunkSource s;
        s.ram = bytes.data();
        s.mask = 0x3FFFFF;
        s.chunkMap = kChunkMap;
        s.chunkTiles = kChunkTiles;
        s.widthTiles = kMapChunksW * 3;
        s.heightTiles = kMapChunksH * 3;
        s.wrap = 1;
        return s;
    }
};

// A 512 x 256 hardware map holding the window that starts at chunk wx, wy, from column 2, as the game's
// renderer copies it.
void Copy(const Wide2D::ChunkSource& s, const Wide2D::Layer& l, std::vector<uint8_t>& vram, int wx, int wy)
{
    std::fill(vram.begin(), vram.end(), 0);
    for (int row = 0; row < PmdGround::kWindowRows; row++)
        for (int col = PmdGround::kWindowCol; col < PmdGround::kWindowCol + PmdGround::kWindowCols; col++)
        {
            uint16_t e = 0;
            s.Entry(wx * 3 + col - PmdGround::kWindowCol, wy * 3 + row, e);
            const int tx = col & 63, ty = row & 31;
            const uint32_t block = (tx >> 5) + (ty >> 5) * (l.mapW >> 8);
            memcpy(vram.data() + l.mapBase + block * 0x800 + (((ty & 31) << 5) + (tx & 31)) * 2, &e, 2);
        }
}

} // namespace

int main()
{
    Ram ram;
    const Wide2D::ChunkSource s = ram.Source();
    Wide2D::Layer l;
    l.num = 2;
    l.mapW = 512;
    l.mapH = 256;
    l.mapBase = 0x4000;
    std::vector<uint8_t> vram(0x10000);
    Wide2D::BgVram v;
    v.copy = &vram;
    int wx = -99, wy = -99;

    // The hardware map holds the window the game names: that one.
    Copy(s, l, vram, 5, 0);
    assert(PmdGround::WindowMatches(s, v, l, 5, 0));
    assert(PmdGround::NearWindow(s, v, l, 5, 0, wx, wy) && wx == 5 && wy == 0);

    // The game has moved on a chunk (right, then left) and not yet copied the window: the one it held.
    assert(!PmdGround::WindowMatches(s, v, l, 6, 0));
    assert(PmdGround::NearWindow(s, v, l, 6, 0, wx, wy) && wx == 5 && wy == 0);
    assert(PmdGround::NearWindow(s, v, l, 4, 0, wx, wy) && wx == 5 && wy == 0);
    // ... or down, or up.
    Copy(s, l, vram, 5, 3);
    assert(PmdGround::NearWindow(s, v, l, 5, 4, wx, wy) && wx == 5 && wy == 3);
    assert(PmdGround::NearWindow(s, v, l, 5, 2, wx, wy) && wx == 5 && wy == 3);

    // Two chunks off, or diagonally: not taken (that is not the game catching up).
    Copy(s, l, vram, 5, 3);
    assert(!PmdGround::NearWindow(s, v, l, 7, 3, wx, wy));
    assert(!PmdGround::NearWindow(s, v, l, 6, 4, wx, wy));

    // Another picture in the hardware map (a scene being loaded): nothing.
    for (size_t i = 0x4000; i < 0x6000; i += 2) vram[i] = (uint8_t)(i * 7), vram[i + 1] = 0;
    assert(!PmdGround::NearWindow(s, v, l, 5, 3, wx, wy));

    // An empty map is never taken, even where it matches (at least 32 entries not empty).
    std::vector<uint8_t> zero(0x400000);
    Wide2D::ChunkSource empty = s;
    empty.ram = zero.data();
    std::fill(vram.begin(), vram.end(), 0);
    assert(!PmdGround::NearWindow(empty, v, l, 5, 3, wx, wy));

    printf("pmd ground: ok\n");
    return 0;
}
