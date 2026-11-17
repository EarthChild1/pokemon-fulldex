// The picture of a scene carried on past its end, from the scene's own art (Explorers of Sky's
// towns, cutscenes, intro paintings and title, whose maps are often no wider than the DS shows).
//
// Where the game has no more picture (a map that stops, or blank to its edge), wide2d.h used to fill
// the side of the view with the soft colour of the picture's last columns: on a fixed painting that
// reads as stretched colour. Here the map is continued with its own 8x8 cells instead, chosen the way
// texture synthesis continues a texture (Ashikhmin-style neighbourhood matching on the 8-pixel grid):
//
// - Outwards from the picture's edge, one column of cells at a time, top to bottom, each new cell is
//   a cell of the picture whose own neighbours look like the cells already placed beside it (the
//   facing column of the cell inwards, the bottom row of the cell above). A cell that continues the
//   same piece of picture as its neighbours matches exactly, so whole stretches of art carry on
//   together and only break where they must; the pixels are never blended or recoloured.
// - Cells are only taken from rows near their own (kRowReach), so sky stays sky and ground ground.
// - Where no cell fits well (a busy edge), the picture mirrored about its edge, cell for cell, is
//   used instead (kMirrorCost): the smart mirror the cascade falls back on.
// - Ties are broken by a hash of the scene's map, so a scene always gets the same fill, frame to frame
//   and run to run.
//
// The result names map cells (Wide2D::Fill), not colours: each frame wide2d.h draws those cells with
// that frame's palette, tile animation, scroll and brightness, so fades and effects stay uniform with
// the screen. It is built once per map (keyed by a hash of the map's chunk tables), on a worker
// thread from a copy of the console's memory, and kept for the last few maps.
#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>
#include "NDS.h"
#include "GPU2D_Soft.h"
#include "wide2d.h"

namespace WideFill
{

constexpr int kRowReach = 6;     // cells taken from at most this many rows above or below
constexpr int kMirrorCost = 54;  // the mirror's cost, per compared pixel (the sum of |dR|+|dG|+|dB|)
constexpr int kRepeatCost = 24; // per compared pixel: the same cell as the one inwards or above
constexpr int kMaxCols = 40;    // cells beside the picture on each side, at most
constexpr int kSparsePercent = 55; // a layer drawing in fewer of its picture's cells: objects, see Build

// What a build needs, copied from the console so the worker never reads live memory.
struct Job
{
    std::vector<uint8_t> ram;
    uint32_t ramMask = 0;
    std::vector<uint8_t> vram; // background video memory, indexed by address & (size - 1)
    uint8_t pal[512] = {};
    Wide2D::Layer layers[4];
    Wide2D::ChunkSource src[4];
    int n = 0;
    int cols = 0;
    uint64_t seed = 0;
    bool othersBehind = false; // another layer with a map of its own lies behind these
};

struct Result
{
    std::atomic<bool> done{ false };
    bool ok = false, retry = false, sparse = false;
    Wide2D::Fill fill;
    double ms = 0;
    int mirrored = 0, cells = 0, w = 0, h = 0, pictures = 0;
};

inline uint32_t Mix(uint64_t a, uint64_t b)
{
    uint64_t h = a * 0x9E3779B97F4A7C15ull ^ (b + 0x632BE59BD9B4E019ull + (a << 6) + (a >> 2));
    h ^= h >> 31;
    h *= 0xBF58476D1CE4E5B9ull;
    return (uint32_t)(h ^ (h >> 29));
}

// The map composited as the layers stack (front first), with the palette copied: RGB per pixel,
// and per cell whether any layer draws in it.
inline void Composite(const Job& job, int W, int H, std::vector<uint8_t>& rgb, std::vector<uint8_t>& opaque)
{
    const int Wc = W / 8, Hc = H / 8;
    rgb.assign((size_t)W * H * 3, 0);
    opaque.assign((size_t)Wc * Hc, 0);
    Wide2D::BgVram v;
    v.copy = &job.vram;
    auto colour = [&](int idx, uint8_t* o) {
        uint16_t c;
        memcpy(&c, job.pal + idx * 2, 2);
        o[0] = (uint8_t)(((c & 0x1F) << 3) | ((c >> 2) & 7));
        o[1] = (uint8_t)((((c >> 5) & 0x1F) << 3) | ((c >> 7) & 7));
        o[2] = (uint8_t)((((c >> 10) & 0x1F) << 3) | ((c >> 12) & 7));
    };
    uint8_t backdrop[3];
    colour(0, backdrop);
    for (int ty = 0; ty < Hc; ty++)
        for (int tx = 0; tx < Wc; tx++)
        {
            uint16_t e[4];
            bool has[4];
            for (int i = 0; i < job.n; i++) has[i] = job.src[i].Entry(tx, ty, e[i]);
            for (int py = 0; py < 8; py++)
                for (int px = 0; px < 8; px++)
                {
                    uint8_t* o = &rgb[(((size_t)(ty * 8 + py)) * W + tx * 8 + px) * 3];
                    int idx = 0;
                    for (int i = 0; i < job.n && !idx; i++)
                        if (has[i]) idx = Wide2D::TilePixel(v, job.layers[i], e[i], px, py);
                    if (idx)
                    {
                        colour(idx, o);
                        opaque[(size_t)ty * Wc + tx] = 1;
                    }
                    else
                        memcpy(o, backdrop, 3);
                }
        }
}

inline void Build(const Job& job, Result& r)
{
    const auto t0 = std::chrono::steady_clock::now();
    const Wide2D::ChunkSource& s0 = job.src[0];
    const int Wc = s0.widthTiles, Hc = s0.heightTiles, W = Wc * 8, H = Hc * 8;
    r.w = W;
    r.h = H;
    std::vector<uint8_t> rgb, opaque;
    Composite(job, W, H, rgb, opaque);

    // A palette still black (or white) while the scene loads gives nothing to match: build again later.
    {
        uint32_t seen[4];
        int distinct = 0;
        for (size_t i = 0; i < rgb.size() && distinct < 4; i += 3 * 7)
        {
            const uint32_t c = rgb[i] << 16 | rgb[i + 1] << 8 | rgb[i + 2];
            if (std::find(seen, seen + distinct, c) == seen + distinct) seen[distinct++] = c;
        }
        if (distinct < 3)
        {
            r.retry = true;
            return;
        }
    }

    // The pictures: a tall map can hold several, one above the other, of different widths (the intro's
    // fountain, sky and island are one map), separated by blank rows. Each run of rows with art is a
    // picture with its own columns, the first to the last any layer draws in; cells are only taken
    // from the picture they continue.
    std::vector<int> seg(Hc, -1), a0r(Hc, 0), a1r(Hc, 0);
    {
        std::vector<int> rowA0(Hc, Wc), rowA1(Hc, 0);
        for (int ty = 0; ty < Hc; ty++)
            for (int tx = 0; tx < Wc; tx++)
                if (opaque[(size_t)ty * Wc + tx])
                {
                    rowA0[ty] = std::min(rowA0[ty], tx);
                    rowA1[ty] = tx + 1;
                }
        int id = 0;
        for (int ty = 0; ty < Hc;)
        {
            if (rowA1[ty] <= rowA0[ty]) { ty++; continue; }
            int end = ty, s0 = Wc, s1 = 0;
            while (end < Hc && rowA1[end] > rowA0[end])
            {
                s0 = std::min(s0, rowA0[end]);
                s1 = std::max(s1, rowA1[end]);
                end++;
            }
            for (int y = ty; y < end; y++)
            {
                seg[y] = s1 - s0 >= 4 ? id : -1;
                a0r[y] = s0;
                a1r[y] = s1;
            }
            id++;
            ty = end;
        }
        r.pictures = id;
    }
    int a0 = Wc, a1 = 0, blank = 0;
    for (int y = 0; y < Hc; y++)
        if (seg[y] >= 0)
        {
            a0 = std::min(a0, a0r[y]);
            a1 = std::max(a1, a1r[y]);
            blank = std::max(blank, std::max(a0r[y], Wc - a1r[y]));
        }
    if (a1 <= a0) return;
    const int cols = std::min(kMaxCols, job.cols + blank);

    // Per cell of the picture: its left and right columns and bottom row, 8 pixels of RGB each.
    const size_t cells = (size_t)Wc * Hc;
    std::vector<uint8_t> leftCol(cells * 24), rightCol(cells * 24), botRow(cells * 24);
    for (int ty = 0; ty < Hc; ty++)
        for (int tx = 0; tx < Wc; tx++)
        {
            const size_t c = (size_t)ty * Wc + tx;
            for (int k = 0; k < 8; k++)
            {
                memcpy(&leftCol[c * 24 + k * 3], &rgb[(((size_t)(ty * 8 + k)) * W + tx * 8) * 3], 3);
                memcpy(&rightCol[c * 24 + k * 3], &rgb[(((size_t)(ty * 8 + k)) * W + tx * 8 + 7) * 3], 3);
                memcpy(&botRow[c * 24 + k * 3], &rgb[(((size_t)(ty * 8 + 7)) * W + tx * 8 + k) * 3], 3);
            }
        }
    auto sad = [](const uint8_t* a, const uint8_t* b, bool reverseB) {
        int s = 0;
        if (!reverseB)
            for (int i = 0; i < 24; i++) s += std::abs((int)a[i] - (int)b[i]);
        else
            for (int k = 0; k < 8; k++)
                for (int ch = 0; ch < 3; ch++) s += std::abs((int)a[k * 3 + ch] - (int)b[(7 - k) * 3 + ch]);
        return s;
    };
    // A placed cell (source, mirrored or not): the column it shows on the given side, its bottom row.
    auto colOf = [&](uint32_t p, int side) -> const uint8_t* {
        const size_t c = (size_t)((p >> 12) & 0xFFF) * Wc + (p & 0xFFF);
        const bool mir = (p >> 24) & 1;
        return &((side == 1) != mir ? rightCol : leftCol)[c * 24];
    };
    auto inPicture = [&](int tx, int ty) { return tx >= a0r[ty] && tx < a1r[ty]; };

    Wide2D::Fill& f = r.fill;
    f.cols = cols;
    f.rows = Hc;
    f.rowA0.assign(Hc, 0);
    f.rowA1.assign(Hc, 0); // (rows of no picture: none, the map as it is)
    for (int y = 0; y < Hc; y++)
        if (seg[y] >= 0)
        {
            f.rowA0[y] = (int16_t)a0r[y];
            f.rowA1[y] = (int16_t)a1r[y];
        }
    // Layers that are mostly see-through (objects laid over a picture on other layers: the
    // treasure scene's items) are not carried on: copies of the objects beside the screen read as
    // debris. Clear there, so the picture behind (its own fill) shows.
    {
        int filledCells = 0, area = 0;
        for (int ty = 0; ty < Hc; ty++)
            if (seg[ty] >= 0)
            {
                for (int tx = a0r[ty]; tx < a1r[ty]; tx++) filledCells += opaque[(size_t)ty * Wc + tx];
                area += a1r[ty] - a0r[ty];
            }
        r.sparse = job.othersBehind && filledCells * 100 < area * kSparsePercent;
        if (r.sparse)
        {
            for (int side = 0; side < 2; side++) f.cell[side].assign((size_t)cols * Hc, Wide2D::Fill::kClear);
            r.ok = true;
            r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            return;
        }
    }
    for (int side = 0; side < 2; side++)
    {
        const int dir = side ? 1 : -1;
        std::vector<uint32_t>& out = f.cell[side];
        out.assign((size_t)cols * Hc, 0);
        for (int k = 0; k < cols; k++)
            for (int y = 0; y < Hc; y++)
            {
                if (seg[y] < 0)
                {
                    out[(size_t)k * Hc + y] = Wide2D::Fill::kClear;
                    continue;
                }
                const int ra0 = a0r[y], ra1 = a1r[y];
                // The cell inwards (the picture's edge cell for the first column) and the one above.
                const uint32_t inner = k ? out[(size_t)(k - 1) * Hc + y]
                                         : (uint32_t)(side ? ra1 - 1 : ra0) | (uint32_t)y << 12;
                const uint8_t* want = colOf(inner, side);
                const bool hasAbove = y > 0 && seg[y - 1] == seg[y];
                const uint32_t above = hasAbove ? out[(size_t)k * Hc + y - 1] : 0;
                const bool aboveMir = (above >> 24) & 1;
                const uint8_t* wantBot = hasAbove ? &botRow[((size_t)((above >> 12) & 0xFFF) * Wc + (above & 0xFFF)) * 24] : nullptr;
                const int pixels = hasAbove ? 16 : 8;

                // The mirror: the picture reflected about its edge (for k past the picture's width,
                // reflected again, so it never runs out).
                uint32_t best = 0;
                int bestCost = 1 << 30;
                {
                    const int span = ra1 - ra0;
                    int m = k % (2 * span);
                    if (m >= span) m = 2 * span - 1 - m;
                    const bool mirAgain = (k / span) & 1;
                    const int mx = side ? ra1 - 1 - m : ra0 + m;
                    best = (uint32_t)mx | (uint32_t)y << 12 | (uint32_t)(!mirAgain) << 24;
                    bestCost = kMirrorCost * pixels;
                }
                const int y0 = std::max(0, y - kRowReach), y1 = std::min(Hc - 1, y + kRowReach);
                for (int sy = y0; sy <= y1; sy++)
                {
                    // (Only from the same picture, and with a cell above it in that picture when
                    // matched against the one placed above.)
                    if (seg[sy] != seg[y] || (hasAbove && (sy == 0 || seg[sy - 1] != seg[y]))) continue;
                    for (int sx = a0r[sy]; sx < a1r[sy]; sx++)
                    {
                        if (!inPicture(sx - dir, sy) || (hasAbove && !inPicture(sx, sy - 1))) continue;
                        // The candidate's own neighbour inwards, in the picture, against the placed one.
                        const size_t ctx = (size_t)sy * Wc + (sx - dir);
                        int cost = sad(want, &(side ? rightCol : leftCol)[ctx * 24], false);
                        if (cost >= bestCost) continue;
                        if (hasAbove)
                        {
                            const size_t up = (size_t)(sy - 1) * Wc + sx;
                            cost += sad(wantBot, &botRow[up * 24], aboveMir);
                            if (cost >= bestCost) continue;
                        }
                        // The same cell again beside or under itself reads as a stamped pattern (a
                        // line of identical stars): only if nothing else will do.
                        {
                            const uint32_t here = (uint32_t)sx | (uint32_t)sy << 12;
                            if ((inner & 0x1FFFFFF) == here || (hasAbove && (above & 0x1FFFFFF) == here) ||
                                (k > 1 && (out[(size_t)(k - 2) * Hc + y] & 0x1FFFFFF) == here))
                                cost += kRepeatCost * pixels;
                            if (cost >= bestCost) continue;
                        }
                        // Ties: the scene's own hash, and a slight preference for rows near its own.
                        cost = cost * 4 + (int)(Mix(job.seed, ((uint64_t)side << 48) | ((uint64_t)k << 32) | ((uint64_t)y << 16) | (uint64_t)(sy * Wc + sx)) & 3)
                            + std::abs(sy - y);
                        cost /= 4;
                        if (cost < bestCost)
                        {
                            bestCost = cost;
                            best = (uint32_t)sx | (uint32_t)sy << 12;
                        }
                    }
                }
                out[(size_t)k * Hc + y] = best;
                r.mirrored += (best >> 24) & 1;
                r.cells++;
            }
    }
    r.ok = true;
    r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    // Diagnostics (SSR_WIDE_FILL_DUMP=<folder>): the whole map with its fill on both sides, as built,
    // fill_<w>x<h>_<seed>.ppm. Mirrored cells are tinted red, rows of no picture blue.
    if (const char* dir = getenv("SSR_WIDE_FILL_DUMP"))
    {
        const int ow = W + 2 * cols * 8;
        std::vector<uint8_t> img((size_t)ow * H * 3, 0);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < ow; x++)
            {
                const int mx = x - cols * 8, ty = y / 8;
                int sx = mx, sy = y, tint = 0;
                const int tx = Wide2D::Tile8(mx);
                if (seg[ty] < 0) tint = 2;
                else if (tx < f.rowA0[ty] || tx >= f.rowA1[ty])
                {
                    const int side = tx >= f.rowA1[ty];
                    const int k = side ? tx - f.rowA1[ty] : f.rowA0[ty] - 1 - tx;
                    if (k >= cols) continue;
                    const uint32_t c = f.cell[side][(size_t)k * Hc + ty];
                    if (c & Wide2D::Fill::kClear) continue;
                    const bool mir = (c >> 24) & 1;
                    sx = (int)(c & 0xFFF) * 8 + (mir ? 7 - (mx & 7) : mx & 7);
                    sy = (int)((c >> 12) & 0xFFF) * 8 + (y & 7);
                    tint = mir ? 1 : 0;
                }
                if (sx < 0 || sx >= W) continue;
                uint8_t* o = &img[((size_t)y * ow + x) * 3];
                memcpy(o, &rgb[((size_t)sy * W + sx) * 3], 3);
                if (tint == 1) o[0] = (uint8_t)std::min(255, o[0] + 80);
                if (tint == 2) o[2] = (uint8_t)std::min(255, o[2] + 80);
            }
        char path[512];
        snprintf(path, sizeof(path), "%s/fill_%dx%d_%08x.ppm", dir, W, H, (unsigned)job.seed);
        if (FILE* fp = fopen(path, "wb"))
        {
            fprintf(fp, "P6\n%d %d\n255\n", ow, H);
            fwrite(img.data(), 1, img.size(), fp);
            fclose(fp);
        }
    }
}

// Keeps the fills of the last few maps and builds the current one's, on a worker thread.
struct Manager
{
    struct Group
    {
        uint64_t key = 0;
        std::shared_ptr<Result> res;
        int lastUsed = 0;
        int retryAt = 0;
        std::vector<uint16_t> sig; // the tiles' pictures, sampled (see Update)
    };
    std::vector<Group> cache;
    uint64_t lastKey[2] = {};
    std::vector<uint16_t> lastSig[2];

    // Two samples of the tiles' pictures from the same scene: at most a quarter differs.
    static bool Similar(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b)
    {
        if (a.size() != b.size()) return false;
        size_t diff = 0;
        for (size_t i = 0; i < a.size(); i++) diff += a[i] != b[i];
        return diff * 4 <= a.size();
    }
    int frame = 0;
    bool off = getenv("SSR_WIDE_FILL") && !strcmp(getenv("SSR_WIDE_FILL"), "0");
    bool debug = getenv("SSR_WIDE_FILL_DEBUG") != nullptr;

    static uint64_t Hash(const Wide2D::ChunkSource& s)
    {
        uint64_t h = Mix(s.widthTiles, s.heightTiles) ^ Mix(s.chunkMap, s.chunkTiles) ^ (uint64_t)s.wrap;
        const int wC = s.widthTiles / 3, hC = s.heightTiles / 3;
        for (int cy = 0; cy < hC; cy++)
            for (int cx = 0; cx < wC; cx++)
            {
                const uint16_t chunk = s.R16(s.chunkMap + (cx + cy * 64) * 2);
                h = Mix(h, chunk);
                for (int i = 0; i < 9; i += 4) h = Mix(h, s.R16(s.chunkTiles + (chunk * 9 + i) * 2));
            }
        return h;
    }

    // Sets src[b].fill for the layers of engine `unitNum` found by PmdGround::Find. `extend`: the
    // columns drawn beside the screen.
    void Update(melonDS::NDS& nds, int unitNum, Wide2D::ChunkSource src[4], int extend)
    {
        frame++;
        if (off) return;
        Wide2D::Layer all[4];
        const int nAll = Wide2D::TextLayers(nds, unitNum, all);
        // Groups: layers whose maps share a size and a placement are one picture (BG2 over BG3).
        bool done[4] = {};
        for (int g = 0; g < 2; g++)
        {
            int members[4], n = 0;
            const Wide2D::ChunkSource* first = nullptr;
            for (int i = 0; i < nAll; i++)
            {
                const Wide2D::ChunkSource& s = src[all[i].num];
                if (!s.on || s.wrap || done[i]) continue;
                if (!first) first = &s;
                else if (s.widthTiles != first->widthTiles || s.heightTiles != first->heightTiles ||
                         s.originX != first->originX || s.originY != first->originY) continue;
                members[n++] = i;
                done[i] = true;
            }
            if (!n) break;
            uint64_t key = Mix(extend, n);
            for (int m = 0; m < n; m++) key = Mix(key, Hash(src[all[members[m]].num]) + m);
            // And the tiles' pictures: full-screen paintings number their tiles the same way (1, 2,
            // 3, ...), so two scenes can share a map and differ only in video memory. Sampled, and
            // told apart only when much of it differs: animated tiles (a waterfall) change a little
            // of it every few frames, and must not make a new fill each time.
            std::vector<uint16_t> sig;
            {
                Wide2D::BgVram v{ &nds, unitNum };
                for (int m = 0; m < n; m++)
                {
                    const Wide2D::Layer& l = all[members[m]];
                    const uint32_t size = l.c256 ? 0x10000 : 0x8000;
                    for (uint32_t a = l.charBase; a < l.charBase + size; a += 16) sig.push_back(v.R16(a) ^ v.R16(a + 6) ^ v.R16(a + 10));
                }
            }
            if (debug && frame % 10 == 0)
            {
                size_t diff = 0;
                for (size_t i = 0; i < sig.size() && sig.size() == lastSig[g].size(); i++) diff += sig[i] != lastSig[g][i];
                fprintf(stderr, "[widefill] frame %d group %d: %dx%d tiles, key %016llx (last %s), tiles changed %zu of %zu\n", frame, g,
                    first->widthTiles, first->heightTiles, (unsigned long long)key, key == lastKey[g] ? "same" : "other", diff, sig.size());
            }
            // (A map is built for once it reads the same two frames running: not while it loads.)
            const bool stable = key == lastKey[g] && Similar(sig, lastSig[g]);
            lastKey[g] = key;
            lastSig[g] = sig;
            Group* hit = nullptr;
            for (Group& c : cache)
                if (c.key == key && Similar(c.sig, sig)) hit = &c;
            if (!hit && stable)
            {
                if (cache.size() >= 6)
                    cache.erase(std::min_element(cache.begin(), cache.end(), [](const Group& a, const Group& b) { return a.lastUsed < b.lastUsed; }));
                cache.push_back(Group());
                hit = &cache.back();
                hit->key = key;
                hit->sig = sig;
                Start(nds, unitNum, all, nAll, members, n, src, extend, key, *hit);
            }
            if (!hit) continue;
            hit->lastUsed = frame;
            Result& r = *hit->res;
            if (!r.done.load(std::memory_order_acquire)) continue;
            if (r.ok)
            {
                for (int m = 0; m < n; m++) src[all[members[m]].num].fill = &r.fill;
            }
            else if (r.retry && frame >= hit->retryAt)
            {
                if (!hit->retryAt) hit->retryAt = frame + 15;
                else Start(nds, unitNum, all, nAll, members, n, src, extend, key, *hit);
            }
        }
    }

    void Start(melonDS::NDS& nds, int unitNum, const Wide2D::Layer* all, int nAll, const int* members, int n, const Wide2D::ChunkSource* src,
        int extend, uint64_t key, Group& g)
    {
        auto job = std::make_shared<Job>();
        job->ram.assign(nds.MainRAM, nds.MainRAM + nds.MainRAMMask + 1);
        job->ramMask = nds.MainRAMMask;
        job->vram.assign(unitNum == 0 ? 0x80000 : 0x20000, 0);
        Wide2D::BgVram live{ &nds, unitNum };
        for (int m = 0; m < n; m++)
        {
            const Wide2D::Layer& l = all[members[m]];
            job->layers[m] = l;
            job->src[m] = src[l.num];
            job->src[m].ram = job->ram.data();
            job->src[m].mask = job->ramMask;
            job->src[m].fill = nullptr;
            const uint32_t size = l.c256 ? 0x10000 : 0x8000;
            for (uint32_t a = l.charBase; a < l.charBase + size; a += 2)
            {
                const melonDS::u16 v = live.R16(a);
                memcpy(&job->vram[a & (job->vram.size() - 1)], &v, 2);
            }
        }
        job->n = n;
        for (int i = members[n - 1] + 1; i < nAll; i++)
            if (all[i].num != all[members[0]].num && src[all[i].num].on) job->othersBehind = true;
        job->cols = (extend + 7) / 8 + 1;
        job->seed = key;
        auto* renderer = static_cast<melonDS::GPU2D::SoftRenderer*>(&nds.GPU.GetRenderer2D());
        memcpy(job->pal, renderer->PalSnap[unitNum], sizeof(job->pal));
        auto res = std::make_shared<Result>();
        g.res = res;
        g.retryAt = 0;
        const int startFrame = frame;
        std::thread([job, res, startFrame]() {
            Build(*job, *res);
            if (res->ok)
                fprintf(stderr, "[widefill] map %dx%d (from frame %d): %d pictures, %d cells a side, %d of %d mirrored%s, %.1f ms\n",
                    res->w, res->h, startFrame, res->pictures, res->fill.cols, res->mirrored, res->cells, res->sparse ? " (objects: left clear)" : "", res->ms);
            else if (res->retry)
                fprintf(stderr, "[widefill] map %dx%d (from frame %d): palette not up yet, later\n", res->w, res->h, startFrame);
            res->done.store(true, std::memory_order_release);
        }).detach();
    }
};

} // namespace WideFill
