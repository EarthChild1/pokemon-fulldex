// A 2D screen drawn wider than the DS shows it: its tiled background layers carried on beyond the
// left and right edges of the screen from their maps in video memory, and its sprites drawn where
// they stand past those edges, as the 2D engine would draw them on a wider screen. For a game whose
// world is drawn in 2D (Pokemon Mystery Dungeon: Explorers of Sky's dungeons and towns): the parts
// of the map the game has already put in video memory beside the screen are shown there.
//
// The screen itself stays exactly as the game drew it; only the columns beside it are drawn here.
// Nothing in the console's state is changed.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "NDS.h"
#include "GPU2D_Soft.h"

namespace Wide2D
{

struct Layer
{
    int num = 0, prio = 0;
    bool c256 = false;
    int mapW = 256, mapH = 256;
    melonDS::u32 charBase = 0, mapBase = 0;
};

// The tiled (text) background layers that are on, front to back. Empty when the screen is not
// drawn the usual way. A 256-colour layer is left out when extended palettes are on (not handled
// here; 16-colour layers do not use them).
inline int TextLayers(melonDS::NDS& nds, int unitNum, Layer out[4])
{
    using namespace melonDS;
    const GPU2D::Unit& unit = unitNum == 0 ? nds.GPU.GPU2D_A : nds.GPU.GPU2D_B;
    const u32 dispcnt = unit.DispCnt;
    if (((dispcnt >> 16) & 3) != 1) return 0;
    const bool extPal = dispcnt & (1u << 30);
    const int mode = dispcnt & 7;
    int n = 0;
    for (int prio = 0; prio < 4; prio++)
        for (int b = 0; b < 4; b++)
        {
            const u16 cnt = unit.BGCnt[b];
            if (!(dispcnt & (0x100 << b)) || (cnt & 3) != prio) continue;
            if (b == 0 && (dispcnt & 8)) continue; // the 3D layer
            if ((b == 2 && mode > 1) || (b == 3 && mode > 0)) continue; // rotating or bitmap
            if (extPal && (cnt & 0x80)) continue;
            Layer& l = out[n++];
            l.num = b;
            l.prio = prio;
            l.c256 = cnt & 0x80;
            l.mapW = (cnt & 0x4000) ? 512 : 256;
            l.mapH = (cnt & 0x8000) ? 512 : 256;
            l.charBase = ((cnt >> 2) & 0xF) * 0x4000;
            l.mapBase = ((cnt >> 8) & 0x1F) * 0x800;
            if (unitNum == 0)
            {
                l.charBase += ((dispcnt >> 24) & 7) * 0x10000;
                l.mapBase += ((dispcnt >> 27) & 7) * 0x10000;
            }
        }
    return n;
}

// The layers of a screen that hold its text and windows rather than a world: tiled layers that are
// on and whose map is the screen's size (256 wide), as a mask of LayerClass bits (bit n: BG n). The
// world layers of a game that scrolls a larger picture are 512 wide. (Not the 3D layer.)
inline uint8_t UiLayers(melonDS::NDS& nds, int unitNum)
{
    const melonDS::GPU2D::Unit& unit = unitNum == 0 ? nds.GPU.GPU2D_A : nds.GPU.GPU2D_B;
    const melonDS::u32 dispcnt = unit.DispCnt;
    if (((dispcnt >> 16) & 3) != 1) return 0;
    const int mode = dispcnt & 7;
    uint8_t mask = 0;
    for (int b = 0; b < 4; b++)
    {
        if (!(dispcnt & (0x100 << b)) || (b == 0 && (dispcnt & 8))) continue;
        if ((b == 2 && mode > 1) || (b == 3 && mode > 0)) continue;
        if (!(unit.BGCnt[b] & 0x4000)) mask |= 1 << b;
    }
    return mask;
}

// Whether any pixel of a screen (its renderer flags, 256x192) comes from one of `layers` (UiLayers).
inline bool Shows(const uint8_t* flags, uint8_t layers)
{
    if (!flags || !layers) return false;
    for (int i = 0; i < 256 * 192; i++)
    {
        const uint8_t f = flags[i];
        if (!(f & 0xD0) && (f & 0x0F & layers)) return true;
    }
    return false;
}

// Reads a background's video memory (unit 0: engine A, 1: engine B).
struct BgVram
{
    melonDS::NDS* nds = nullptr;
    int unitNum = 0;
    // A copy instead of the console's memory (see Capture), indexed by address & (size - 1).
    const std::vector<uint8_t>* copy = nullptr;

    melonDS::u16 R16(melonDS::u32 a) const
    {
        if (copy)
        {
            melonDS::u16 v;
            memcpy(&v, copy->data() + (a & (copy->size() - 1) & ~1u), 2);
            return v;
        }
        return unitNum == 0 ? nds->GPU.ReadVRAM_ABG<melonDS::u16>(a) : nds->GPU.ReadVRAM_BBG<melonDS::u16>(a);
    }
    melonDS::u8 R8(melonDS::u32 a) const
    {
        if (copy) return (*copy)[a & (copy->size() - 1)];
        return unitNum == 0 ? nds->GPU.ReadVRAM_ABG<melonDS::u8>(a) : nds->GPU.ReadVRAM_BBG<melonDS::u8>(a);
    }
};

// The map entry (tile number, flips, palette bank) at tile (tx, ty) of a layer's map in video
// memory, which wraps round the map as the hardware does.
inline melonDS::u16 MapEntry(const BgVram& v, const Layer& l, int tx, int ty)
{
    tx &= (l.mapW >> 3) - 1;
    ty &= (l.mapH >> 3) - 1;
    // 32x32-tile blocks: across first, then down (a 256-wide map has one across).
    const melonDS::u32 block = (tx >> 5) + (ty >> 5) * (l.mapW >> 8);
    return v.R16(l.mapBase + block * 0x800 + (((ty & 31) << 5) + (tx & 31)) * 2);
}

// The palette index (with the palette bank of a 16-colour tile) of pixel (px, py) of the tile a map
// entry names. 0: transparent.
inline int TilePixel(const BgVram& v, const Layer& l, melonDS::u16 e, int px, int py)
{
    if (e & 0x400) px = 7 - px;
    if (e & 0x800) py = 7 - py;
    if (l.c256) return v.R8(l.charBase + (e & 0x3FF) * 64 + py * 8 + px);
    const melonDS::u8 two = v.R8(l.charBase + (e & 0x3FF) * 32 + py * 4 + (px >> 1));
    const int idx = (px & 1) ? two >> 4 : two & 0xF;
    return idx ? idx + (e >> 12) * 16 : 0;
}

// A layer's palette index at map position (mx, my), wrapping round the map. 0: transparent.
inline int Sample(const BgVram& v, const Layer& l, int mx, int my)
{
    const int sx = mx & (l.mapW - 1), sy = my & (l.mapH - 1);
    return TilePixel(v, l, MapEntry(v, l, sx >> 3, sy >> 3), sx & 7, sy & 7);
}

// A map carried on past the end of its picture (widefill.h builds it): for each 8x8 cell beside the
// picture, which cell of the picture is shown there (the map's own art, never made-up colours).
// Cells are in map tiles; side 0 is left of the picture, 1 right of it.
struct Fill
{
    // A picture of the map: columns [a0, a1) of rows [row0, row1). A map can hold several, above one
    // another and side by side.
    struct Picture { int16_t a0, a1, row0, row1; };
    int cols = 0, rows = 0; // cells beside a picture on each side; the map's rows
    std::vector<Picture> pics;
    // Per map row, its pictures: pics[rowFirst .. rowFirst + rowCount), left to right (none: the map
    // as it is).
    std::vector<uint8_t> rowFirst, rowCount;
    // [side][(picture * cols + k) * rows + ty], k counted outwards from the picture: source
    // tx | ty << 12 | mirrored << 24, or kClear: nothing of this layer there (a layer of objects over
    // another: not carried on).
    std::vector<uint32_t> cell[2];
    static constexpr uint32_t kClear = 1u << 25;
};

// Where a layer's map comes from beside the screen, when not from video memory: a game that keeps
// its whole map in main RAM and copies only a window of it to the hardware's map (Explorers of Sky's
// towns and cutscenes: 3x3-tile chunks, a chunk map 64 chunks across; pmdground.h). For the map
// pixel the hardware shows at scrolled position (sx, sy) of its map: map pixel
// (originX + sx, originY + sy), with sx and sy not wrapped at the hardware map's size.
struct ChunkSource
{
    bool on = false;
    const uint8_t* ram = nullptr;
    uint32_t mask = 0;
    uint32_t chunkMap = 0, chunkTiles = 0; // addresses: u16 chunk numbers (64 a row); 9 u16 entries a chunk
    int originX = 0, originY = 0;          // map pixel at hardware map pixel (0, 0)
    int widthTiles = 0, heightTiles = 0;
    bool wrap = false;                     // the map repeats across (a panorama), or stops at its edges
    // (Kept in sources[0] whether or not that layer has a source: the columns of the hardware map the
    // game draws its window into, for the layers without one. 0: not known.)
    int windowCol = 0, windowCols = 0;
    // Which of the game's map renderers it comes from (one renderer's layers are one picture); -1: not known.
    int renderer = -1;
    // The map column (in tiles) under the screen's middle, set as it is drawn: which of a map's
    // pictures side by side is in view (Fill). INT32_MIN: not known.
    int viewTx = INT32_MIN;
    // Set as it is drawn: the map in RAM did not give the screen this frame (wide2d.h's check), and
    // the hardware's map beside the screen is not to be trusted either (a transition under way).
    bool distrusted = false;
    // Past the picture's end: the map carried on (widefill.h), or null.
    const Fill* fill = nullptr;

    uint16_t R16(uint32_t a) const
    {
        uint16_t v;
        memcpy(&v, ram + (a & mask & ~1u), 2);
        return v;
    }
    // The entry at map tile (tx, ty); false outside a map that does not repeat.
    bool Entry(int tx, int ty, uint16_t& e) const
    {
        if (wrap)
        {
            tx %= widthTiles; if (tx < 0) tx += widthTiles;
            ty %= heightTiles; if (ty < 0) ty += heightTiles;
        }
        else if (tx < 0 || ty < 0 || tx >= widthTiles || ty >= heightTiles)
            return false;
        const uint16_t chunk = R16(chunkMap + ((tx / 3) + (ty / 3) * 64) * 2);
        e = R16(chunkTiles + (chunk * 9 + (ty % 3) * 3 + tx % 3) * 2);
        return true;
    }
};

// Floor division by 8 for negative positions too.
inline int Tile8(int p) { return p >= 0 ? p >> 3 : -((7 - p) >> 3); }

// As Sample, from a ChunkSource: (sx, sy) a scrolled position of the hardware map, not wrapped.
// -1 past the edge of a map that does not repeat (there is no more of it).
inline int SampleChunks(const BgVram& v, const Layer& l, const ChunkSource& s, int sx, int sy)
{
    int mx = s.originX + sx, my = s.originY + sy;
    if (s.fill)
    {
        // Beside the picture: the cell of the picture the fill shows there (mirrored if so).
        const Fill& f = *s.fill;
        const int tx = Tile8(mx), ty = Tile8(my);
        const int count = ty >= 0 && ty < f.rows ? f.rowCount[ty] : 0;
        // The picture shown: of the row's pictures, the one under the screen's middle (or nearest it).
        int p = count ? f.rowFirst[ty] : -1;
        if (count > 1)
        {
            const int view = s.viewTx != INT32_MIN ? s.viewTx : tx;
            int bestD = 1 << 30;
            for (int i = f.rowFirst[ty]; i < f.rowFirst[ty] + count; i++)
            {
                const Fill::Picture& q = f.pics[i];
                const int d = view < q.a0 ? q.a0 - view : view >= q.a1 ? view - q.a1 + 1 : 0;
                if (d < bestD) { bestD = d; p = i; }
            }
        }
        if (p >= 0 && (tx < f.pics[p].a0 || tx >= f.pics[p].a1))
        {
            const int a0 = f.pics[p].a0, a1 = f.pics[p].a1;
            const int side = tx >= a1;
            const int k = side ? tx - a1 : a0 - 1 - tx;
            if (k >= f.cols) return -1;
            const uint32_t c = f.cell[side][((size_t)p * f.cols + k) * f.rows + ty];
            if (c & Fill::kClear) return 0;
            const int px = (c >> 24) & 1 ? 7 - (mx & 7) : mx & 7;
            mx = (int)(c & 0xFFF) * 8 + px;
            my = (int)((c >> 12) & 0xFFF) * 8 + (my & 7);
        }
    }
    uint16_t e;
    if (!s.Entry(Tile8(mx), Tile8(my), e)) return -1;
    return TilePixel(v, l, e, mx & 7, my & 7);
}

// What the last Render did with the maps in RAM (diagnostics): the offset it found to match the
// screen, whether it gave up on them (safety), and the share of samples that matched.
struct LastCheck
{
    int dx = 0, dy = 0;
    bool safety = false, searched = false;
    int good = 0, usable = 0;
};
inline LastCheck g_lastCheck;

// BGR555 to XRGB8888.
inline uint32_t Rgb(melonDS::u16 c)
{
    auto to8 = [](int v) { return (uint32_t)((v << 3) | (v >> 2)); };
    return (to8(c & 0x1F) << 16) | (to8((c >> 5) & 0x1F) << 8) | to8((c >> 10) & 0x1F);
}

// Diagnostics (SSR_BGMAP_DUMP): a layer's whole map as an image (mapW x mapH, XRGB, transparent
// pixels in magenta), with the palette the frame was drawn with.
inline std::vector<uint32_t> DrawMap(melonDS::NDS& nds, int unitNum, const Layer& l, const melonDS::u8* palSnap)
{
    BgVram v{ &nds, unitNum };
    std::vector<uint32_t> img((size_t)l.mapW * l.mapH);
    for (int y = 0; y < l.mapH; y++)
        for (int x = 0; x < l.mapW; x++)
        {
            const int idx = Sample(v, l, x, y);
            melonDS::u16 c;
            memcpy(&c, palSnap + idx * 2, 2);
            img[(size_t)y * l.mapW + x] = idx ? Rgb(c) : 0xFF00FF;
        }
    return img;
}

// BLDCNT effect 1 (alpha blending) as the hardware does it, on 8-bit channels: a * EVA + b * EVB, in
// 16ths, at most white. (EVA and EVB above 16 count as 16.)
inline uint32_t AlphaBlend(uint32_t a, uint32_t b, int eva, int evb)
{
    eva = std::min(eva, 16);
    evb = std::min(evb, 16);
    uint32_t r = 0;
    for (int sh = 0; sh < 24; sh += 8)
        r |= (uint32_t)std::min(255, (int)(((a >> sh) & 0xFF) * eva + ((b >> sh) & 0xFF) * evb) >> 4) << sh;
    return r;
}

constexpr int kHoleMax = 16; // pixels: a gap in the art beside the screen this wide or less is closed up

// The screen of engine `unitNum` with `extend` columns drawn on each side: the screen as the game
// drew it in the middle (`screen`, 256x192), and beside it every background layer whose map is 512
// pixels wide (one the game scrolls across a larger picture: a dungeon floor, and its grid; not the
// text windows and status lines, whose maps are the screen's size) and the sprites standing past
// the edges, with each line's scroll, the frame's palettes and sprite table, and the brightness
// each line was drawn with. Beside the screen is outside all of the game's windows. Not drawn there:
// rotating, bitmap and 256-colour (extended palette) layers and sprites, mosaic, and sprites blended
// with what is under them (layers blended with each other are: BLDCNT effect 1). `out` is (256 + 2 * extend) x 192, XRGB. False (out untouched) if the screen is not
// drawn the usual way or has no such layer. `sources` (4, by background number; may be null): layers
// whose map beside the screen comes from the game's own map in main RAM (ChunkSource) rather than
// from video memory, where the game keeps only the screen's window of it.
inline bool Render(melonDS::NDS& nds, int unitNum, const uint32_t* screen, int extend, std::vector<uint32_t>& out,
    const ChunkSource* sources = nullptr)
{
    using namespace melonDS;
    if (extend <= 0 || !screen) return false;
    const GPU2D::Unit& unit = unitNum == 0 ? nds.GPU.GPU2D_A : nds.GPU.GPU2D_B;
    const u32 dispcnt = unit.DispCnt;
    Layer all[4];
    const int nAll = TextLayers(nds, unitNum, all);
    // Outside the windows (when any is on): what WINOUT lets through.
    const u8 shown = (dispcnt & 0xE000) ? unit.WinCnt[2] : 0x3F;
    Layer layers[4];
    int n = 0;
    for (int i = 0; i < nAll; i++)
        if (all[i].mapW == 512 && (shown & (1 << all[i].num))) layers[n++] = all[i];
    if (!n) return false;

    auto* renderer = static_cast<GPU2D::SoftRenderer*>(&nds.GPU.GetRenderer2D());
    const u8* pal = renderer->PalSnap[unitNum];
    auto palette = [&](u32 off) -> u16 { u16 v; memcpy(&v, pal + (off & 0x3FE), 2); return v; };
    const u16 (*lineScroll)[4][2] = renderer->LineScroll[unitNum];
    const u16 (*lineBlend)[3] = renderer->LineBlend[unitNum];
    // A colour with the brightness effect of its layer (target: BG0-3, 4 sprites, 5 backdrop) and
    // the master brightness, as the line was drawn.
    auto colour = [&](u16 c, int target, int y) -> uint32_t {
        const u16 blend = lineBlend[y][0];
        const int effect = (blend & (1 << target)) && (shown & 0x20) ? (blend >> 6) & 3 : 0;
        const int evy = std::min<int>(16, lineBlend[y][1]);
        const int mbMode = (lineBlend[y][2] >> 14) & 3, mb = std::min(16, lineBlend[y][2] & 0x1F);
        int ch[3] = { (c & 0x1F) << 1, ((c >> 5) & 0x1F) << 1, ((c >> 10) & 0x1F) << 1 };
        for (int& v : ch)
        {
            if (effect == 2) v += ((63 - v) * evy + 8) >> 4;
            else if (effect == 3) v -= (v * evy + 7) >> 4;
            if (mbMode == 1) v += ((63 - v) * mb) >> 4;
            else if (mbMode == 2) v -= (v * mb + 15) >> 4;
        }
        auto to8 = [](int v) { return (uint32_t)((v << 2) | (v >> 4)); };
        return (to8(ch[0]) << 16) | (to8(ch[1]) << 8) | to8(ch[2]);
    };

    // Maps in main RAM (sources) are checked against the screen the game drew: while a scene scrolls,
    // the game's own state can be a few pixels ahead of the frame shown (Explorers of Sky's sky
    // scrolling up behind Dialga), and a map can even match the hardware's map without being the
    // picture shown. On a sample of the screen where a world layer with a source is in front, the
    // source must give the screen's colours. Measured over a whole intro (SSR_WIDE2D_TRACE), when it
    // does not, the game's map window has moved on by one chunk (24 pixels) across, down or both, and
    // each layer's map can move on its own (the treasure scene's items and its painting): so per
    // layer, those offsets are tried and the best used. A layer that matches under 70% at all of them
    // (tile animation alone stays above it) is not used from RAM this frame: beside the screen it keeps
    // to what the game has drawn (logged as safety, at most once a second).
    ChunkSource calibrated[4];
    if (sources)
    {
        for (int b = 0; b < 4; b++) calibrated[b] = sources[b];
        const u8* fl = renderer->LayerFlags[unitNum];
        BgVram vv{ &nds, unitNum };
        static const int xs[] = { 2, 5, 40, 90, 128, 170, 220, 250, 253 };
        // Layer i's samples (where it is in front) with its map moved (dx, dy): how many match.
        auto score = [&](int i, int dx, int dy, int& usable) {
            int good = 0;
            usable = 0;
            const Layer& l = layers[i];
            for (int y = 1; y < 192; y += 3)
                for (int x : xs)
                {
                    const u8 f = fl[y * 256 + x];
                    if ((f & 0xD0) || !(f & 0x0F & (1 << l.num))) continue;
                    usable++;
                    const int idx = SampleChunks(vv, l, sources[l.num], lineScroll[y][l.num][0] + x + dx, lineScroll[y][l.num][1] + y + dy);
                    if (idx <= 0) continue;
                    uint32_t a = colour(palette(idx * 2), l.num, y);
                    const uint32_t b = screen[y * 256 + x];
                    // (Blended on the screen with what is under it, BLDCNT effect 1: compared blended the
                    // same way, with the layer under it as the hardware's map gives it there.)
                    const u16 bc = lineBlend[y][0];
                    if (((bc >> 6) & 3) == 1 && (shown & 0x20) && (bc & (1 << l.num)))
                    {
                        uint32_t u = colour(palette(0), 5, y);
                        int under = 5;
                        for (int j = i + 1; j < n && under == 5; j++)
                        {
                            const Layer& m = layers[j];
                            const int jdx = Sample(vv, m, lineScroll[y][m.num][0] + x, lineScroll[y][m.num][1] + y);
                            if (jdx > 0) { u = colour(palette(jdx * 2), m.num, y); under = m.num; }
                        }
                        if (bc & (0x100 << under)) a = AlphaBlend(a, u, unit.EVA, unit.EVB);
                    }
                    int d = 0;
                    for (int sh = 0; sh < 24; sh += 8) d = std::max(d, std::abs((int)((a >> sh) & 0xFF) - (int)((b >> sh) & 0xFF)));
                    good += d <= 12;
                }
            return good;
        };
        static const int offsets[9][2] = { { 0, 0 }, { 24, 0 }, { -24, 0 }, { 0, 24 }, { 0, -24 }, { 24, 24 }, { -24, 24 }, { 24, -24 }, { -24, -24 } };
        g_lastCheck = LastCheck();
        static int lastLog = -1000, calls = 0;
        // (Per engine and background: the call it last matched on, at which offset.)
        static int64_t trusted[2][4] = { { -1000, -1000, -1000, -1000 }, { -1000, -1000, -1000, -1000 } };
        static int64_t gracedAt[2][4] = { { -1000, -1000, -1000, -1000 }, { -1000, -1000, -1000, -1000 } };
        static int trustedAt[2][4][2] = {};
        const int64_t frameNo = nds.NumFrames; // (the same frame can be drawn more than once)
        calls++;
        int chosen[4][2] = {}, decided = -1; // (a layer with too few samples of its own takes a decided one's offset)
        bool few[4] = {};
        for (int i = 0; i < n; i++)
        {
            const int b = layers[i].num;
            if (!sources[b].on) continue;
            int usable = 0;
            int best = score(i, 0, 0, usable), bx = 0, by = 0;
            g_lastCheck.good += best;
            g_lastCheck.usable += usable;
            if (usable < 30) { few[b] = true; continue; }
            if (best * 100 < usable * 90)
            {
                for (int k = 1; k < 9; k++)
                {
                    int u;
                    const int g = score(i, offsets[k][0], offsets[k][1], u);
                    if (g > best) { best = g; bx = offsets[k][0]; by = offsets[k][1]; }
                }
                g_lastCheck.searched = true;
            }
            // (A layer that matched the frame before and still matches near half: animated tiles the
            // game changed in video memory after drawing the frame (the waterfall, every 9 frames), or a
            // flash over part of the screen for a frame (the fountain-to-island burst, 40% at frame 3437).
            // Kept as it was the frame before, rather than drop out for one frame.)
            bool graced = false;
            if (best * 100 < usable * 70 && best * 100 >= usable * 30 &&
                (trusted[unitNum][b] == frameNo - 1 || gracedAt[unitNum][b] == frameNo))
            {
                bx = trustedAt[unitNum][b][0];
                by = trustedAt[unitNum][b][1];
                graced = true; // (once only: the next frame must match on its own)
                gracedAt[unitNum][b] = frameNo;
            }
            if (!graced && best * 100 < usable * 70)
            {
                g_lastCheck.safety = true;
                trusted[unitNum][b] = -1000;
                calibrated[b].on = false;
                calibrated[b].distrusted = true;
                if (calls - lastLog >= 60)
                {
                    fprintf(stderr, "[wide2d] safety: BG%d's map in RAM does not give the screen (best %d of %d samples, at %d,%d)\n", b, best, usable, bx, by);
                    lastLog = calls;
                }
                continue;
            }
            if (!graced)
            {
                trusted[unitNum][b] = frameNo;
                trustedAt[unitNum][b][0] = bx;
                trustedAt[unitNum][b][1] = by;
            }
            if (bx || by) { g_lastCheck.dx = bx; g_lastCheck.dy = by; }
            chosen[b][0] = bx;
            chosen[b][1] = by;
            if (decided < 0) decided = b;
        }
        for (int b = 0; b < 4; b++)
        {
            if (!calibrated[b].on) continue;
            if (few[b] && decided >= 0 && sources[decided].originX == sources[b].originX && sources[decided].originY == sources[b].originY)
            {
                chosen[b][0] = chosen[decided][0];
                chosen[b][1] = chosen[decided][1];
            }
            calibrated[b].originX += chosen[b][0];
            calibrated[b].originY += chosen[b][1];
            calibrated[b].viewTx = Tile8(calibrated[b].originX + lineScroll[96][b][0] + 128);
        }
        sources = calibrated;
    }

    const int outW = 256 + 2 * extend;
    out.resize((size_t)outW * 192);

    // The sprites beside the screen: colour and priority per pixel of the two side strips.
    // (Index: side strip column, 0..2*extend-1, the left strip first.)
    const int sideW = 2 * extend;
    static thread_local std::vector<uint32_t> objCol;
    static thread_local std::vector<uint8_t> objPrio;
    objCol.assign((size_t)sideW * 192, 0);
    objPrio.assign((size_t)sideW * 192, 4);
    auto sideIndex = [&](int x) -> int { return x < 0 ? x + extend : x - 256 + extend; }; // x outside [0, 256)
    if ((dispcnt & 0x1000) && (shown & 0x10))
    {
        static const uint8_t sizes[4][4][2] = {
            { { 8, 8 }, { 16, 16 }, { 32, 32 }, { 64, 64 } },
            { { 16, 8 }, { 32, 8 }, { 32, 16 }, { 64, 32 } },
            { { 8, 16 }, { 8, 32 }, { 16, 32 }, { 32, 64 } },
            { { 8, 8 }, { 8, 8 }, { 8, 8 }, { 8, 8 } } };
        u16 oam[0x200];
        memcpy(oam, renderer->OAMSnap[unitNum], sizeof(oam));
        const u32 objBase = unitNum == 0 ? 0x06400000 : 0x06600000;
        for (int o = 127; o >= 0; o--) // (the first sprite in front: drawn last)
        {
            const u16 a0 = oam[o * 4], a1 = oam[o * 4 + 1], a2 = oam[o * 4 + 2];
            if ((a0 & 0x300) == 0x200 || (a0 >> 14) == 3) continue; // hidden
            if (a0 & 0x100) continue;                                 // rotating: not drawn here
            const int mode = (a0 >> 10) & 3;
            if (mode == 2 || mode == 3) continue;                     // a window, or a bitmap
            const bool c256 = a0 & 0x2000;
            if (c256 && (dispcnt & (1u << 31))) continue;             // extended palettes
            const int sw = sizes[a0 >> 14][a1 >> 14][0], sh = sizes[a0 >> 14][a1 >> 14][1];
            // X is 9 bits: past the right edge up to the view's, or wrapped round to the left.
            int ox = a1 & 0x1FF, oy = a0 & 0xFF;
            if (ox >= 256 + extend) ox -= 512;
            if (oy + sh > 256) oy -= 256;
            if (ox + sw <= -extend || ox >= 256 + extend || (ox >= 0 && ox + sw <= 256)) continue; // nothing beside the screen
            const int prio = (a2 >> 10) & 3;
            const u32 tile = a2 & 0x3FF;
            const int bpp = c256 ? 8 : 4;
            for (int y = std::max(0, oy); y < std::min(192, oy + sh); y++)
                for (int x = std::max(ox, -extend); x < std::min(ox + sw, 256 + extend); x++)
                {
                    if (x >= 0 && x < 256) continue;
                    int px = x - ox, py = y - oy;
                    if (a1 & 0x1000) px = sw - 1 - px;
                    if (a1 & 0x2000) py = sh - 1 - py;
                    const int tx = px >> 3, ty = py >> 3;
                    u32 addr;
                    if (dispcnt & 0x10) // one-dimensional mapping
                        addr = tile * (32u << ((dispcnt >> 20) & 3)) + (ty * (sw >> 3) + tx) * 8 * bpp;
                    else
                        addr = (tile & (c256 ? 0x3FE : 0x3FF)) * 32 + (ty * 32 * 32 + tx * 8 * bpp);
                    addr += ((py & 7) * 8 + (px & 7)) * bpp / 8;
                    addr &= unitNum == 0 ? 0x3FFFF : 0x1FFFF;
                    const u8 v = nds.GPU.ReadVRAM_OBJ<u8>(objBase + addr);
                    const int idx = c256 ? v : ((px & 1) ? v >> 4 : v & 0xF);
                    if (!idx) continue;
                    const size_t i = (size_t)y * sideW + sideIndex(x);
                    if (prio <= objPrio[i])
                    {
                        objPrio[i] = (uint8_t)prio;
                        objCol[i] = colour(palette(0x200 + (c256 ? idx : idx + (a2 >> 12) * 16) * 2), 4, y);
                    }
                }
        }
    }

    BgVram v{ &nds, unitNum };
    const u8* flags = renderer->LayerFlags[unitNum];
    uint8_t worldLayers = 0;
    for (int i = 0; i < n; i++) worldLayers |= 1 << layers[i].num;
    // What a cover over the world at an edge of the screen does (see below): its weight in 256ths
    // and its colour, kept from the row above where the edge shows no world (a text window, a
    // sprite); the rows above the first that shows it take the first's.
    int coverA[2] = { 0, 0 };
    uint32_t coverC[2] = { 0, 0 };
    // Per side and row, where the game's map ends beside the screen (MapEnd): on the right the first
    // column past it, on the left how many columns from the outer edge are; -1 where it does not.
    int mapEnd[2][192];
    for (int sd = 0; sd < 2; sd++)
        for (int yy = 0; yy < 192; yy++) mapEnd[sd][yy] = -1;
    bool pictureAt[128] = {}; // (per side column of the row being drawn: a layer drew there)
    bool holeAt[128] = {};    // (and: only the backdrop there, inside the map)
    // Whether the screen shows the backdrop anywhere (LayerClass: no layer, 3D or sprite drew there).
    bool screenBackdrop = false;
    for (int i = 0; i < 256 * 192 && !screenBackdrop; i += 3) screenBackdrop = !(flags[i] & 0xDF);
    bool filled = false;
    for (int b = 0; b < 4 && sources; b++) filled |= sources[b].on && sources[b].fill;
    if (extend > 128) return false;
    // (Pass 0 only finds the first row showing the world at each edge; pass 1 draws.)
    bool firstFound[2] = { false, false };
    // In towns and cutscenes (sources) the cover is one for the whole side: the median of the rows'
    // (signed: black below 0, white above). The game fades those scenes with an overlay, the same
    // on every row, which this carries on exactly; a light effect over part of the picture (the
    // island's rays) changes it row by row, which carried on row by row painted bands. Pass 0 then
    // measures every row.
    const bool uniformCover = sources != nullptr;
    int rowCover[2][192], rowCovers[2] = { 0, 0 };
    for (int pass = 0; pass < 2; pass++)
    for (int y = 0; y < 192 && (pass || uniformCover || !firstFound[0] || !firstFound[1]); y++)
    {
        if (pass && y == 0 && uniformCover)
            for (int sd = 0; sd < 2; sd++)
            {
                coverA[sd] = 0;
                if (!rowCovers[sd]) continue;
                std::nth_element(rowCover[sd], rowCover[sd] + rowCovers[sd] / 2, rowCover[sd] + rowCovers[sd]);
                const int m = rowCover[sd][rowCovers[sd] / 2];
                coverA[sd] = std::abs(m);
                coverC[sd] = m > 0 ? 0xFFFFFF : 0;
            }
        uint32_t* row = &out[(size_t)y * outW];
        if (pass) memcpy(row + extend, screen + y * 256, 256 * 4);
        const uint32_t backdrop = colour(palette(0), 5, y);
        for (int side = 0; side < 2; side++)
        {
            if (!pass && firstFound[side] && !uniformCover) continue;
            // The layers carried on at this side of this row: those that show at its edge on the
            // screen (any of the 8 outermost pixels drawn): a floor goes on, a grid the game only
            // draws while asked for (and has left stale beside the screen) does not.
            // Layer i at screen column x of this row: from the game's own map in main RAM where it
            // gives one (sources), else from the hardware's map in video memory.
            auto sampleAt = [&](int i, int x) -> int {
                const Layer& l = layers[i];
                const int sx = lineScroll[y][l.num][0] + x, sy = lineScroll[y][l.num][1] + y;
                if (sources && sources[l.num].on) return SampleChunks(v, l, sources[l.num], sx, sy);
                if (sources && sources[l.num].distrusted && (x < 0 || x >= 256)) return -1; // (the mirror, MapEnd)
                // (With sources, the game keeps its picture in a window of the hardware map, columns
                // windowCol to windowCol + windowCols: a layer it has no map in RAM for has nothing
                // outside that window.)
                if (sources && sources[0].windowCols)
                {
                    const int col = Tile8(sx);
                    if (col < sources[0].windowCol || col >= sources[0].windowCol + sources[0].windowCols) return -1;
                }
                return Sample(v, l, sx, sy);
            };
            bool use[4] = {};
            for (int i = 0; i < n; i++)
            {
                // (A layer with a source is always carried on: its map is known beyond the screen.)
                use[i] = sources && sources[layers[i].num].on;
                for (int k = 0; k < 8 && !use[i]; k++) use[i] = sampleAt(i, side ? 255 - k : k) != 0;
            }
            // Diagnostics (SSR_WIDE2D_VERIFY): how much of this row, on the screen, the layers drawn here
            // disagree with what the game drew where a world layer is in front (should be none).
            if (pass && side == 0 && getenv("SSR_WIDE2D_VERIFY"))
            {
                static int bad = 0, total = 0, rows = 0;
                for (int x = 0; x < 256; x++)
                {
                    const u8 fl = flags[y * 256 + x];
                    if ((fl & 0xD0) || !(fl & 0x0F & worldLayers)) continue;
                    int topIdx = -1, tl = 0;
                    for (int i = 0; i < n && topIdx <= 0; i++)
                    {
                        if (!(fl & (1 << layers[i].num))) continue;
                        topIdx = sampleAt(i, x);
                        tl = i;
                    }
                    if (topIdx <= 0) continue;
                    total++;
                    const uint32_t a = colour(palette(topIdx * 2), layers[tl].num, y), b = screen[y * 256 + x];
                    int d = 0;
                    for (int sh = 0; sh < 24; sh += 8) d = std::max(d, std::abs((int)((a >> sh) & 0xFF) - (int)((b >> sh) & 0xFF)));
                    bad += d > 12;
                    if (d > 12 && bad == 1 && getenv("SSR_WIDE2D_VERIFY")[0] == '2')
                        fprintf(stderr, "[wide2d] verify: first difference at %d,%d: drawn %06X, screen %08X\n", x, y, a, b);
                }
                if (++rows == 192)
                {
                    fprintf(stderr, "[wide2d] verify: %d of %d world pixels differ from the screen\n", bad, total);
                    bad = total = rows = 0;
                }
            }
            if (pass && y == 100 && getenv("SSR_WIDE2D_DEBUG"))
                for (int i = 0; i < n; i++)
                    fprintf(stderr, "[wide2d] side %d bg%d prio %d map %dx%d base %X char %X scroll %d,%d use %d source %d\n", side,
                        layers[i].num, layers[i].prio, layers[i].mapW, layers[i].mapH, layers[i].mapBase, layers[i].charBase,
                        lineScroll[y][layers[i].num][0], lineScroll[y][layers[i].num][1], use[i], sources && sources[layers[i].num].on);
            if (pass && y == 100 && side == 0 && sources && getenv("SSR_WIDE2D_DEBUG"))
                for (int b = 0; b < 4; b++)
                    if (sources[b].on)
                        fprintf(stderr, "[wide2d] bg%d source %dx%d tiles wrap %d origin %d,%d\n", b, sources[b].widthTiles, sources[b].heightTiles,
                            sources[b].wrap, sources[b].originX, sources[b].originY);
            // The world without sprites at screen column x (or beside the screen). Where no layer
            // has anything (past the edge of a map that does not repeat), the line's backdrop, as
            // the hardware would show it.
            // (prio 5: every layer there is past the edge of its map: the game has no picture there.)
            // (Alpha blending, BLDCNT effect 1, as the line was drawn: a first-target layer on top is mixed
            // with what is under it, when that is a second target, EVA and EVB as the frame ended. The
            // personality test's backdrop is one layer at half over another: drawn opaque beside the
            // screen, it was brighter than in it, and out of step.)
            const u16 blendCnt = lineBlend[y][0];
            const bool alpha = ((blendCnt >> 6) & 3) == 1 && (shown & 0x20);
            auto world = [&](int x, int& prio) -> uint32_t {
                bool past = true;
                for (int i = 0; i < n; i++)
                {
                    if (!use[i]) continue;
                    const int idx = sampleAt(i, x);
                    if (idx > 0)
                    {
                        prio = layers[i].prio;
                        const uint32_t a = colour(palette(idx * 2), layers[i].num, y);
                        if (!alpha || !(blendCnt & (1 << layers[i].num))) return a;
                        // (What is under it: the next layer that draws here, else the backdrop.)
                        uint32_t b = backdrop;
                        int under = 5;
                        for (int j = i + 1; j < n && under == 5; j++)
                        {
                            if (!use[j]) continue;
                            const int jdx = sampleAt(j, x);
                            if (jdx > 0) { b = colour(palette(jdx * 2), layers[j].num, y); under = layers[j].num; }
                        }
                        return (blendCnt & (0x100 << under)) ? AlphaBlend(a, b, unit.EVA, unit.EVB) : a;
                    }
                    past &= idx < 0;
                }
                prio = past ? 5 : 4;
                return backdrop;
            };
            // A cover over the world at the edge: the 3D layer blended over it (Explorers of Sky
            // darkens what lies outside the lit room with translucent black polygons), or anything
            // else that makes the screen's edge differ from the world drawn here. Measured on the
            // 4 outermost pixels where the world is on top (or under the 3D layer), as a black or
            // white cover of some weight, and carried on beside the screen.
            int sumS = 0, sumW = 0, count = 0;
            for (int k = 0; k < 4; k++)
            {
                const int x = side ? 255 - k : k;
                const u8 f = flags[y * 256 + x];
                // (The flags as LayerClass reads them: 0x40 without 0x80 the 3D layer, 0x80 or 0x10 a sprite.)
                const bool is3D = (f & 0x40) && !(f & 0x80);
                const bool isWorld = !(f & 0xD0) && (f & 0x0F & worldLayers);
                if (!is3D && !isWorld) continue;
                int p;
                const uint32_t w = world(x, p), s = screen[y * 256 + x];
                sumW += (int)((w >> 16) & 0xFF) + ((w >> 8) & 0xFF) + (w & 0xFF);
                sumS += (int)((s >> 16) & 0xFF) + ((s >> 8) & 0xFF) + (s & 0xFF);
                count++;
            }
            // (Per row in a dungeon, whose darkness it is for; in towns and cutscenes measured in pass 0
            // and used as one, see uniformCover.)
            if (count && (!uniformCover || !pass))
            {
                if (std::abs(sumS - sumW) <= count * 3 * 4) coverA[side] = 0;
                else if (sumS < sumW)
                {
                    coverA[side] = 256 - sumS * 256 / std::max(1, sumW);
                    coverC[side] = 0;
                }
                else
                {
                    coverA[side] = (sumS - sumW) * 256 / std::max(1, count * 765 - sumW);
                    coverC[side] = 0xFFFFFF;
                }
                coverA[side] = std::clamp(coverA[side], 0, 256);
                firstFound[side] = true;
                if (uniformCover) rowCover[side][rowCovers[side]++] = coverC[side] ? coverA[side] : -coverA[side];
            }
            if (!pass) continue;
            const int a = coverA[side];
            const uint32_t cc = coverC[side];
            for (int k = 0; k < extend; k++)
            {
                const int x = side ? 256 + k : -extend + k;
                const size_t si = (size_t)y * sideW + sideIndex(x);
                int found;
                uint32_t c = world(x, found);
                pictureAt[k] = found < 4;
                holeAt[k] = found == 4;
                if (found == 5)
                {
                    // Past the map: filled in below (MapEnd). Sprites still go on top.
                    if (mapEnd[side][y] < 0) mapEnd[side][y] = k;
                    if (side == 0) mapEnd[0][y] = k + 1; // (the left side runs outwards from its right end)
                    found = 4;
                }
                if (objPrio[si] < 4 && objPrio[si] <= found) c = objCol[si]; // (4: no sprite there)
                if (a)
                {
                    auto mix = [&](int sh) {
                        const int o = (c >> sh) & 0xFF, t = (cc >> sh) & 0xFF;
                        return (uint32_t)((o * (256 - a) + t * a) >> 8) << sh;
                    };
                    c = mix(16) | mix(8) | mix(0);
                }
                row[x + extend] = c;
            }
            // Holes in the art: where the screen shows no backdrop anywhere (its picture covers it), the
            // backdrop beside it, inside the map, is a gap the game never shows (the sunset sea's map is
            // three copies of the sea with an empty column between them: black strips beside the screen).
            // A thin one takes the column next to it, nearer the screen; a wide one is where the picture
            // ends (MapEnd).
            if (sources && !screenBackdrop)
            {
                int run = 0;
                for (int j = 0; j < extend; j++)
                {
                    const int k = side ? j : extend - 1 - j; // (j: columns out from the screen)
                    const int x = side ? 256 + k : -extend + k;
                    if (!holeAt[k] || objPrio[(size_t)y * sideW + sideIndex(x)] < 4) { run = 0; continue; }
                    if (++run > kHoleMax)
                    {
                        const int start = side ? k - run + 1 : k + run - 1;
                        mapEnd[side][y] = side ? start : start + 1;
                        break;
                    }
                    row[x + extend] = row[x + extend + (side ? -1 : 1)];
                }
            }
            // (Blank map, the backdrop alone, running into the map's end, or with a map in RAM
            // (sources: a town or a cutscene) to the edge of the view: the picture ends where the
            // last of it is, and MapEnd takes over from there.)
            // (Not where the map is carried on past its picture (Fill): there the backdrop beside the
            // screen is the picture's own, as in it.)
            if (sources && !filled && mapEnd[side][y] < 0)
                mapEnd[side][y] = side ? extend : 0;
            if (mapEnd[side][y] >= 0)
            {
                if (side)
                    while (mapEnd[1][y] > 0 && !pictureAt[mapEnd[1][y] - 1]) mapEnd[1][y]--;
                else
                    while (mapEnd[0][y] < extend && !pictureAt[mapEnd[0][y]]) mapEnd[0][y]++;
                if (mapEnd[side][y] == (side ? extend : 0)) mapEnd[side][y] = -1; // (picture to the edge)
            }
        }
    }

    // A plain picture (one colour over nearly all of the 64 columns next to where it ends: lines of text on
    // black, a logo on white) is carried on in that colour: mirrored, its letters showed beside the
    // screen the wrong way round.
    for (int side = 0; side < 2; side++)
    {
        static thread_local std::vector<int> bins;
        bins.assign(4096, 0);
        int total = 0;
        // (Rows through one of the game's text windows, its dark neutral grey body, are not counted: on a
        // black screen with a message box the box was mirrored beside the screen, the ground is black.
        // The grey as the line's master brightness left it: while a scene fades to white the body is
        // lighter, and the box was mirrored again.)
        auto body = [&](uint32_t c, int y) {
            const int mbMode = (lineBlend[y][2] >> 14) & 3, mb = std::min(16, lineBlend[y][2] & 0x1F);
            auto lit = [&](int v) { // (6-bit, as the hardware does it; then 8-bit)
                if (mbMode == 1) v += ((63 - v) * mb) >> 4;
                else if (mbMode == 2) v -= (v * mb + 15) >> 4;
                return (v << 2) | (v >> 4);
            };
            const int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
            return r >= lit(0x18 >> 2) && r <= lit(0x30 >> 2) && std::abs(r - g) <= 4 && std::abs(g - b) <= 4;
        };
        for (int y = 0; y < 192; y++)
        {
            const int e = mapEnd[side][y];
            if (e < 0) continue;
            const uint32_t* row = &out[(size_t)y * outW];
            const int inner = side ? extend + 256 + e - 1 : e;
            int inBody = 0;
            for (int k = 0; k < 64; k++)
            {
                const int i = side ? inner - k : inner + k;
                if (i >= 0 && i < outW) inBody += body(row[i], y);
            }
            if (inBody >= 16) continue;
            for (int k = 0; k < 64; k++)
            {
                const int i = side ? inner - k : inner + k;
                if (i < 0 || i >= outW) break;
                const uint32_t c = row[i];
                bins[((c >> 12) & 0xF00) | ((c >> 8) & 0xF0) | ((c >> 4) & 0xF)]++;
                total++;
            }
        }
        if (!total) continue;
        const int top = (int)(std::max_element(bins.begin(), bins.end()) - bins.begin());
        if (bins[top] * 100 < total * 90) continue;
        // (The colour itself: the first pixel of the picture in that bin.)
        uint32_t ground = 0;
        for (int y = 0; y < 192 && !ground; y++)
        {
            const int e = mapEnd[side][y];
            if (e < 0) continue;
            const uint32_t c = out[(size_t)y * outW + (side ? extend + 256 + e - 1 : e)];
            if ((((c >> 12) & 0xF00) | ((c >> 8) & 0xF0) | ((c >> 4) & 0xF)) == top) ground = c | 0x1000000;
        }
        ground &= 0xFFFFFF;
        for (int y = 0; y < 192; y++)
        {
            const int e = mapEnd[side][y];
            if (e < 0) continue;
            uint32_t* row = &out[(size_t)y * outW];
            const int from = side ? extend + 256 + e : 0, to = side ? outW : e;
            for (int i = from; i < to; i++)
                if (objPrio[(size_t)y * sideW + sideIndex(i - extend)] >= 4) row[i] = ground;
            mapEnd[side][y] = -1; // (done: not mirrored below)
        }
    }
    // MapEnd: past the end of what can be drawn (no fill from widefill.h yet, a layer whose map in RAM
    // did not match the screen this frame, a dungeon floor's end), the picture mirrored about its last
    // column, fading with distance (to 60%, 96 pixels out) into the soft colour of its last 8 columns
    // averaged over the 49 rows around. A soft colour alone read as streaks in transitions (each
    // row its own colour); the mirror carries the picture's shapes on instead.
    for (int side = 0; side < 2; side++)
    {
        int rowSum[192][3];
        bool has[192];
        for (int y = 0; y < 192; y++)
        {
            const int e = mapEnd[side][y];
            has[y] = e >= 0;
            rowSum[y][0] = rowSum[y][1] = rowSum[y][2] = 0;
            if (!has[y]) continue;
            const uint32_t* row = &out[(size_t)y * outW];
            // (The 8 columns of picture next to where it ends, in `out`.)
            const int first = side ? extend + 256 + e - 8 : e;
            for (int k = 0; k < 8; k++)
            {
                const uint32_t c = row[std::clamp(first + k, 0, outW - 1)];
                rowSum[y][0] += (c >> 16) & 0xFF;
                rowSum[y][1] += (c >> 8) & 0xFF;
                rowSum[y][2] += c & 0xFF;
            }
        }
        for (int y = 0; y < 192; y++)
        {
            if (!has[y]) continue;
            int r = 0, g = 0, b = 0, cnt = 0;
            for (int k = std::max(0, y - 24); k <= std::min(191, y + 24); k++)
                if (has[k])
                {
                    r += rowSum[k][0]; g += rowSum[k][1]; b += rowSum[k][2];
                    cnt += 8;
                }
            const uint32_t soft = (uint32_t)(r / cnt) << 16 | (uint32_t)(g / cnt) << 8 | (uint32_t)(b / cnt);
            // (Next to the picture, the same over only the 7 rows around: closer to the picture's
            // own colours there, and still not a row's pixel drawn out into a streak.)
            int nr = 0, ng = 0, nb = 0, ncnt = 0;
            for (int k = std::max(0, y - 3); k <= std::min(191, y + 3); k++)
                if (has[k])
                {
                    nr += rowSum[k][0]; ng += rowSum[k][1]; nb += rowSum[k][2];
                    ncnt += 8;
                }
            const uint32_t last = (uint32_t)(nr / ncnt) << 16 | (uint32_t)(ng / ncnt) << 8 | (uint32_t)(nb / ncnt);
            uint32_t* row = &out[(size_t)y * outW];
            const int e = mapEnd[side][y];
            const int inner = side ? extend + 256 + e - 1 : e; // (the last column of picture)
            const int from = side ? extend + 256 + e : 0, to = side ? outW : e;
            // (The picture's other end on this row, so the mirror stays inside it.)
            const int lo = side ? (mapEnd[0][y] >= 0 ? mapEnd[0][y] : 0) : e,
                      hi = side ? inner : (mapEnd[1][y] >= 0 ? extend + 256 + mapEnd[1][y] - 1 : outW - 1);
            (void)last;
            for (int i = from; i < to; i++)
            {
                const int x = i - extend; // (screen column)
                if (objPrio[(size_t)y * sideW + sideIndex(x)] < 4) continue; // (a sprite there)
                const int d = side ? i - inner : inner - i;
                const int src = std::clamp(side ? inner - (d - 1) : inner + (d - 1), lo, hi);
                const uint32_t m = row[src];
                const int t = std::min(d, 96) * 150 / 96;
                auto mix = [&](int sh) {
                    const int o = (m >> sh) & 0xFF, q = (soft >> sh) & 0xFF;
                    return (uint32_t)((o * (256 - t) + q * t) >> 8) << sh;
                };
                row[i] = mix(16) | mix(8) | mix(0);
            }
        }
    }
    return true;
}

} // namespace Wide2D
