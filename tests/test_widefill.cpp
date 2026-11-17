// ROM-free: widefill.h carries a map past the end of its picture with the picture's own cells.
// Synthetic maps only (no game data): a periodic texture must be continued in step with its period
// (the one case with a known right answer), the same map must always give the same fill, a mostly
// see-through layer over another must be left clear, and pictures stacked in one map must each be
// continued from themselves.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "../frontend/widefill.h"

namespace
{

constexpr uint32_t kChunkMap = 0x02010000, kChunkTiles = 0x02020000;

// A job for one 16-colour layer whose map is `wTiles` x `hTiles` tiles; tileAt(tx, ty) gives each map
// entry (0: blank). Tile t's pixels are all different from other tiles' (a ramp offset by t).
template <typename F>
WideFill::Job MakeJob(int wTiles, int hTiles, F tileAt, int nTiles)
{
    WideFill::Job job;
    job.ram.assign(0x100000, 0);
    job.ramMask = 0xFFFFF;
    job.vram.assign(0x80000, 0);
    for (int i = 0; i < 16; i++)
    {
        const uint16_t c = (uint16_t)((i * 2) | ((31 - i * 2) << 5) | ((i * 7 % 32) << 10));
        memcpy(job.pal + i * 2, &c, 2);
    }
    for (int t = 1; t <= nTiles; t++)
        for (int py = 0; py < 8; py++)
            for (int px = 0; px < 8; px += 2)
            {
                // (7 and 15 share no factor: tiles 1 to 15 all differ.)
                auto idx = [&](int x) { return 1 + (t * 7 + x * (1 + t % 2) + py * 3) % 15; };
                job.vram[t * 32 + py * 4 + px / 2] = (uint8_t)(idx(px) | idx(px + 1) << 4);
            }
    // Chunks of 3x3 tiles, one chunk number per chunk (64 a row), its 9 entries.
    const int wC = wTiles / 3, hC = hTiles / 3;
    for (int cy = 0; cy < hC; cy++)
        for (int cx = 0; cx < wC; cx++)
        {
            const uint16_t chunk = (uint16_t)(1 + cx + cy * wC);
            memcpy(&job.ram[(kChunkMap + (cx + cy * 64) * 2) & job.ramMask], &chunk, 2);
            for (int k = 0; k < 9; k++)
            {
                const uint16_t e = (uint16_t)tileAt(cx * 3 + k % 3, cy * 3 + k / 3);
                memcpy(&job.ram[(kChunkTiles + (chunk * 9 + k) * 2) & job.ramMask], &e, 2);
            }
        }
    Wide2D::Layer l;
    l.num = 3;
    l.mapW = 512;
    job.layers[0] = l;
    Wide2D::ChunkSource& s = job.src[0];
    s.on = true;
    s.ram = job.ram.data();
    s.mask = job.ramMask;
    s.chunkMap = kChunkMap;
    s.chunkTiles = kChunkTiles;
    s.widthTiles = wTiles;
    s.heightTiles = hTiles;
    job.n = 1;
    job.cols = 12;
    job.seed = 1234;
    return job;
}

int SourceX(uint32_t c) { return (int)(c & 0xFFF); }
int SourceY(uint32_t c) { return (int)((c >> 12) & 0xFFF); }

} // namespace

int main()
{
    // ---- a periodic texture: period 4 across, 3 down; 33 x 27 tiles ----
    {
        auto tile = [](int tx, int ty) { return 1 + tx % 4 + 4 * (ty % 3); };
        WideFill::Job job = MakeJob(33, 27, tile, 12);
        WideFill::Result r;
        WideFill::Build(job, r);
        assert(r.ok && r.pictures == 1);
        const Wide2D::Fill& f = r.fill;
        assert(f.rows == 27 && f.cols >= 12 && f.pics.size() == 1);
        const Wide2D::Fill::Picture& pic = f.pics[0];
        assert(pic.a0 == 0 && pic.a1 == 33 && pic.row0 == 0 && pic.row1 == 27);
        int inStep = 0, total = 0;
        for (int side = 0; side < 2; side++)
            for (int k = 0; k < f.cols; k++)
                for (int y = 0; y < f.rows; y++)
                {
                    const uint32_t c = f.cell[side][(size_t)k * f.rows + y];
                    const int x = side ? pic.a1 + k : pic.a0 - 1 - k; // (the map column it stands for)
                    total++;
                    inStep += !((c >> 24) & 1) && tile(SourceX(c), SourceY(c)) == tile(((x % 4) + 4) % 4, y);
                }
        fprintf(stderr, "periodic: %d of %d cells continue the pattern in step, %d mirrored\n", inStep, total, r.mirrored);
        assert(inStep == total);
        assert(r.mirrored == 0);

        // The same map, the same fill.
        WideFill::Result again;
        WideFill::Build(job, again);
        for (int side = 0; side < 2; side++) assert(again.fill.cell[side] == f.cell[side]);
    }

    // ---- a layer of a few objects over another layer: left clear ----
    {
        auto tile = [](int tx, int ty) { return (tx % 7 == 0 && ty % 5 == 0) ? 1 + (tx + ty) % 4 : 0; };
        WideFill::Job job = MakeJob(36, 27, tile, 4);
        job.othersBehind = true;
        WideFill::Result r;
        WideFill::Build(job, r);
        assert(r.ok && r.sparse);
        for (uint32_t c : r.fill.cell[1]) assert(c & Wide2D::Fill::kClear);
        // Alone (nothing behind it), the same layer is carried on.
        job.othersBehind = false;
        WideFill::Result alone;
        WideFill::Build(job, alone);
        assert(alone.ok && !alone.sparse);
    }

    // ---- two pictures in one map, one above the other, of different widths ----
    {
        // Rows 0-11: a picture across columns 0-32; rows 12-14 blank; rows 15-26: columns 6-26 only.
        auto tile = [](int tx, int ty) {
            if (ty < 12) return 1 + tx % 4 + 4 * (ty % 3);
            if (ty < 15 || tx < 6 || tx >= 27) return 0;
            return 13 + tx % 3;
        };
        WideFill::Job job = MakeJob(33, 27, tile, 15);
        WideFill::Result r;
        WideFill::Build(job, r);
        assert(r.ok && r.pictures == 2);
        const Wide2D::Fill& f = r.fill;
        assert(f.pics[0].a0 == 0 && f.pics[0].a1 == 33 && f.pics[0].row0 == 0 && f.pics[0].row1 == 12);
        assert(f.pics[1].a0 == 6 && f.pics[1].a1 == 27 && f.pics[1].row0 == 15 && f.pics[1].row1 == 27);
        assert(f.rowCount[13] == 0); // (no picture)
        for (int p = 0; p < 2; p++)
            for (int side = 0; side < 2; side++)
                for (int k = 0; k < f.cols; k++)
                    for (int y = f.pics[p].row0; y < f.pics[p].row1; y++)
                    {
                        const uint32_t c = f.cell[side][((size_t)p * f.cols + k) * f.rows + y];
                        // Every cell comes from its own picture.
                        assert(SourceY(c) >= f.pics[p].row0 && SourceY(c) < f.pics[p].row1);
                        assert(SourceX(c) >= f.pics[p].a0 && SourceX(c) < f.pics[p].a1);
                    }
    }

    // ---- two screens side by side in one map (the Nintendo and Chunsoft logos): cut apart ----
    {
        // Columns 0-32: dark tiles 1-12 (palette bank 0); 33-65: bright tiles in palette bank 1.
        auto tile = [](int tx, int ty) { return tx < 33 ? 1 + tx % 4 + 4 * (ty % 3) : (1 + (tx % 3) + 3 * (ty % 2)) | 0x1000; };
        WideFill::Job job = MakeJob(66, 27, tile, 12);
        for (int i = 16; i < 32; i++)
        {
            const uint16_t c = (uint16_t)(0x7FFF - (i - 16)); // (near white)
            memcpy(job.pal + i * 2, &c, 2);
        }
        WideFill::Result r;
        WideFill::Build(job, r);
        assert(r.ok && r.pictures == 2);
        const Wide2D::Fill& f = r.fill;
        assert(f.pics[0].a0 == 0 && f.pics[0].a1 == 33 && f.pics[1].a0 == 33 && f.pics[1].a1 == 66);
        // With the left screen in view, the map past its right edge is its own pattern carried on, not
        // the white screen next to it.
        Wide2D::ChunkSource s = job.src[0];
        s.fill = &r.fill;
        s.viewTx = 16;
        Wide2D::BgVram v;
        v.copy = &job.vram;
        for (int y = 0; y < 27 * 8; y += 7)
            for (int x = 33 * 8; x < 45 * 8; x += 3)
            {
                const int want = Wide2D::TilePixel(v, job.layers[0], (uint16_t)tile((x / 8) % 4, y / 8), x & 7, y & 7);
                assert(Wide2D::SampleChunks(v, job.layers[0], s, x, y) == want);
            }
        // With the right one in view, the map left of it is the white screen's fill (palette bank 1).
        s.viewTx = 50;
        for (int y = 0; y < 27 * 8; y += 7)
            for (int x = 20 * 8; x < 33 * 8; x += 5) assert(Wide2D::SampleChunks(v, job.layers[0], s, x, y) >= 16);
    }

    // ---- wide2d.h draws through the fill: past the picture, the source cell's pixels ----
    {
        auto tile = [](int tx, int ty) { return 1 + tx % 4 + 4 * (ty % 3); };
        WideFill::Job job = MakeJob(33, 27, tile, 12);
        WideFill::Result r;
        WideFill::Build(job, r);
        Wide2D::ChunkSource s = job.src[0];
        s.fill = &r.fill;
        Wide2D::BgVram v;
        v.copy = &job.vram;
        // Map column 33 + 4k + 1 (beyond the picture) must look like map column 1 (same phase).
        for (int y = 0; y < 27 * 8; y += 5)
            for (int x = 33 * 8; x < 33 * 8 + 12 * 8; x += 3)
            {
                const int want = Wide2D::TilePixel(v, job.layers[0], (uint16_t)tile((x / 8) % 4, y / 8), x & 7, y & 7);
                assert(Wide2D::SampleChunks(v, job.layers[0], s, x, y) == want);
            }
    }

    printf("widefill: all checks passed\n");
    return 0;
}
