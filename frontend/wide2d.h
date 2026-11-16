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

// A layer's palette index (with the palette bank of a 16-colour tile) at map position (mx, my),
// which wraps round the map as the hardware does. 0: transparent.
inline int Sample(const BgVram& v, const Layer& l, int mx, int my)
{
    const int sx = mx & (l.mapW - 1), sy = my & (l.mapH - 1);
    const int tx = sx >> 3, ty = sy >> 3;
    // 32x32-tile blocks: across first, then down (a 256-wide map has one across).
    const melonDS::u32 block = (tx >> 5) + (ty >> 5) * (l.mapW >> 8);
    const melonDS::u16 e = v.R16(l.mapBase + block * 0x800 + (((ty & 31) << 5) + (tx & 31)) * 2);
    int px = sx & 7, py = sy & 7;
    if (e & 0x400) px = 7 - px;
    if (e & 0x800) py = 7 - py;
    if (l.c256) return v.R8(l.charBase + (e & 0x3FF) * 64 + py * 8 + px);
    const melonDS::u8 two = v.R8(l.charBase + (e & 0x3FF) * 32 + py * 4 + (px >> 1));
    const int idx = (px & 1) ? two >> 4 : two & 0xF;
    return idx ? idx + (e >> 12) * 16 : 0;
}

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

// The screen of engine `unitNum` with `extend` columns drawn on each side: the screen as the game
// drew it in the middle (`screen`, 256x192), and beside it every background layer whose map is 512
// pixels wide (one the game scrolls across a larger picture: a dungeon floor, and its grid; not the
// text windows and status lines, whose maps are the screen's size) and the sprites standing past
// the edges, with each line's scroll, the frame's palettes and sprite table, and the brightness
// each line was drawn with. Beside the screen is outside all of the game's windows. Not drawn there:
// rotating, bitmap and 256-colour (extended palette) layers and sprites, mosaic, and blending
// between layers. `out` is (256 + 2 * extend) x 192, XRGB. False (out untouched) if the screen is not
// drawn the usual way or has no such layer.
inline bool Render(melonDS::NDS& nds, int unitNum, const uint32_t* screen, int extend, std::vector<uint32_t>& out)
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
    // (Pass 0 only finds the first row showing the world at each edge; pass 1 draws.)
    bool firstFound[2] = { false, false };
    for (int pass = 0; pass < 2; pass++)
    for (int y = 0; y < 192 && (pass || !firstFound[0] || !firstFound[1]); y++)
    {
        uint32_t* row = &out[(size_t)y * outW];
        if (pass) memcpy(row + extend, screen + y * 256, 256 * 4);
        const uint32_t backdrop = colour(palette(0), 5, y);
        for (int side = 0; side < 2; side++)
        {
            if (!pass && firstFound[side]) continue;
            // The layers carried on at this side of this row: those that show at its edge on the
            // screen (any of the 8 outermost pixels drawn): a floor goes on, a grid the game only
            // draws while asked for (and has left stale beside the screen) does not.
            bool use[4] = {};
            for (int i = 0; i < n; i++)
                for (int k = 0; k < 8 && !use[i]; k++)
                {
                    const int x = side ? 255 - k : k;
                    use[i] = Sample(v, layers[i], lineScroll[y][layers[i].num][0] + x, lineScroll[y][layers[i].num][1] + y) != 0;
                }
            if (pass && y == 100 && getenv("SSR_WIDE2D_DEBUG"))
                for (int i = 0; i < n; i++)
                    fprintf(stderr, "[wide2d] side %d bg%d prio %d map %dx%d base %X char %X scroll %d,%d use %d sample0 %d\n", side,
                        layers[i].num, layers[i].prio, layers[i].mapW, layers[i].mapH, layers[i].mapBase, layers[i].charBase,
                        lineScroll[y][layers[i].num][0], lineScroll[y][layers[i].num][1], use[i],
                        Sample(v, layers[i], lineScroll[y][layers[i].num][0] + (side ? 255 : 0), lineScroll[y][layers[i].num][1] + y));
            // The world without sprites at screen column x (or beside the screen).
            auto world = [&](int x, int& prio) -> uint32_t {
                for (int i = 0; i < n; i++)
                {
                    if (!use[i]) continue;
                    const Layer& l = layers[i];
                    if (const int idx = Sample(v, l, lineScroll[y][l.num][0] + x, lineScroll[y][l.num][1] + y))
                    {
                        prio = l.prio;
                        return colour(palette(idx * 2), l.num, y);
                    }
                }
                prio = 4;
                return backdrop;
            };
            // Where the map ends beside the screen (a town or a beach no wider than the game's own
            // view of it, a floor's last column), the world is mirrored about its last column: the
            // sand, the sea, a wall go on as they are near that column instead of stopping at the
            // backdrop. `edgeAt`: the last column with something drawn, outwards from the screen.
            int edgeAt = side ? 255 : 0;
            bool ends = false;
            if (pass)
            {
                int p;
                for (int k = 0; k < extend; k++)
                {
                    const int x = side ? 256 + k : -1 - k;
                    world(x, p);
                    if (p == 4)
                    {
                        ends = true;
                        break;
                    }
                    edgeAt = x;
                }
            }
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
            if (count)
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
            }
            if (!pass) continue;
            const int a = coverA[side];
            const uint32_t cc = coverC[side];
            for (int k = 0; k < extend; k++)
            {
                const int x = side ? 256 + k : -extend + k;
                const size_t si = (size_t)y * sideW + sideIndex(x);
                int found;
                uint32_t c;
                if (ends && (side ? x > edgeAt : x < edgeAt))
                {
                    // (Mirrored; nothing in the map beyond the mirror either: the backdrop.)
                    c = world(2 * edgeAt - x + (side ? 1 : -1), found);
                    found = 4;
                }
                else
                    c = world(x, found);
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
        }
    }
    return true;
}

} // namespace Wide2D
