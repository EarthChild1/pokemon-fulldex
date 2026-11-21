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
// - Cells are only taken from rows near their own (kRowReach), so sky stays sky and ground ground,
//   and from the part of the picture next to that side (kColPart), so a painting's subject stays in it.
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
constexpr int kColPart = 4;      // and from the picture's outer part on that side: this fraction of its width (at least 4 cells)
constexpr int kMirrorCost = 54;  // the mirror's cost, per compared pixel (the sum of |dR|+|dG|+|dB|)
constexpr int kRepeatCost = 24; // per compared pixel: the same cell as the one inwards or above
constexpr int kAlphaCost = 64;  // per pixel where one side has art and the other only the backdrop
// Busyness over a cell's neighbour's, divided by this, per compared pixel (0: not counted). Off: on the
// island it kept trees out of the water as it kept glyphs off the copyright screens' black, and the
// copyright screens' sides are rarely in view.
constexpr int kBusyDivisor = 0;
constexpr int kPlainBusy = 6;     // a cell this busy or less is plain ground
constexpr int kPlainPercent = 85; // a picture this much plain ground is carried on with plain cells only
constexpr int kMinPicture = 28; // cells: a picture side by side with another is at least a screen wide
constexpr int kMinCutRows = 8; // cell rows: a boundary between pictures side by side runs at least this far
constexpr int kStickyFrames = 30; // frames a layer keeps its fill while its group's is not ready
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
// per pixel whether a layer draws there (kAlphaCost, or 0 where the backdrop shows), and per cell
// whether any layer draws in it.
inline void Composite(const Job& job, int W, int H, std::vector<uint8_t>& rgb, std::vector<uint8_t>& alpha, std::vector<uint8_t>& opaque)
{
    const int Wc = W / 8, Hc = H / 8;
    rgb.assign((size_t)W * H * 3, 0);
    alpha.assign((size_t)W * H, 0);
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
                        alpha[((size_t)(ty * 8 + py)) * W + tx * 8 + px] = kAlphaCost;
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
    std::vector<uint8_t> rgb, alpha, opaque;
    Composite(job, W, H, rgb, alpha, opaque);

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

    // The pictures. A tall map can hold several, one above the other, of different widths (the intro's
    // fountain, sky and island are one map), separated by blank rows: each run of rows with art has its
    // own columns, the first to the last any layer draws in. A wide map can hold several side by side
    // (the Nintendo and Chunsoft screens are one 792-pixel map, the camera moving from one to the next):
    // where, at a cell boundary, nearly every row of the run changes sharply (white to black), and the
    // pieces either side are at least kMinPicture cells wide (a screen), the run is cut there. Each piece
    // is a picture of its own: carried on from itself only, and the one under the screen's middle is the
    // one shown (beside it, its fill rather than the next picture of the map).
    std::vector<Wide2D::Fill::Picture> pics;
    {
        std::vector<int> rowA0(Hc, Wc), rowA1(Hc, 0);
        for (int ty = 0; ty < Hc; ty++)
            for (int tx = 0; tx < Wc; tx++)
                if (opaque[(size_t)ty * Wc + tx])
                {
                    rowA0[ty] = std::min(rowA0[ty], tx);
                    rowA1[ty] = tx + 1;
                }
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
            if (s1 - s0 >= 4)
            {
                // Per cell row: whether column boundary tx divides two pictures there (all but one of
                // its 8 pixel rows change sharply, and the 32 pixels either side differ on average:
                // two pictures, not the edges of tiles inside one repeating texture), and by how much.
                // Screens can sit in a grid (the logos: 3 across, 2 down), so a boundary may run
                // through only some rows; it counts on a run of at least kMinCutRows cell rows.
                const int rowsN = end - ty, span = s1 - s0;
                std::vector<int> apartAt((size_t)rowsN * span, 0); // 0: no boundary there
                for (int y = ty; y < end; y++)
                    for (int tx = s0 + kMinPicture; tx <= s1 - kMinPicture; tx++)
                    {
                        int sharp = 0, apart = 0;
                        for (int py = y * 8; py < y * 8 + 8; py++)
                        {
                            const size_t row = (size_t)py * W, a = row + tx * 8 - 1, b = a + 1;
                            const int d = std::abs(rgb[a * 3] - rgb[b * 3]) + std::abs(rgb[a * 3 + 1] - rgb[b * 3 + 1]) + std::abs(rgb[a * 3 + 2] - rgb[b * 3 + 2]);
                            sharp += d > 96 || alpha[a] != alpha[b];
                            int l = 0, rr = 0;
                            for (int k = 1; k <= 32; k++)
                            {
                                const size_t p = row + std::max(0, tx * 8 - k), q = row + std::min(W - 1, tx * 8 + k - 1);
                                l += rgb[p * 3] + rgb[p * 3 + 1] + rgb[p * 3 + 2];
                                rr += rgb[q * 3] + rgb[q * 3 + 1] + rgb[q * 3 + 2];
                            }
                            apart += std::abs(l - rr) / 32;
                        }
                        if (sharp >= 7 && apart >= 8 * 120) apartAt[(size_t)(y - ty) * span + (tx - s0)] = apart / 8;
                    }
                // (Only runs of kMinCutRows or more.)
                for (int tx = s0; tx < s1; tx++)
                    for (int y = 0; y < rowsN;)
                    {
                        if (!apartAt[(size_t)y * span + (tx - s0)]) { y++; continue; }
                        int e = y;
                        while (e < rowsN && apartAt[(size_t)e * span + (tx - s0)]) e++;
                        if (e - y < kMinCutRows)
                            for (int k = y; k < e; k++) apartAt[(size_t)k * span + (tx - s0)] = 0;
                        y = e;
                    }
                // Each row's cuts, the strongest first, each kept if it leaves every piece a screen wide;
                // rows with the same cuts one after another are one picture each between cuts.
                std::vector<int> prevCuts;
                int runStart = ty;
                auto flush = [&](int runEnd) {
                    for (size_t i = 0; i + 1 < prevCuts.size(); i++)
                        pics.push_back({ (int16_t)prevCuts[i], (int16_t)prevCuts[i + 1], (int16_t)runStart, (int16_t)runEnd });
                };
                for (int y = ty; y < end; y++)
                {
                    std::vector<std::pair<int, int>> cand; // (how far apart the two sides are, column)
                    for (int tx = s0; tx < s1; tx++)
                        if (const int a = apartAt[(size_t)(y - ty) * span + (tx - s0)]) cand.push_back({ a, tx });
                    std::sort(cand.begin(), cand.end(), [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
                    std::vector<int> cuts = { s0, s1 };
                    for (const auto& c : cand)
                    {
                        bool roomy = true;
                        for (int x : cuts) roomy &= std::abs(c.second - x) >= kMinPicture;
                        if (roomy) cuts.push_back(c.second);
                    }
                    std::sort(cuts.begin(), cuts.end());
                    if (y > ty && cuts != prevCuts)
                    {
                        flush(y);
                        runStart = y;
                    }
                    prevCuts = cuts;
                }
                flush(end);
            }
            ty = end;
        }
        r.pictures = (int)pics.size();
    }
    if (pics.empty() || pics.size() > 64) return;
    int blank = 0;
    for (const auto& p : pics) blank = std::max(blank, std::max((int)p.a0, Wc - p.a1));
    const int cols = std::min(kMaxCols, job.cols + blank);

    // Per cell of the picture: its left and right columns and bottom row, 8 pixels of RGB each.
    const size_t cells = (size_t)Wc * Hc;
    // (32 bytes: 8 pixels of R, G, B, and whether a layer draws there, weighted by kAlphaCost: opaque
    // art is not carried on into see-through cells that only show the backdrop, or the reverse.)
    std::vector<uint8_t> leftCol(cells * 32), rightCol(cells * 32), botRow(cells * 32);
    for (int ty = 0; ty < Hc; ty++)
        for (int tx = 0; tx < Wc; tx++)
        {
            const size_t c = (size_t)ty * Wc + tx;
            for (int k = 0; k < 8; k++)
            {
                const size_t pl = ((size_t)(ty * 8 + k)) * W + tx * 8, pr = pl + 7, pb = ((size_t)(ty * 8 + 7)) * W + tx * 8 + k;
                memcpy(&leftCol[c * 32 + k * 4], &rgb[pl * 3], 3);
                memcpy(&rightCol[c * 32 + k * 4], &rgb[pr * 3], 3);
                memcpy(&botRow[c * 32 + k * 4], &rgb[pb * 3], 3);
                leftCol[c * 32 + k * 4 + 3] = alpha[pl];
                rightCol[c * 32 + k * 4 + 3] = alpha[pr];
                botRow[c * 32 + k * 4 + 3] = alpha[pb];
            }
        }
    // Per cell, how busy it is: the mean of |dR|+|dG|+|dB| from its own average colour. Matching looks
    // only at a cell's neighbours, so on a plain background every cell fits, glyphs and logos too (the
    // copyright screens): see plainPic below. And its average colour.
    std::vector<uint16_t> busy(cells, 0);
    std::vector<uint32_t> meanOf(cells, 0);
    for (int ty = 0; ty < Hc; ty++)
        for (int tx = 0; tx < Wc; tx++)
        {
            int m[3] = {};
            for (int py = 0; py < 8; py++)
                for (int px = 0; px < 8; px++)
                    for (int ch = 0; ch < 3; ch++) m[ch] += rgb[(((size_t)(ty * 8 + py)) * W + tx * 8 + px) * 3 + ch];
            int dev = 0;
            for (int py = 0; py < 8; py++)
                for (int px = 0; px < 8; px++)
                    for (int ch = 0; ch < 3; ch++) dev += std::abs(rgb[(((size_t)(ty * 8 + py)) * W + tx * 8 + px) * 3 + ch] - m[ch] / 64);
            busy[(size_t)ty * Wc + tx] = (uint16_t)(dev / 64);
            meanOf[(size_t)ty * Wc + tx] = (uint32_t)(m[0] / 64) << 16 | (uint32_t)(m[1] / 64) << 8 | (uint32_t)(m[2] / 64);
        }
    auto sad = [](const uint8_t* a, const uint8_t* b, bool reverseB) {
        int s = 0;
        if (!reverseB)
            for (int i = 0; i < 32; i++) s += std::abs((int)a[i] - (int)b[i]);
        else
            for (int k = 0; k < 8; k++)
                for (int ch = 0; ch < 4; ch++) s += std::abs((int)a[k * 4 + ch] - (int)b[(7 - k) * 4 + ch]);
        return s;
    };
    // A placed cell (source, mirrored or not): the column it shows on the given side, its bottom row.
    auto colOf = [&](uint32_t p, int side) -> const uint8_t* {
        const size_t c = (size_t)((p >> 12) & 0xFFF) * Wc + (p & 0xFFF);
        const bool mir = (p >> 24) & 1;
        return &((side == 1) != mir ? rightCol : leftCol)[c * 32];
    };

    Wide2D::Fill& f = r.fill;
    f.cols = cols;
    f.rows = Hc;
    f.pics = pics;
    f.rowFirst.assign(Hc, 0);
    f.rowCount.assign(Hc, 0); // (rows of no picture: none, the map as it is)
    for (size_t i = 0; i < pics.size(); i++)
        for (int y = pics[i].row0; y < pics[i].row1; y++)
        {
            if (!f.rowCount[y]) f.rowFirst[y] = (uint8_t)i;
            f.rowCount[y]++;
        }
    auto at = [&](int p, int k, int y) { return ((size_t)p * cols + k) * Hc + y; };
    for (int side = 0; side < 2; side++) f.cell[side].assign(pics.size() * cols * Hc, Wide2D::Fill::kClear);
    // Layers that are mostly see-through (objects laid over a picture on other layers: the
    // treasure scene's items) are not carried on: copies of the objects beside the screen read as
    // debris. Clear there, so the picture behind (its own fill) shows.
    {
        int filledCells = 0, area = 0;
        for (const auto& p : pics)
            for (int ty = p.row0; ty < p.row1; ty++)
                for (int tx = p.a0; tx < p.a1; tx++)
                {
                    filledCells += opaque[(size_t)ty * Wc + tx];
                    area++;
                }
        r.sparse = job.othersBehind && filledCells * 100 < area * kSparsePercent;
        if (r.sparse)
        {
            r.ok = true;
            r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            return;
        }
    }
    for (int p = 0; p < (int)pics.size(); p++)
    {
        const int ra0 = pics[p].a0, ra1 = pics[p].a1, row0 = pics[p].row0, row1 = pics[p].row1;
        // A picture that is nearly all plain ground (the logo and copyright screens: text and a logo
        // on black) is carried on with plain cells only: anything else beside it is pieces of its
        // letters. A scene with texture over a dark ground (the forest, the island) has far more
        // busy cells than this and is carried on in full.
        // (Plain ground: calm cells of the picture's most common colour, not a smooth gradient such as
        // Dialga's sky, whose clouds must carry on.)
        bool plainPic = false;
        uint32_t ground = 0;
        auto near = [](uint32_t a, uint32_t b) {
            return std::abs((int)(a >> 16) - (int)(b >> 16)) + std::abs((int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF)) +
                std::abs((int)(a & 0xFF) - (int)(b & 0xFF)) <= 24;
        };
        {
            std::vector<int> bins(4096, 0);
            for (int y = row0; y < row1; y++)
                for (int x = ra0; x < ra1; x++)
                {
                    const uint32_t c = meanOf[(size_t)y * Wc + x];
                    bins[((c >> 20) & 0xF) << 8 | ((c >> 12) & 0xF) << 4 | ((c >> 4) & 0xF)]++;
                }
            const int top = (int)(std::max_element(bins.begin(), bins.end()) - bins.begin());
            ground = (uint32_t)(((top >> 8) & 0xF) * 16 + 8) << 16 | (uint32_t)(((top >> 4) & 0xF) * 16 + 8) << 8 | (uint32_t)((top & 0xF) * 16 + 8);
            int plain = 0;
            for (int y = row0; y < row1; y++)
                for (int x = ra0; x < ra1; x++) plain += busy[(size_t)y * Wc + x] <= kPlainBusy && near(meanOf[(size_t)y * Wc + x], ground);
            plainPic = plain * 100 >= (ra1 - ra0) * (row1 - row0) * kPlainPercent;
        }
        for (int side = 0; side < 2; side++)
        {
            const int dir = side ? 1 : -1;
            std::vector<uint32_t>& out = f.cell[side];
            for (int k = 0; k < cols; k++)
                for (int y = row0; y < row1; y++)
                {
                    // The cell inwards (the picture's edge cell for the first column) and the one above.
                    const uint32_t inner = k ? out[at(p, k - 1, y)] : (uint32_t)(side ? ra1 - 1 : ra0) | (uint32_t)y << 12;
                    const uint8_t* want = colOf(inner, side);
                    const bool hasAbove = y > row0;
                    const uint32_t above = hasAbove ? out[at(p, k, y - 1)] : 0;
                    const bool aboveMir = (above >> 24) & 1;
                    const uint8_t* wantBot = hasAbove ? &botRow[((size_t)((above >> 12) & 0xFFF) * Wc + (above & 0xFFF)) * 32] : nullptr;
                    const int pixels = hasAbove ? 16 : 8;

                    // (Only from the part of the picture next to this side: what is at its edge goes on, not
                    // what is in its middle. A painting's subject, the night fountain's jets and steps, carried on
                    // beside it read as broken copies and flat blocks.)
                    const int reach = std::max(4, (ra1 - ra0) / kColPart);
                    // The mirror: that part of the picture reflected about its edge (for k past it, reflected
                    // again, so it never runs out).
                    uint32_t best = 0;
                    int bestCost = 1 << 30;
                    {
                        const int span = std::min(reach, ra1 - ra0);
                        int m = k % (2 * span);
                        if (m >= span) m = 2 * span - 1 - m;
                        const bool mirAgain = (k / span) & 1;
                        const int mx = side ? ra1 - 1 - m : ra0 + m;
                        best = (uint32_t)mx | (uint32_t)y << 12 | (uint32_t)(!mirAgain) << 24;
                        bestCost = kMirrorCost * pixels;
                    }
                    // Only from this picture, from rows near this one, with the cell inwards (and above,
                    // when matched against the one placed above) in the picture too.
                    const int y0 = std::max(hasAbove ? row0 + 1 : row0, y - kRowReach), y1 = std::min(row1 - 1, y + kRowReach);
                    const int x0 = side ? std::max(ra0 + 1, ra1 - reach) : ra0, x1 = side ? ra1 : std::min(ra1 - 1, ra0 + reach);
                    for (int sy = y0; sy <= y1; sy++)
                        for (int sx = x0; sx < x1; sx++)
                        {
                            if (plainPic && (busy[(size_t)sy * Wc + sx] > kPlainBusy || !near(meanOf[(size_t)sy * Wc + sx], ground))) continue;
                            // The candidate's own neighbour inwards, in the picture, against the placed one.
                            const size_t ctx = (size_t)sy * Wc + (sx - dir);
                            int cost = sad(want, &(side ? rightCol : leftCol)[ctx * 32], false);
                            if (cost >= bestCost) continue;
                            if (hasAbove)
                            {
                                const size_t up = (size_t)(sy - 1) * Wc + sx;
                                cost += sad(wantBot, &botRow[up * 32], aboveMir);
                                if (cost >= bestCost) continue;
                            }
                            // The same cell again beside or under itself reads as a stamped pattern (a
                            // line of identical stars): only if nothing else will do.
                            {
                                const uint32_t here = (uint32_t)sx | (uint32_t)sy << 12;
                                if ((inner & 0x1FFFFFF) == here || (hasAbove && (above & 0x1FFFFFF) == here) ||
                                    (k > 1 && (out[at(p, k - 2, y)] & 0x1FFFFFF) == here))
                                    cost += kRepeatCost * pixels;
                                if (cost >= bestCost) continue;
                            }
                            // (A cell much busier than its own neighbour inwards: a glyph or a logo's edge on
                            // a plain ground, not a texture, whose cells are all about as busy.)
                            if (kBusyDivisor)
                            {
                                const int bc = busy[(size_t)sy * Wc + sx], bn = busy[ctx];
                                if (bc > 2 * bn + 8) cost += (bc - bn) * pixels / (kBusyDivisor ? kBusyDivisor : 1);
                            }
                            if (cost >= bestCost) continue;
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
                    out[at(p, k, y)] = best;
                    r.mirrored += (best >> 24) & 1;
                    r.cells++;
                }
        }
    }
    r.ok = true;
    r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    // Diagnostics (SSR_WIDE_FILL_DUMP=<folder>): the whole map with, outside it, the fill of the
    // leftmost and rightmost picture of each row, fill_<w>x<h>_<seed>.ppm. Mirrored cells are tinted
    // red, rows of no picture blue, the cuts between pictures side by side green.
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
                if (!f.rowCount[ty]) tint = 2;
                else if (tx < 0 || tx >= Wc)
                {
                    const int side = tx >= Wc;
                    const int p = side ? f.rowFirst[ty] + f.rowCount[ty] - 1 : f.rowFirst[ty];
                    const int k = side ? tx - pics[p].a1 : pics[p].a0 - 1 - tx;
                    if (k >= cols) continue;
                    const uint32_t c = f.cell[side][at(p, k, ty)];
                    if (c & Wide2D::Fill::kClear) continue;
                    const bool mir = (c >> 24) & 1;
                    sx = (int)(c & 0xFFF) * 8 + (mir ? 7 - (mx & 7) : mx & 7);
                    sy = (int)((c >> 12) & 0xFFF) * 8 + (y & 7);
                    tint = mir ? 1 : 0;
                }
                else
                    for (int i = f.rowFirst[ty]; i < f.rowFirst[ty] + f.rowCount[ty]; i++)
                        if (i > f.rowFirst[ty] && mx == pics[i].a0 * 8) tint = 3;
                if (sx < 0 || sx >= W) continue;
                uint8_t* o = &img[((size_t)y * ow + x) * 3];
                memcpy(o, &rgb[((size_t)sy * W + sx) * 3], 3);
                if (tint == 1) o[0] = (uint8_t)std::min(255, o[0] + 80);
                if (tint == 2) o[2] = (uint8_t)std::min(255, o[2] + 80);
                if (tint == 3) { o[0] = 0; o[1] = 255; o[2] = 0; }
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
        int tries = 0;             // builds started (the palette's retry is logged on the first)
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
    bool wasTogether[4][4] = {}; // (layers grouped the frame before, by background number)
    // Per background number, the last fill it was given (kept alive here), for its map and tiles.
    struct Sticky
    {
        std::shared_ptr<Result> res;
        uint64_t mapHash = 0;
        std::vector<uint16_t> sig;
        int frame = -1000;
    };
    Sticky sticky[4];

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
        // Groups: layers whose maps share a size and a placement are one picture (BG2 over BG3, from
        // one of the game's map renderers or two: the forest's foliage and its dark are two). Layers
        // grouped the frame before stay grouped while their windows are a chunk apart: the game moves
        // them on a frame apart, which split them for that frame and left the fill out. (The treasure
        // scene's items and painting are a chunk apart throughout: never grouped.)
        bool done[4] = {};
        bool together[4][4] = {};
        for (int g = 0; g < 2; g++)
        {
            int members[4], n = 0;
            const Wide2D::ChunkSource* first = nullptr;
            int firstBg = -1;
            for (int i = 0; i < nAll; i++)
            {
                const int b = all[i].num;
                const Wide2D::ChunkSource& s = src[b];
                if (!s.on || s.wrap || done[i]) continue;
                if (!first) { first = &s; firstBg = b; }
                else
                {
                    if (s.widthTiles != first->widthTiles || s.heightTiles != first->heightTiles) continue;
                    const int dx = std::abs(s.originX - first->originX), dy = std::abs(s.originY - first->originY);
                    if ((dx || dy) && !(wasTogether[firstBg][b] && dx <= 24 && dy <= 24)) continue;
                    together[firstBg][b] = together[b][firstBg] = true;
                }
                members[n++] = i;
                done[i] = true;
            }
            if (!n) break;
            uint64_t key = Mix(extend, n), memberHash[4];
            for (int m = 0; m < n; m++)
            {
                memberHash[m] = Hash(src[all[members[m]].num]);
                key = Mix(key, memberHash[m] + m);
            }
            // And the tiles' pictures: full-screen paintings number their tiles the same way (1, 2,
            // 3, ...), so two scenes can share a map and differ only in video memory. Sampled, and
            // told apart only when much of it differs: animated tiles (a waterfall) change a little
            // of it every few frames, and must not make a new fill each time.
            std::vector<uint16_t> sig, memberSig[4];
            {
                Wide2D::BgVram v{ &nds, unitNum };
                for (int m = 0; m < n; m++)
                {
                    const Wide2D::Layer& l = all[members[m]];
                    const uint32_t size = l.c256 ? 0x10000 : 0x8000;
                    for (uint32_t a = l.charBase; a < l.charBase + size; a += 16) memberSig[m].push_back(v.R16(a) ^ v.R16(a + 6) ^ v.R16(a + 10));
                    sig.insert(sig.end(), memberSig[m].begin(), memberSig[m].end());
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
            bool given = false;
            if (hit)
            {
                hit->lastUsed = frame;
                Result& r = *hit->res;
                if (r.done.load(std::memory_order_acquire))
                {
                    if (r.ok)
                    {
                        for (int m = 0; m < n; m++)
                        {
                            const int b = all[members[m]].num;
                            src[b].fill = &r.fill;
                            sticky[b] = { hit->res, memberHash[m], memberSig[m], frame };
                        }
                        given = true;
                    }
                    else if (r.retry && frame >= hit->retryAt)
                    {
                        if (!hit->retryAt) hit->retryAt = frame + 15;
                        else Start(nds, unitNum, all, nAll, members, n, src, extend, key, *hit);
                    }
                }
            }
            // No fill ready for this group: a layer keeps the fill it had, a little while, as long as
            // its own map and tiles are the same. (A layer missed for a frame, or two layers' windows
            // briefly a chunk together or apart, changes the group, not the picture.)
            if (!given)
                for (int m = 0; m < n; m++)
                {
                    const int b = all[members[m]].num;
                    const Sticky& st = sticky[b];
                    if (st.res && frame - st.frame <= kStickyFrames && st.mapHash == memberHash[m] && Similar(st.sig, memberSig[m]))
                        src[b].fill = &st.res->fill;
                }
        }
        memcpy(wasTogether, together, sizeof(together));
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
        const int startFrame = frame, tries = ++g.tries;
        std::thread([job, res, startFrame, tries]() {
            Build(*job, *res);
            if (res->ok)
                fprintf(stderr, "[widefill] map %dx%d (from frame %d): %d pictures, %d cells a side, %d of %d mirrored%s, %.1f ms\n",
                    res->w, res->h, startFrame, res->pictures, res->fill.cols, res->mirrored, res->cells, res->sparse ? " (objects: left clear)" : "", res->ms);
            else if (res->retry && tries == 1)
                fprintf(stderr, "[widefill] map %dx%d (from frame %d): palette not up yet, later\n", res->w, res->h, startFrame);
            res->done.store(true, std::memory_order_release);
        }).detach();
    }
};

} // namespace WideFill
