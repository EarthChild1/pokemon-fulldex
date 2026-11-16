#pragma once
// Pokemon Mystery Dungeon: Explorers of Sky (North America, C2SE): which part of the game is running,
// from the functions in its table (tools/pmd/C2SE.txt, made from pmdsky-debug's symbols; FuncWatch).
// It plays the part SoulSilver's recompiled probes play for its layouts: here, which screen the game
// is played on. Where none of them runs (the title, its menus, a fade) the screens' activity decides
// (ActivityFocus).
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "funcwatch.h"

struct PmdFuncs
{
    enum Mode { Unknown, Dungeon, DungeonMenu, Ground, Quiz };

    int advance = -1, display = -1, camera = -1, menu = -1, stairs = -1, script = -1, script2 = -1, quiz = -1;
    bool on = false;
    // A mode counts as running for this many frames after one of its functions last ran: the script
    // engine and the personality test skip frames, and a mode flickering off would move the picture.
    static constexpr unsigned kWindow = 30;

    static std::vector<std::string> Names()
    {
        return { "AdvanceFrame", "DisplayMonstersAndItems", "UpdateCamera", "DungeonMenuLoop", "StairsMenuLoop",
                 "RunNextOpcode", "FuncThatCallsRunNextOpcode", "PersonalityTestFrameUpdate" };
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
        on = advance >= 0 || script >= 0 || quiz >= 0;
    }

    Mode Current(const FuncWatch& fw) const
    {
        if (fw.Ran(quiz, kWindow)) return Quiz;
        if (fw.Ran(menu, kWindow) || fw.Ran(stairs, kWindow)) return DungeonMenu;
        if (fw.Ran(advance, kWindow) || fw.Ran(display, kWindow) || fw.Ran(camera, kWindow)) return Dungeon;
        if (fw.Ran(script, kWindow) || fw.Ran(script2, kWindow)) return Ground;
        return Unknown;
    }

    // The screen the game is played on in a mode: 0 top, 1 bottom, -1 not known (the activity decides).
    // The dungeon and its menus are on the bottom screen (the top one shows what the player chose for
    // it: the controls, the team's stats, the message log or the map); so is ground mode (towns, the
    // guild, cutscenes); the personality test asks its questions on the top one.
    static int LargeScreen(Mode m)
    {
        switch (m)
        {
        case Quiz: return 0;
        case Dungeon:
        case DungeonMenu:
        case Ground: return 1;
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

    static const char* Name(Mode m)
    {
        static const char* names[] = { "unknown", "dungeon", "dungeon menu", "ground", "personality test" };
        return names[m];
    }
};
