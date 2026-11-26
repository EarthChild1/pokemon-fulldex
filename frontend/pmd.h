#pragma once
// Pokemon Mystery Dungeon: Explorers of Sky (North America, C2SE): which part of the game is running,
// from the functions in its table (tools/pmd/C2SE.txt, made from pmdsky-debug's symbols; FuncWatch).
// It plays the part SoulSilver's recompiled probes play for its layouts: here, which screen the game
// is played on. Where none of them runs (the title, its menus, a fade) the screens' activity decides
// (ActivityFocus).
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "funcwatch.h"

struct PmdFuncs
{
    enum Mode { Unknown, Dungeon, DungeonMenu, Ground, Quiz, Keyboard };

    int advance = -1, display = -1, camera = -1, menu = -1, stairs = -1, script = -1, script2 = -1, quiz = -1, keyboard = -1;
    bool on = false;
    // A mode counts as running for this many frames after one of its functions last ran: the script
    // engine and the personality test skip frames, and a mode flickering off would move the picture.
    static constexpr unsigned kWindow = 30;
    // The keyboard's longer: while a typed letter goes into the name its status is not asked for, 50 to 55
    // frames each time (seen on the phone), and with 30 the touch screen stopped being the large one for
    // about 20 frames at every letter.
    static constexpr unsigned kKeyboardWindow = 90;

    static std::vector<std::string> Names()
    {
        return { "AdvanceFrame", "DisplayMonstersAndItems", "UpdateCamera", "DungeonMenuLoop", "StairsMenuLoop",
                 "RunNextOpcode", "FuncThatCallsRunNextOpcode", "PersonalityTestFrameUpdate", "GetKeyboardStatus" };
    }

    void Find(const FuncWatch& fw)
    {
        advance = fw.Find("AdvanceFrame");
        display = fw.Find("DisplayMonstersAndItems");
        camera = fw.Find("UpdateCamera");
        menu = fw.Find("DungeonMenuLoop");
        stairs = fw.Find("StairsMenuLoop");
        script = fw.Find("RunNextOpcode");
        script2 = fw.Find("FuncThatCallsRunNextOpcode");
        quiz = fw.Find("PersonalityTestFrameUpdate");
        keyboard = fw.Find("GetKeyboardStatus");
        on = advance >= 0 || script >= 0 || quiz >= 0;
    }

    Mode Current(const FuncWatch& fw) const
    {
        // (The keyboard first: it is opened from the personality test, the towns and the guild alike.)
        if (fw.Ran(keyboard, kKeyboardWindow)) return Keyboard;
        if (fw.Ran(quiz, kWindow)) return Quiz;
        if (fw.Ran(menu, kWindow) || fw.Ran(stairs, kWindow)) return DungeonMenu;
        if (fw.Ran(advance, kWindow) || fw.Ran(display, kWindow) || fw.Ran(camera, kWindow)) return Dungeon;
        if (fw.Ran(script, kWindow) || fw.Ran(script2, kWindow)) return Ground;
        return Unknown;
    }

    // The screen the game is played on in a mode: 0 top, 1 bottom, -1 not known (the activity decides).
    // The dungeon and its menus are on the bottom screen (the top one shows what the player chose for
    // it: the controls, the team's stats, the message log or the map); so is ground mode (towns, the
    // guild, cutscenes). The personality test's backdrop is on both: there the screen with its windows
    // decides (WindowScreen), the top one until it has any. The naming keyboard is on the touch screen,
    // with the controls' help on the top one: the keyboard is where the player is, so it is large for as
    // long as it is up. (Before, the personality test's rule made the help large, the keyboard a small
    // panel at 20:9 and hidden under the help at 16:9.)
    static int LargeScreen(Mode m)
    {
        switch (m)
        {
        case Quiz: return 0;
        case Dungeon:
        case DungeonMenu:
        case Ground:
        case Keyboard: return 1;
        default: return -1;
        }
    }

    // The game's open windows (menus, text boxes, the help pages), from its own list (pmdsky-debug:
    // WINDOW_LIST, North America 0x022A88DC, 20 windows of 0xE0 bytes; struct window_params at the
    // start: the update function, then x, y, width and height in 8-pixel units, then the screen, 0
    // the main engine's). A slot is in use while its update function is set (GetWindowRectangle
    // reads the same four bytes), and shown while byte 0xB6 is not 0 (below).
    struct Window
    {
        int screen; // 0: engine A (the screen the dungeon is played on), 1: engine B
        int x0, y0, x1, y1; // pixels, end exclusive
    };
    static constexpr uint32_t kWindowList = 0x022A88DC, kWindowSize = 0xE0;
    static constexpr int kWindows = 20;

    // `ram`, `mask`: the console's main RAM. Returns how many windows were written to `out`.
    static int Windows(const uint8_t* ram, uint32_t mask, Window out[kWindows])
    {
        int n = 0;
        for (int i = 0; i < kWindows; i++)
        {
            const uint8_t* w = ram + ((kWindowList + i * kWindowSize) & mask);
            uint32_t update;
            memcpy(&update, w, 4);
            if (update < 0x02000000 || update >= 0x02400000) continue; // (a free slot)
            if (!w[6] || !w[7]) continue;
            // 0xB6 (pmdsky-debug: `valid`): 0 while the window is kept but not shown (the dungeon's
            // message box between messages). Seen 1-2 on every window on screen (the dungeon menu, its
            // floor name and team boxes, the top screen's help).
            if (!w[0xB6]) continue;
            Window& o = out[n++];
            o.screen = w[8] ? 1 : 0;
            o.x0 = w[4] * 8;
            o.y0 = w[5] * 8;
            o.x1 = o.x0 + w[6] * 8;
            o.y1 = o.y0 + w[7] * 8;
        }
        return n;
    }

    // Whether one of the windows on engine `engine` (8 pixels larger all round) reaches the rectangle
    // x0..x1, y0..y1 in the screen's pixels (where the panel goes: GenericComposer::PanelRect).
    static bool Covers(const Window* w, int n, int engine, float x0, float y0, float x1, float y1)
    {
        for (int i = 0; i < n; i++)
            if (w[i].screen == engine && w[i].x0 - 8 < x1 && w[i].x1 + 8 > x0 && w[i].y0 - 8 < y1 && w[i].y1 + 8 > y0) return true;
        return false;
    }

    // The same from the picture, on its own: a window's body is a dark neutral grey (about 0x20 on each
    // channel; the frame round it is bright), unlike the floor, the sky or the HUD line, which has no
    // body. True when, within x0..x1, y0..y1 of `screen` (256x192, 0xRRGGBB) widened by 8 pixels, at
    // least 6 rows have 12 or more such pixels in a row.
    static bool WindowPixels(const uint32_t* screen, float x0, float y0, float x1, float y1)
    {
        if (!screen) return false;
        const int ax = std::max(0, (int)x0 - 8), bx = std::min(256, (int)x1 + 8);
        const int ay = std::max(0, (int)y0 - 8), by = std::min(192, (int)y1 + 8);
        auto body = [](uint32_t c) {
            const int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
            return r >= 0x18 && r <= 0x30 && std::abs(r - g) <= 4 && std::abs(g - b) <= 4;
        };
        int rows = 0;
        for (int y = ay; y < by; y++)
        {
            int run = 0, best = 0;
            for (int x = ax; x < bx; x++)
            {
                run = body(screen[y * 256 + x]) ? run + 1 : 0;
                best = std::max(best, run);
            }
            rows += best >= 12;
        }
        return rows >= 6;
    }

    // The screen the game's windows are on, as LargeScreen gives it (0 top, 1 bottom; -1 none). The
    // personality test asks its questions, with their answers, in windows on the touch screen, over the
    // same backdrop as the top one's, and gives its result in one on the top screen (that one wins
    // while both are up, as the result comes in). `topEngine`: the engine on the top screen (0 is A).
    static int WindowScreen(const uint8_t* ram, uint32_t mask, int topEngine)
    {
        Window w[kWindows];
        const int n = Windows(ram, mask, w);
        int s = -1;
        for (int i = 0; i < n; i++)
        {
            if (w[i].screen == topEngine) return 0;
            s = 1;
        }
        return s;
    }

    static const char* Name(Mode m)
    {
        static const char* names[] = { "unknown", "dungeon", "dungeon menu", "ground", "personality test", "keyboard" };
        return names[m];
    }
};
