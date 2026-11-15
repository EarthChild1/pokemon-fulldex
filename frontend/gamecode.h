#pragma once
// Another language's version of a game (Pokemon White in Spanish, IRAS) plays with the English
// version's layouts (IRAO): its game code differs only in the last letter, the language. Its own
// function table (tools/versions, tools/port_version.py) or shifts (tools/soulsilver/*.json) say
// where its functions are; what it is, for everything else, is the English version's code.
#include <cstring>
#include <string>
#include "regional_profiles.h"

inline std::string BaseGameCode(const std::string& code)
{
    static const char* const aliases[][2] = {
        { "IRBS", "IRBO" }, { "IRAS", "IRAO" }, { "IRES", "IREO" }, { "IRDS", "IRDO" },
        { "CPUS", "CPUE" }, { "IPGS", "IPGE" }, { "IPKS", "IPKE" }, { "ADAS", "ADAE" },
        { "IRBF", "IRBO" }, { "IRAF", "IRAO" }, { "IREF", "IREO" }, { "IRDF", "IRDO" },
        { "IPGF", "IPGE" }, { "IPKF", "IPKE" }, { "CPUF", "CPUE" }, { "IPKI", "IPKE" },
        { "APAF", "APAE" }, { "APAS", "APAE" }, { "APAD", "APAE" }, { "ADAD", "ADAE" }, { "ADAI", "ADAE" }
    };
    for (const auto& alias : aliases) if (code == alias[0]) return alias[1];
    if (const auto* p = RegionalProfiles::Find(code.c_str())) return p->base;
    return code;
}
inline bool IsGame(const std::string& code, const char* english) { return BaseGameCode(code) == english; }

// Games that play on the plain emulator: no function table (a table would switch on the Black
// games' layouts for them) and no widescreen of their own yet. The launcher lists them as games
// it plays; they start with the two screens as one picture (Hybrid).
inline bool IsGenericGame(const std::string& code) { return code == "C2SE"; } // Pokemon Mystery Dungeon: Explorers of Sky (US)

// The size of a DS game's save: 512 KB for the Pokemon games, a generic game's own (Explorers of
// Sky: 128 KB, as melonDS's ROM list has it).
inline size_t DsSaveBytes(const std::string& code) { return code == "C2SE" ? 128 * 1024 : 512 * 1024; }
