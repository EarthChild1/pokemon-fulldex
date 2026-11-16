#pragma once
// The picture of a generic game (gamecode.h): a game with no layout per scene, so the picture is
// worked out from the two screens themselves, the way SoulSilver's professor scene is put together
// (main.cpp, "intro"): the screen the game is played on fills the view at full height with its edges
// continued to the sides (Scene::seamlessBars), and what is on the other screen apart from its
// background (a menu, a text window, a button, the touch prompt) is cut out and stands beside it, on
// the continued background, instead of the whole other screen in a box. When that needs more room
// than a bar, the main screen slides to the left edge to make it, as the professor does for a choice.
// A small screen that is one picture with nothing on it is shown whole, framed; a blank one not at all.
#include <algorithm>
#include <cstdint>
#include "merged.h"

class GenericComposer
{
public:
    static constexpr int kGone = 40;          // frames without content before its held box is forgotten
    static constexpr float kSlideStep = 1.0f / 14; // the slide takes 14 frames, as the professor's
    static constexpr float kMinBarScale = 0.6f;    // smaller than this in a bar: slide to make room
    static constexpr float kWorldPanel = 0.5f;     // the other screen's panel over a wide world

    // What is on screen s apart from its background. The background is the backdrop and any layer
    // behind everything: one that shows round at least half of the screen's border (a window rarely
    // touches more than one edge) or covers most of the screen. False if there is nothing else, or the
    // rest covers nearly all of it too (the screen is itself the picture).
    static bool Content(const ScreenImage& s, uint8_t& drop, int& x0, int& y0, int& x1, int& y1)
    {
        if (!s.pixels || !s.flags) return false;
        int count[7] = {}, border[7] = {}, borderAll = 0; // BG0..BG3, OBJ, BACKDROP, 3D
        static const uint8_t cls[7] = { LAYER_BG0, LAYER_BG1, LAYER_BG2, LAYER_BG3, LAYER_OBJ, LAYER_BACKDROP, LAYER_3D };
        for (int y = 0; y < 192; y++)
            for (int x = 0; x < 256; x++)
            {
                const uint8_t c = LayerClass(s.flags[y * 256 + x]);
                const bool edge = x < 2 || x >= 254 || y < 2 || y >= 190;
                borderAll += edge;
                for (int k = 0; k < 7; k++)
                    if (c == cls[k])
                    {
                        count[k]++;
                        border[k] += edge;
                    }
            }
        drop = LAYER_BACKDROP;
        for (int k = 0; k < 7; k++)
            if (k != 4 && (border[k] * 2 >= borderAll || count[k] * 10 >= 256 * 192 * 6)) drop |= cls[k]; // (sprites are never the background)
        x0 = 256; y0 = 192; x1 = -1; y1 = -1;
        for (int y = 0; y < 192; y++)
            for (int x = 0; x < 256; x++)
                if (!(LayerClass(s.flags[y * 256 + x]) & drop))
                {
                    x0 = std::min(x0, x);
                    x1 = std::max(x1, x);
                    y0 = std::min(y0, y);
                    y1 = std::max(y1, y);
                }
        if (x1 < x0) return false;
        if ((x1 - x0 + 1) * (y1 - y0 + 1) >= 256 * 192 * 9 / 10) return false;
        // Outwards to an 8-pixel grid, so a small animation does not move the piece.
        x0 &= ~7;
        y0 &= ~7;
        x1 = std::min(x1 | 7, 255);
        y1 = std::min(y1 | 7, 191);
        return true;
    }

    static bool Blank(const ScreenImage& s)
    {
        if (!s.pixels) return true;
        const uint32_t c = s.pixels[0] & 0xFFFFFF;
        for (int i = 1; i < 256 * 192; i++)
            if ((s.pixels[i] & 0xFFFFFF) != c) return false;
        return true;
    }

    // The scene for this frame: `big` the screen the game is played on (ActivityFocus or the player).
    // `world`: that screen's world is drawn across the whole view (Scene::wide, wide2d.h: Explorers of
    // Sky's dungeons and towns), so it stays in the middle, and the other screen, if `panel` (it shows
    // text or windows, not just more of the world), is a framed panel in the view's top right corner
    // instead, over the world, at half size (no bar to put it in).
    Scene Build(int big, float viewWidth, const ScreenImage screens[2], bool world = false, bool panel = true)
    {
        frame_++;
        if (big != lastBig_)
        {
            lastBig_ = big;
            seen_ = 0;
            slide_ = 0;
        }
        const int small = 1 - big;
        const float L = 128 - viewWidth / 2, R = 128 + viewWidth / 2, bar = (viewWidth - 256) / 2;
        Scene sc = GenericScene(big, viewWidth); // (the main screen and the whole other one, framed, as a start)
        Element whole = sc.items[1];
        sc.items.resize(1);
        float target = 0;
        Element piece{};
        bool havePiece = false, framed = false;
        uint8_t drop = 0;
        int x0, y0, x1, y1;
        if (!Blank(screens[small]) && Content(screens[small], drop, x0, y0, x1, y1))
        {
            // The box only grows while the content is up (a button flashing, text being written), so
            // the piece does not jump about; it starts again once it has been gone a moment.
            const bool fresh = !seen_ || frame_ - seen_ > kGone || drop != heldDrop_;
            hx0_ = fresh ? x0 : std::min(hx0_, x0);
            hy0_ = fresh ? y0 : std::min(hy0_, y0);
            hx1_ = fresh ? x1 : std::max(hx1_, x1);
            hy1_ = fresh ? y1 : std::max(hy1_, y1);
            heldDrop_ = drop;
            seen_ = frame_;
            piece = Crop(small, hx0_, hy0_, hx1_ - hx0_ + 1, hy1_ - hy0_ + 1);
            piece.drop = heldDrop_;
            havePiece = true;
        }
        else if (!Blank(screens[small]))
            framed = true; // one picture: the whole screen, framed, in the bar as GenericScene has it
        const float sw = havePiece ? (float)piece.sw : 0, sh = havePiece ? (float)piece.sh : 0;
        const float inBar = havePiece ? std::min({ 1.25f, (bar - 6) / sw, 180.0f / sh }) : 0;
        if (havePiece && viewWidth >= 300 && inBar < kMinBarScale && !world) target = 1; // (no room in a bar: slide)
        if (world) slide_ = 0;
        slide_ += std::clamp(target - slide_, -kSlideStep, kSlideStep);
        const float ease = slide_ * slide_ * (3 - 2 * slide_);
        sc.items[0].dx = L * ease; // (L is negative: the main screen moves left)
        if (world && !panel) return sc;
        if (world && (havePiece || framed))
        {
            // (The whole screen: a panel's own background keeps its text readable over the world.)
            Element p = whole;
            p.drop = 0;
            const float pw = (float)p.sw, ph = (float)p.sh;
            p.scale = std::min({ kWorldPanel, 140.0f / pw, 100.0f / ph });
            p.dx = R - 3 - pw * p.scale;
            p.dy = 3;
            p.border = true;
            p.crisp = false;
            sc.items.push_back(p);
            return sc;
        }
        if (havePiece)
        {
            if (viewWidth < 300)
            {
                // No bars to speak of: over the main screen's lower right corner, framed.
                piece.scale = std::min({ 0.45f, 120.0f / sw, 90.0f / sh });
                piece.dx = 253 - sw * piece.scale;
                piece.dy = 189 - sh * piece.scale;
                piece.border = true;
            }
            else
            {
                // In the space right of the main screen: the bar, or what the slide frees up.
                const float left = sc.items[0].dx + 256 + 3, room = R - 3 - left;
                piece.scale = std::max(0.1f, std::min({ 1.25f, room / sw, 180.0f / sh }));
                piece.dx = left + (room - sw * piece.scale) / 2;
                piece.dy = 96 - sh * piece.scale / 2;
            }
            piece.crisp = piece.scale == 1.0f;
            sc.items.push_back(piece);
        }
        else if (framed)
            sc.items.push_back(whole);
        return sc;
    }

    float Slide() const { return slide_; }

private:
    unsigned frame_ = 0, seen_ = 0;
    int lastBig_ = -1;
    int hx0_ = 0, hy0_ = 0, hx1_ = -1, hy1_ = -1;
    uint8_t heldDrop_ = 0;
    float slide_ = 0;
};
