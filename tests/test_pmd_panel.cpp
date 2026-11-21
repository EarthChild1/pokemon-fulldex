// ROM-free: Explorers of Sky's second-screen panel over the wide world (generic_scene.h, pmd.h). Where it
// goes at several shapes of screen, and the two signals that make it step aside: the game's window
// list (synthetic windows) and a window's body in the picture (synthetic pictures).
#include <cassert>
#include <cstdio>
#include <vector>
#include "../frontend/merged.h"
#include "../frontend/generic_scene.h"
#include "../frontend/pmd.h"

int main()
{
    // ---- where the panel goes: the view's top right corner, inside the view, for each shape ----
    struct Shape { int w, h; bool besideScreen; } shapes[] = {
        { 1600, 720, true },   // 20:9, the owner's phone: wholly in the margin
        { 2400, 1080, true },  // 20:9 at another size
        { 1280, 720, false },  // 16:9: over the screen's right edge
        { 1920, 1200, false }, // 16:10
        { 1280, 800, false },  // 16:10
    };
    for (const Shape& s : shapes)
    {
        const MergedView v = MergedMakeView(s.w, s.h);
        for (float size : { 0.3f, 0.6f, 1.0f })
        {
            float x0, y0, x1, y1;
            GenericComposer::PanelRect(v.width, size, x0, y0, x1, y1);
            assert(x1 <= 128 + v.width / 2 && x0 < x1 && y0 >= 0 && y1 <= 192); // in the view
            assert(x1 - x0 <= 256 * 0.5f + 0.01f);                           // at most half the screen
            if (size == 0.6f) assert((x0 >= 256) == s.besideScreen);         // at 20:9 it never reaches the screen
        }
    }
    {
        // SECOND SCREEN SIZE out of range is held to 30-100%.
        float a0, a1, b0, b1, c0, c1, d0, d1;
        GenericComposer::PanelRect(340, 0.1f, a0, b0, a1, b1);
        GenericComposer::PanelRect(340, 0.3f, c0, d0, c1, d1);
        assert(a0 == c0 && b1 == d1);
    }

    // ---- the window list: a window on the played screen's engine, 8 pixels larger, reaching it ----
    float x0, y0, x1, y1;
    GenericComposer::PanelRect(MergedMakeView(1280, 720).width, 0.6f, x0, y0, x1, y1); // 16:9
    const int engine = 0;
    PmdFuncs::Window menu = { 0, 16, 16, 64, 112 };      // the dungeon menu, left
    PmdFuncs::Window stairs = { 0, 88, 32, 240, 120 };   // the Stairs information window, reaching right
    PmdFuncs::Window help = { 1, 16, 16, 240, 176 };     // the top screen's help: the other engine
    PmdFuncs::Window near = { 0, 120, 40, (int)x0 - 9, 80 }; // 9 pixels short of it: not over
    PmdFuncs::Window touch = { 0, 120, 40, (int)x0 - 7, 80 }; // within 8: over
    PmdFuncs::Window w1[] = { menu, help };
    assert(!PmdFuncs::Covers(w1, 2, engine, x0, y0, x1, y1));
    PmdFuncs::Window w2[] = { menu, stairs };
    assert(PmdFuncs::Covers(w2, 2, engine, x0, y0, x1, y1));
    assert(!PmdFuncs::Covers(&near, 1, engine, x0, y0, x1, y1));
    assert(PmdFuncs::Covers(&touch, 1, engine, x0, y0, x1, y1));
    assert(!PmdFuncs::Covers(w2, 0, engine, x0, y0, x1, y1));
    {
        // At 20:9 the game's windows (which end 16 pixels in from the edge, at 240) never reach it.
        float a0, b0, a1, b1;
        GenericComposer::PanelRect(MergedMakeView(1600, 720).width, 0.6f, a0, b0, a1, b1);
        PmdFuncs::Window widest = { 0, 0, 0, 240, 192 };
        assert(!PmdFuncs::Covers(&widest, 1, engine, a0, b0, a1, b1));
    }

    // ---- the picture: a window's dark grey body under the panel; not the floor, not text ----
    std::vector<uint32_t> fb(256 * 192);
    auto paint = [&](uint32_t c) { for (uint32_t& p : fb) p = c; };
    auto box = [&](int bx0, int by0, int bx1, int by1, uint32_t body, uint32_t frame) {
        for (int y = by0; y < by1; y++)
            for (int x = bx0; x < bx1; x++)
                fb[y * 256 + x] = (y < by0 + 2 || y >= by1 - 2 || x < bx0 + 2 || x >= bx1 - 2) ? frame : body;
    };
    paint(0x8C6A3C); // a sandy floor
    assert(!PmdFuncs::WindowPixels(fb.data(), x0, y0, x1, y1));
    box(88, 32, 240, 120, 0x222222, 0x50F050); // the Stairs window
    assert(PmdFuncs::WindowPixels(fb.data(), x0, y0, x1, y1));
    paint(0x8C6A3C);
    box(16, 16, 64, 112, 0x222222, 0x50F050);  // the menu, far from it
    assert(!PmdFuncs::WindowPixels(fb.data(), x0, y0, x1, y1));
    paint(0x8C6A3C);
    for (int y = 4; y < 12; y++)                // the HUD line: white text on the floor, no body
        for (int x = 0; x < 256; x += 3) fb[y * 256 + x] = 0xF8F8F8;
    assert(!PmdFuncs::WindowPixels(fb.data(), x0, y0, x1, y1));
    paint(0x2A2A3A);                            // a dark blue-grey cave floor: not neutral
    assert(!PmdFuncs::WindowPixels(fb.data(), x0, y0, x1, y1));
    assert(!PmdFuncs::WindowPixels(nullptr, x0, y0, x1, y1));

    puts("pmd panel passed");
}
