// ROM-free: a generic game's Hybrid layout at 16:9 (Layout::fill) and its AUTO choice of the large
// screen (ActivityFocus).
#include <cassert>
#include <cstdio>
#include <vector>
#include "../frontend/layout.h"
#include "../frontend/activity_focus.h"
#include "../frontend/merged.h"
#include "../frontend/generic_scene.h"

// A screen at frame n: one colour, or with a bar sweeping across it (the part that changes).
static void Paint(std::vector<uint32_t>& fb, bool moving, int n)
{
    for (size_t i = 0; i < fb.size(); i++) fb[i] = 0x203040;
    if (moving)
        for (int y = 0; y < 192; y++)
            for (int x = 0; x < 40; x++) fb[y * 256 + ((n * 7 + x) % 256)] = 0xE0E0E0;
}
// Both screens changing everywhere: a flash or a fade.
static void Flash(std::vector<uint32_t>& fb, int n)
{
    for (size_t i = 0; i < fb.size(); i++) fb[i] = n % 2 ? 0xFFFFFF : 0x000000;
}

int main()
{
    // ---- layout ----
    Layout l;
    l.mode = LayoutMode::Hybrid;
    SDL_Rect r[2];
    LayoutRects(l, 1600, 720, r); // as before: the large screen at 4:3
    assert(r[0].w == 960 && r[0].h == 720);
    l.fill = true;
    LayoutRects(l, 1600, 720, r); // a phone's 20:9: the large screen at 16:9, the small one beside it
    assert(r[0].x == 0 && r[0].w == 1280 && r[0].h == 720);
    assert(r[1].x == 1280 && r[1].w == 320 && r[1].h == 240 && r[1].y == 240);
    int tx = -1, ty = -1;
    l.swapped = true; // the bottom screen large: it touches across the whole of its stretched rect
    LayoutRects(l, 1600, 720, r);
    assert(r[1].w == 1280 && r[0].w == 320);
    assert(LayoutTouch(l, r, r[1].x + r[1].w / 2, r[1].y + r[1].h / 2, tx, ty) && tx == 128 && ty == 96);
    assert(!LayoutTouch(l, r, r[0].x + 5, r[0].y + 5, tx, ty)); // (the small top screen is not the touch screen)
    l.swapped = false;
    LayoutRects(l, 1280, 720, r); // a 16:9 window: no strip, the small screen over the lower right corner
    assert(r[0].w == 1280 && r[1].x + r[1].w == r[0].x + r[0].w && r[1].y + r[1].h == r[0].y + r[0].h);

    // ---- the merged scene ----
    for (int big = 0; big < 2; big++)
    {
        const float wide = 1600.0f / 720.0f * 192; // a phone's 20:9 in base pixels (about 427)
        Scene s = GenericScene(big, wide);
        assert(s.items.size() == 2 && s.items[0].screen == big && s.items[1].screen == 1 - big && s.seamlessBars);
        const Element& in = s.items[1];
        const float w = 256 * in.scale, h = 192 * in.scale;
        assert(in.dx >= 256 && in.dx + w <= 128 + wide / 2 && in.dy >= 0 && in.dy + h <= 192); // in the bar, clear of the screen
        assert(w >= 60 && w < 200);                                                              // big enough to read
        int tx = -1, ty = -1;
        assert(MergedTouch(s, 128, 96, tx, ty) == (big == 1) && (big == 0 || (tx == 128 && ty == 96))); // only the bottom screen touches
        const float cx = in.dx + w / 2, cy = in.dy + h / 2;
        assert(MergedTouch(s, cx, cy, tx, ty) == (big == 0) && (big == 1 || (tx > 100 && tx < 156 && ty > 70 && ty < 122))); // the inset too
        Scene n = GenericScene(big, 256); // a 4:3 view: no bar, so the small screen is over the lower right corner
        const Element& c = n.items[1];
        assert(c.dx + 256 * c.scale <= 256 && c.dy + 192 * c.scale <= 192 && c.dx > 128 && c.dy > 96);
    }

    // ---- the bars continued from the screen's edges ----
    {
        // Rows 0-63: one colour at both edges. Rows 64-127: stripes 4 pixels wide (period 8). Rows
        // 128-191: noise (a picture: soft colour). The middle of the screen does not matter.
        std::vector<uint32_t> px(256 * 192, 0x000000);
        unsigned seed = 1;
        for (int y = 0; y < 192; y++)
            for (int x = 0; x < 256; x++)
            {
                uint32_t c;
                if (y < 64) c = x < 128 ? 0x3050A0 : 0xA05030;
                else if (y < 128) c = (x / 4) % 2 ? 0xF0F0F0 : 0x101010;
                else { seed = seed * 1103515245 + 12345; c = (seed >> 8) & 0xFFFFFF; }
                px[y * 256 + x] = c;
            }
        ScreenImage img = { px.data(), nullptr };
        std::vector<uint32_t> L, R;
        uint8_t kinds[2 * 192];
        const int w = 90;
        MergedSideBars(img, w, L, R, kinds);
        assert(L.size() == (size_t)w * 192 && R.size() == (size_t)w * 192);
        for (int d = 1; d <= w; d++)
        {
            assert((L[10 * w + d - 1] & 0xFFFFFF) == 0x3050A0 && (R[10 * w + d - 1] & 0xFFFFFF) == 0xA05030); // carried straight out
            // The stripes go on: d pixels left of x = 0 is the pixel at x = -d, which repeats x = 8 - d % 8.
            assert((L[80 * w + d - 1] & 0xFFFFFF) == px[80 * 256 + (8 - d % 8) % 8]);
            assert((R[80 * w + d - 1] & 0xFFFFFF) == px[80 * 256 + 255 - (8 - d % 8) % 8]);
        }
        assert(kinds[10] == 0 && kinds[192 + 10] == 0 && kinds[80] == 1 && kinds[192 + 80] == 1 && kinds[150] == 2 && kinds[192 + 150] == 2);
        uint32_t soft[2][192];
        MergedNarrowBars(img, soft[0], soft[1]);
        assert(L[150 * w] == soft[0][150] && R[150 * w + 40] == soft[1][150]); // a picture: the soft colour
        // And the view uses them: on a wide view the bar pixel beside the screen continues it.
        Scene sc;
        sc.items.push_back(FullScreen(0));
        sc.seamlessBars = true;
        ScreenImage both[2] = { img, img };
        std::vector<uint32_t> out;
        MergedCompose(sc, both, 1600, 720, out);
        const MergedView v = MergedMakeView(1600, 720);
        const int yRow = (int)(v.originY + 10.5f * v.unit), xLeft = (int)(v.originX - 3 * v.unit);
        assert((out[(size_t)yRow * 1600 + 2] & 0xFFFFFF) == 0x3050A0 && (out[(size_t)yRow * 1600 + xLeft] & 0xFFFFFF) == 0x3050A0);
    }

    // ---- the other screen's content beside the main one (GenericComposer) ----
    {
        const float wide = 1600.0f / 720.0f * 192;
        std::vector<uint32_t> topPx(256 * 192, 0x204060), botPx(256 * 192, 0x103050);
        std::vector<uint8_t> topFl(256 * 192, 0x08), botFl(256 * 192, 0x08); // BG3 behind everything on both
        for (int y = 0; y < 192; y++) for (int x = 0; x < 256; x++) topPx[y * 256 + x] = 0x204060 + (uint32_t)x; // (not blank)
        auto window = [&](int x0, int y0, int x1, int y1) { // a text window on BG0 of the bottom screen
            for (int y = 0; y < 192; y++) for (int x = 0; x < 256; x++) { botFl[y * 256 + x] = 0x08; botPx[y * 256 + x] = 0x103050; }
            for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) { botFl[y * 256 + x] = 0x01; botPx[y * 256 + x] = 0xE0E0E0; }
        };
        ScreenImage sc2[2] = { { topPx.data(), topFl.data() }, { botPx.data(), botFl.data() } };
        uint8_t drop; int x0, y0, x1, y1;
        window(40, 40, 215, 151);
        assert(GenericComposer::Content(sc2[1], drop, x0, y0, x1, y1) && (drop & LAYER_BG3) && !(drop & LAYER_BG0));
        assert(x0 == 40 && y0 == 40 && x1 == 215 && y1 == 151);
        {
            GenericComposer g; // a wide window: no room in a bar, so the main screen slides to the left edge
            Scene s;
            for (int f = 0; f < 30; f++) s = g.Build(0, wide, sc2);
            assert(g.Slide() == 1.0f && s.items.size() == 2 && s.seamlessBars);
            const Element& m = s.items[0]; const Element& p = s.items[1];
            assert(m.screen == 0 && std::abs(m.dx - (128 - wide / 2)) < 0.01f);  // at the left edge
            assert(p.screen == 1 && p.drop == drop && p.dx >= m.dx + 256 && p.dx + p.sw * p.scale <= 128 + wide / 2 + 0.01f);
            assert(p.scale >= 0.8f);                                            // big enough to read
            int tx, ty;                                                         // and it is touched where it is
            assert(MergedTouch(s, p.dx + p.sw * p.scale / 2, p.dy + p.sh * p.scale / 2, tx, ty) && std::abs(tx - 128) < 4 && std::abs(ty - 96) < 4);
        }
        {
            GenericComposer g; // a small piece (a button): in the bar, no slide
            window(100, 80, 131, 103);
            Scene s;
            for (int f = 0; f < 30; f++) s = g.Build(0, wide, sc2);
            const float bar = (wide - 256) / 2;
            assert(g.Slide() == 0.0f && s.items.size() == 2 && s.items[0].dx == 0 && s.items[1].dx >= 256 && s.items[1].dx < 256 + bar);
        }
        {
            GenericComposer g; // nothing on it but its picture: the whole screen, framed, in the bar
            for (int i = 0; i < 256 * 192; i++) { botFl[i] = 0x08; botPx[i] = 0x103050 + (uint32_t)(i % 7); }
            Scene s = g.Build(0, wide, sc2);
            assert(s.items.size() == 2 && s.items[1].sw == 256 && s.items[1].border);
            for (int i = 0; i < 256 * 192; i++) botPx[i] = 0; // blank: nothing beside the main screen
            s = g.Build(0, wide, sc2);
            assert(s.items.size() == 1);
        }
        {
            // The main screen's world drawn across the view (Explorers of Sky's dungeon): no slide even
            // for a wide window; the other screen whole, framed, at half size in the top right corner.
            window(40, 40, 215, 151);
            GenericComposer g;
            Scene s;
            for (int f = 0; f < 30; f++) s = g.Build(1, wide, sc2, true, true);
            const float R = 128 + wide / 2;
            assert(g.Slide() == 0.0f && s.items.size() == 2 && s.items[0].screen == 1 && s.items[0].dx == 0);
            const Element& p = s.items[1];
            assert(p.screen == 0 && p.sw == 256 && p.sh == 192 && p.drop == 0 && p.border && p.scale == 0.5f);
            assert(p.dy == 3 && std::abs(p.dx + 256 * p.scale - (R - 3)) < 0.01f);
            // Not when it shows only more of the world (no text or windows on it): the main screen alone.
            s = g.Build(1, wide, sc2, true, false);
            assert(s.items.size() == 1 && s.items[0].screen == 1 && s.items[0].dx == 0);
        }
    }

    // ---- which screen is large ----
    std::vector<uint32_t> t(256 * 192), b(256 * 192);
    {
        ActivityFocus a;
        int f = 0; // one frame count through the three parts
        for (; f < 600; f++)
        {
            Paint(t, false, f);
            Paint(b, false, f);
            assert(!a.Update(t.data(), b.data())); // nothing moves: it stays as it is
        }
        assert(!a.wantBottom);
        const int movingFrom = f;
        int firstFlip = -1;
        for (; f < movingFrom + 400 && firstFlip < 0; f++)
        {
            Paint(t, false, f);
            Paint(b, true, f); // only the bottom screen moves
            if (a.Update(t.data(), b.data())) firstFlip = f;
        }
        assert(a.wantBottom && firstFlip - movingFrom >= ActivityFocus::kHold - 1 && firstFlip - movingFrom < ActivityFocus::kHold + 30); // (not at once: the hold)
        int back = -1;
        for (; f < firstFlip + 1000 && back < 0; f++)
        {
            Paint(t, true, f); // now only the top screen moves
            Paint(b, false, f);
            if (a.Update(t.data(), b.data())) back = f;
        }
        assert(!a.wantBottom && back >= 0);
        assert(back - firstFlip >= ActivityFocus::kDwell); // (and not straight back: the dwell)
    }
    {
        ActivityFocus a; // a flash or a fade on both screens does not flip it
        for (int n = 0; n < 600; n++)
        {
            Flash(t, n);
            Flash(b, n);
            assert(!a.Update(t.data(), b.data()));
        }
        assert(!a.wantBottom);
    }
    {
        ActivityFocus a; // both moving about as much: it stays
        for (int n = 0; n < 600; n++)
        {
            Paint(t, true, n);
            Paint(b, true, n + 3);
            assert(!a.Update(t.data(), b.data()));
        }
        assert(!a.wantBottom);
    }
    puts("generic game layout and focus passed");
}
