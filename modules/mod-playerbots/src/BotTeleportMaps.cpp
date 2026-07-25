/*
 * SkyFire playerbots — level/faction gates for open-world teleport destinations.
 */

#include "BotTeleportMaps.h"
#include "Player.h"
#include "SharedDefines.h"

namespace
{
    // Continent / starter maps (not instances).
    constexpr uint32 MAP_EASTERN_KINGDOMS = 0;
    constexpr uint32 MAP_KALIMDOR = 1;
    constexpr uint32 MAP_OUTLAND = 530;
    constexpr uint32 MAP_NORTHREND = 571;
    constexpr uint32 MAP_GILNEAS = 654;          // worgen start
    constexpr uint32 MAP_GILNEAS_CITY = 655;
    constexpr uint32 MAP_KEZAN = 648;            // goblin start
    constexpr uint32 MAP_LOST_ISLES = 649;
    constexpr uint32 MAP_TOL_BARAD = 732;
    constexpr uint32 MAP_TOL_BARAD_PEN = 733;
    constexpr uint32 MAP_DEEPHOLM = 646;
    constexpr uint32 MAP_DARKMOON = 974;
    constexpr uint32 MAP_WANDERING_ISLE = 860;   // pandaren start
    constexpr uint32 MAP_PANDARIA = 870;

    bool InSet(std::initializer_list<uint32> list, uint32 mapId)
    {
        for (uint32 id : list)
            if (id == mapId)
                return true;
        return false;
    }
}

namespace BotTeleportMaps
{
    bool IsMapAllowedForLevel(uint32 mapId, uint32 level)
    {
        // Always allow classic continents + worgen/goblin/pandaren starts.
        if (InSet({ MAP_EASTERN_KINGDOMS, MAP_KALIMDOR,
                MAP_GILNEAS, MAP_GILNEAS_CITY, MAP_KEZAN, MAP_LOST_ISLES,
                MAP_WANDERING_ISLE }, mapId))
            return true;

        if (level < 69)
            return false;

        // Burning Crusade
        if (mapId == MAP_OUTLAND)
            return true;

        if (level < 80)
            return false;

        // Wrath
        if (mapId == MAP_NORTHREND)
            return true;

        if (level < 85)
            return false;

        // Cataclysm / MoP open world
        return InSet({ MAP_DEEPHOLM, MAP_TOL_BARAD, MAP_TOL_BARAD_PEN,
            MAP_DARKMOON, MAP_PANDARIA }, mapId);
    }

    bool IsHostileCapitalZone(uint32 zoneId, uint32 team)
    {
        if (!zoneId)
            return false;

        // Horde capitals — Alliance must not scatter here.
        static uint32 const hordeCaps[] = {
            1637, // Orgrimmar
            1638, // Thunder Bluff
            1497, // Undercity
            3487, // Silvermoon City
            14,   // Durotar (Orgrimmar approach)
            215,  // Mulgore
            85,   // Tirisfal Glades
            3430, // Eversong Woods
            4815, // Vashj'ir (ignored if unused)
        };
        // Alliance capitals — Horde must not scatter here.
        static uint32 const allianceCaps[] = {
            1519, // Stormwind City
            1537, // Ironforge
            1657, // Darnassus
            3557, // The Exodar
            12,   // Elwynn Forest
            1,    // Dun Morogh
            141,  // Teldrassil
            3524, // Azuremyst Isle
            4714, // Gilneas (worgen capital area)
        };

        auto has = [](uint32 const* arr, size_t n, uint32 z) -> bool
        {
            for (size_t i = 0; i < n; ++i)
                if (arr[i] == z)
                    return true;
            return false;
        };

        if (team == ALLIANCE)
            return has(hordeCaps, sizeof(hordeCaps) / sizeof(hordeCaps[0]), zoneId);
        if (team == HORDE)
            return has(allianceCaps, sizeof(allianceCaps) / sizeof(allianceCaps[0]), zoneId);
        return false;
    }

    bool CanBotTeleportTo(Player const* bot, uint32 mapId, uint32 zoneId)
    {
        if (!bot)
            return false;
        if (!IsMapAllowedForLevel(mapId, bot->getLevel()))
            return false;
        if (zoneId && IsHostileCapitalZone(zoneId, bot->GetTeam()))
            return false;
        return true;
    }
}
