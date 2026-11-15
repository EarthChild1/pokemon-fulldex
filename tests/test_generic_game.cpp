// ROM-free: a generic game's Hybrid layout at 16:9 (Layout::fill) and its AUTO choice of the large
// screen (ActivityFocus).
#include <cassert>
#include <cstdio>
#include <vector>
#include "../frontend/layout.h"
#include "../frontend/activity_focus.h"
#include "../frontend/merged.h"

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
        assert(s.items.size() == 2 && s.items[0].screen == big && s.items[1].screen == 1 - big && s.softBars);
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
