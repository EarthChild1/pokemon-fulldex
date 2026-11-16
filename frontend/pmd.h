#pragma once
// Pokemon Mystery Dungeon: Explorers of Sky (North America, C2SE): which part of the game is running,
// from the functions in its table (tools/pmd/C2SE.txt, made from pmdsky-debug's symbols; FuncWatch).
// It plays the part SoulSilver's recompiled probes play for its layouts: here, which screen the game
// is played on. Where none of them runs (the title, its menus, a fade) the screens' activity decides
// (ActivityFocus).
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
    // The dungeon and its menus are on the top screen; ground mode (towns, the guild, cutscenes) on the
    // bottom one; the personality test asks its questions on the top one.
    static int LargeScreen(Mode m)
    {
        switch (m)
        {
        case Dungeon:
        case DungeonMenu:
        case Quiz: return 0;
        case Ground: return 1;
        default: return -1;
        }
    }

    static const char* Name(Mode m)
    {
        static const char* names[] = { "unknown", "dungeon", "dungeon menu", "ground", "personality test" };
        return names[m];
    }
};
