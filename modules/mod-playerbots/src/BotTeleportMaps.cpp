/*
 * SkyFire playerbots — level/faction gates and init scatter teleports.
 */

#include "BotTeleportMaps.h"
#include "Log.h"
#include "MapManager.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "WorldSession.h"

#include <cstdlib>
#include <vector>

namespace
{
    constexpr uint32 MAP_EASTERN_KINGDOMS = 0;
    constexpr uint32 MAP_KALIMDOR = 1;
    constexpr uint32 MAP_OUTLAND = 530;
    constexpr uint32 MAP_NORTHREND = 571;
    constexpr uint32 MAP_GILNEAS = 654;
    constexpr uint32 MAP_GILNEAS_CITY = 655;
    constexpr uint32 MAP_KEZAN = 648;
    constexpr uint32 MAP_LOST_ISLES = 649;
    constexpr uint32 MAP_TOL_BARAD = 732;
    constexpr uint32 MAP_TOL_BARAD_PEN = 733;
    constexpr uint32 MAP_DEEPHOLM = 646;
    constexpr uint32 MAP_DARKMOON = 974;
    constexpr uint32 MAP_WANDERING_ISLE = 860;
    constexpr uint32 MAP_PANDARIA = 870;

    // teamFilter: 0 = either faction, else ALLIANCE / HORDE.
    // raceFilter: 0 = any race; otherwise required race (pandaren neutral
    // matches all three pandaren race IDs).
    struct TeleAnchor
    {
        uint32 teamFilter;
        uint8 raceFilter;
        uint8 minLevel;
        uint8 maxLevel;
        uint32 mapId;
        float x;
        float y;
        float z;
        float o;
        uint32 zoneId;
    };

    bool InSet(std::initializer_list<uint32> list, uint32 mapId)
    {
        for (uint32 id : list)
            if (id == mapId)
                return true;
        return false;
    }

    bool IsPandarenRace(uint8 race)
    {
        return race == RACE_PANDAREN_NEUTRAL
            || race == RACE_PANDAREN_ALLIANCE
            || race == RACE_PANDAREN_HORDE;
    }

    bool RaceMatchesAnchor(uint8 botRace, uint8 raceFilter)
    {
        if (!raceFilter)
            return true;
        if (raceFilter == RACE_PANDAREN_NEUTRAL)
            return IsPandarenRace(botRace);
        return botRace == raceFilter;
    }

    // Worgen / goblin / pandaren starter continents — only that race may land there.
    bool CanRaceUseStartMap(uint8 race, uint32 mapId)
    {
        switch (mapId)
        {
            case MAP_GILNEAS:
            case MAP_GILNEAS_CITY:
                return race == RACE_WORGEN;
            case MAP_KEZAN:
            case MAP_LOST_ISLES:
                return race == RACE_GOBLIN;
            case MAP_WANDERING_ISLE:
                return IsPandarenRace(race);
            default:
                return true;
        }
    }

    // Curated open-world anchors (inns / flight / leveling hubs). Not enemy capitals.
    TeleAnchor const kAnchors[] =
    {
        // --- Alliance classic leveling ---
        { ALLIANCE, 0, 1, 10, MAP_EASTERN_KINGDOMS, -8949.95f, -132.49f, 83.65f, 0.0f, 12 },   // Northshire / Elwynn
        { ALLIANCE, 0, 5, 20, MAP_EASTERN_KINGDOMS, -9459.0f, 64.0f, 55.9f, 0.0f, 12 },         // Goldshire
        { ALLIANCE, 0, 1, 12, MAP_EASTERN_KINGDOMS, -6240.0f, 350.0f, 383.0f, 0.0f, 1 },        // Coldridge
        { ALLIANCE, 0, 5, 20, MAP_EASTERN_KINGDOMS, -5601.0f, -482.0f, 397.0f, 0.0f, 1 },       // Kharanos
        { ALLIANCE, 0, 10, 30, MAP_EASTERN_KINGDOMS, -5424.0f, -2930.0f, 349.0f, 0.0f, 38 },    // Loch Modan
        { ALLIANCE, 0, 15, 35, MAP_EASTERN_KINGDOMS, -10574.0f, -1178.0f, 28.0f, 0.0f, 40 },    // Westfall
        { ALLIANCE, 0, 18, 40, MAP_EASTERN_KINGDOMS, -9246.0f, -2232.0f, 69.0f, 0.0f, 44 },     // Redridge
        { ALLIANCE, 0, 20, 45, MAP_EASTERN_KINGDOMS, -10547.0f, -1157.0f, 28.0f, 0.0f, 40 },
        { ALLIANCE, 0, 1, 15, MAP_KALIMDOR, 10311.0f, 831.0f, 1326.0f, 0.0f, 141 },             // Shadowglen / Teldrassil
        { ALLIANCE, 0, 5, 20, MAP_KALIMDOR, 9888.0f, 947.0f, 1307.0f, 0.0f, 141 },              // Dolanaar
        { ALLIANCE, 0, 10, 30, MAP_KALIMDOR, 6406.0f, 482.0f, 8.0f, 0.0f, 148 },                // Darkshore
        { ALLIANCE, 0, 1, 15, MAP_OUTLAND, -3961.0f, -13931.0f, 100.5f, 0.0f, 3524 },           // Azure Watch
        { ALLIANCE, 0, 10, 30, MAP_OUTLAND, -4190.0f, -12510.0f, 45.0f, 0.0f, 3525 },           // Blood Watch
        { ALLIANCE, 0, 20, 50, MAP_KALIMDOR, -3615.0f, -4440.0f, 13.0f, 0.0f, 400 },            // Thousand Needles approach
        { ALLIANCE, 0, 30, 55, MAP_EASTERN_KINGDOMS, -10487.0f, -3270.0f, 21.0f, 0.0f, 33 },    // Stranglethorn
        { ALLIANCE, 0, 40, 60, MAP_EASTERN_KINGDOMS, -7190.0f, -3940.0f, 9.0f, 0.0f, 45 },      // Arathi
        { ALLIANCE, 0, 50, 68, MAP_EASTERN_KINGDOMS, -11070.0f, -2840.0f, 4.0f, 0.0f, 33 },
        { ALLIANCE, 0, 55, 68, MAP_EASTERN_KINGDOMS, 3230.0f, -4030.0f, 108.0f, 0.0f, 139 },    // EPL
        // Worgen start (Alliance) — worgen only
        { ALLIANCE, RACE_WORGEN, 1, 15, MAP_GILNEAS, -1450.0f, 1670.0f, 16.0f, 0.0f, 4714 },

        // --- Horde classic leveling ---
        { HORDE, 0, 1, 10, MAP_KALIMDOR, -618.0f, -4250.0f, 38.0f, 0.0f, 14 },                  // Valley of Trials
        { HORDE, 0, 5, 20, MAP_KALIMDOR, 338.0f, -4688.0f, 16.0f, 0.0f, 14 },                   // Razor Hill
        { HORDE, 0, 1, 12, MAP_EASTERN_KINGDOMS, 1900.0f, 1545.0f, 89.0f, 0.0f, 85 },           // Deathknell
        { HORDE, 0, 5, 20, MAP_EASTERN_KINGDOMS, 2260.0f, 250.0f, 34.0f, 0.0f, 85 },            // Brill
        { HORDE, 0, 1, 12, MAP_KALIMDOR, -2917.0f, -257.0f, 53.0f, 0.0f, 215 },                 // Red Cloud Mesa
        { HORDE, 0, 5, 20, MAP_KALIMDOR, -2360.0f, -390.0f, -7.0f, 0.0f, 215 },                 // Bloodhoof
        { HORDE, 0, 1, 15, MAP_OUTLAND, 10349.0f, -6360.0f, 36.0f, 0.0f, 3430 },                // Sunstrider Isle
        { HORDE, 0, 5, 20, MAP_OUTLAND, 9470.0f, -6850.0f, 16.0f, 0.0f, 3430 },                 // Falconwing
        { HORDE, 0, 10, 30, MAP_KALIMDOR, -2370.0f, -1940.0f, 96.0f, 0.0f, 17 },                // Crossroads
        { HORDE, 0, 15, 35, MAP_EASTERN_KINGDOMS, -1260.0f, -2520.0f, 21.0f, 0.0f, 130 },       // Tarren Mill
        { HORDE, 0, 20, 45, MAP_EASTERN_KINGDOMS, -920.0f, -3500.0f, 53.0f, 0.0f, 267 },        // Hillsbrad
        { HORDE, 0, 25, 50, MAP_KALIMDOR, -720.0f, -1850.0f, 92.0f, 0.0f, 17 },                 // Barrens mid
        { HORDE, 0, 30, 55, MAP_EASTERN_KINGDOMS, -14450.0f, 495.0f, 15.0f, 0.0f, 33 },         // STV camp
        { HORDE, 0, 40, 60, MAP_EASTERN_KINGDOMS, -1040.0f, -3500.0f, 60.0f, 0.0f, 45 },
        { HORDE, 0, 50, 68, MAP_EASTERN_KINGDOMS, 2300.0f, -4500.0f, 75.0f, 0.0f, 139 },        // EPL
        { HORDE, 0, 55, 68, MAP_KALIMDOR, 4250.0f, -2800.0f, 950.0f, 0.0f, 361 },               // Felwood / Winterspring approach
        // Goblin start — goblin only
        { HORDE, RACE_GOBLIN, 1, 15, MAP_KEZAN, -8424.0f, 1360.0f, 104.0f, 0.0f, 4737 },
        { HORDE, RACE_GOBLIN, 5, 20, MAP_LOST_ISLES, 1720.0f, 2420.0f, 10.0f, 0.0f, 4720 },

        // --- Outland (69-79; also usable mid if map allowed — gated by CanBotTeleportTo) ---
        { ALLIANCE, 0, 58, 79, MAP_OUTLAND, -883.0f, 2690.0f, 94.0f, 0.0f, 3483 },              // Honor Hold
        { ALLIANCE, 0, 60, 79, MAP_OUTLAND, -3000.0f, 2200.0f, 70.0f, 0.0f, 3519 },             // Telredor
        { ALLIANCE, 0, 62, 79, MAP_OUTLAND, -2070.0f, 5280.0f, 4.0f, 0.0f, 3522 },              // Orebor
        { HORDE, 0, 58, 79, MAP_OUTLAND, 139.0f, 2670.0f, 85.0f, 0.0f, 3483 },                  // Thrallmar
        { HORDE, 0, 60, 79, MAP_OUTLAND, -1480.0f, 5050.0f, 20.0f, 0.0f, 3518 },                // Garadar
        { HORDE, 0, 62, 79, MAP_OUTLAND, -2560.0f, 7300.0f, 35.0f, 0.0f, 3520 },                // Stonebreaker
        { 0, 0, 65, 79, MAP_OUTLAND, 3087.0f, 3590.0f, 144.0f, 0.0f, 3523 },                   // Area 52 (neutral-ish)

        // --- Northrend (80-84) ---
        { ALLIANCE, 0, 68, 84, MAP_NORTHREND, 2230.0f, 5270.0f, 6.0f, 0.0f, 495 },              // Valgarde
        { ALLIANCE, 0, 70, 84, MAP_NORTHREND, 3470.0f, -1100.0f, 165.0f, 0.0f, 65 },            // Westguard
        { HORDE, 0, 68, 84, MAP_NORTHREND, 2750.0f, -650.0f, 100.0f, 0.0f, 495 },               // Vengeance Landing
        { HORDE, 0, 70, 84, MAP_NORTHREND, 3870.0f, -4540.0f, 210.0f, 0.0f, 394 },              // Conquest Hold
        { 0, 0, 72, 84, MAP_NORTHREND, 5570.0f, 5750.0f, -75.0f, 0.0f, 210 },                   // Dalaran outskirts / Crystalsong
        { 0, 0, 74, 84, MAP_NORTHREND, 5800.0f, 460.0f, 140.0f, 0.0f, 66 },                     // Dragonblight

        // --- Pandaria (85+) ---
        { ALLIANCE, 0, 85, 90, MAP_PANDARIA, -2950.0f, -250.0f, 250.0f, 0.0f, 5785 },           // Paw'don
        { HORDE, 0, 85, 90, MAP_PANDARIA, 3130.0f, -680.0f, 255.0f, 0.0f, 5805 },               // Honeydew
        { 0, 0, 85, 90, MAP_PANDARIA, 1600.0f, 900.0f, 480.0f, 0.0f, 5840 },                    // Dawn's Blossom area
        { 0, 0, 86, 90, MAP_PANDARIA, -200.0f, 2300.0f, 160.0f, 0.0f, 5841 },                   // Jade Forest mid

        // Pandaren start — pandaren only
        { 0, RACE_PANDAREN_NEUTRAL, 1, 15, MAP_WANDERING_ISLE, 1460.0f, 3460.0f, 182.0f, 0.0f, 5736 },
    };
}

namespace BotTeleportMaps
{
    bool IsMapAllowedForLevel(uint32 mapId, uint32 level)
    {
        // EK/Kalimdor/race starts always. Outland (530) also hosts Draenei/BE
        // starter isles — keep it open; anchor level bands keep Hellfire etc.
        // off low-level init teleports.
        if (InSet({ MAP_EASTERN_KINGDOMS, MAP_KALIMDOR, MAP_OUTLAND,
                MAP_GILNEAS, MAP_GILNEAS_CITY, MAP_KEZAN, MAP_LOST_ISLES,
                MAP_WANDERING_ISLE }, mapId))
            return true;

        if (level < 69)
            return false;

        if (level < 80)
            return false;

        if (mapId == MAP_NORTHREND)
            return true;

        if (level < 85)
            return false;

        return InSet({ MAP_DEEPHOLM, MAP_TOL_BARAD, MAP_TOL_BARAD_PEN,
            MAP_DARKMOON, MAP_PANDARIA }, mapId);
    }

    bool IsHostileCapitalZone(uint32 zoneId, uint32 team)
    {
        if (!zoneId)
            return false;

        static uint32 const hordeCaps[] = {
            1637, 1638, 1497, 3487, 14, 215, 85, 3430,
        };
        static uint32 const allianceCaps[] = {
            1519, 1537, 1657, 3557, 12, 1, 141, 3524, 4714,
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
        if (!CanRaceUseStartMap(bot->getRace(), mapId))
            return false;
        if (zoneId && IsHostileCapitalZone(zoneId, bot->GetTeam()))
            return false;
        return true;
    }

    namespace
    {
        constexpr uint8 kScatterMinLevel = 6;

        void FinalizeIfNeeded(Player* bot)
        {
            if (WorldSession* session = bot->GetSession())
                if (session->IsBot() && bot->IsBeingTeleported())
                    session->FinalizeBotTeleport();
        }
    }

    bool TeleportToHomebind(Player* bot)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return false;

        uint32 mapId = bot->m_homebindMapId;
        float x = bot->m_homebindX;
        float y = bot->m_homebindY;
        float z = bot->m_homebindZ;
        float o = bot->GetOrientation();

        if (!MapManager::IsValidMapCoord(mapId, x, y, z))
        {
            if (PlayerInfo const* info = sObjectMgr->GetPlayerInfo(bot->getRace(), bot->getClass()))
            {
                mapId = info->mapId;
                x = info->positionX;
                y = info->positionY;
                z = info->positionZ;
                o = info->orientation;
            }
            else
                return false;
        }

        if (!bot->TeleportTo(mapId, x, y, z, o))
            return false;

        FinalizeIfNeeded(bot);
        if (MotionMaster* mm = bot->GetMotionMaster())
        {
            mm->Clear();
            mm->MoveIdle();
        }
        SF_LOG_INFO("modules",
            "[mod-playerbots] Init-teleport '%s' -> homebind map %u (%.1f, %.1f, %.1f) level %u.",
            bot->GetName().c_str(), mapId, x, y, z, uint32(bot->getLevel()));
        return true;
    }

    bool TeleportForLevel(Player* bot, bool fromPlayerCommand)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return false;

        uint8 const level = bot->getLevel();
        if (level < kScatterMinLevel)
        {
            // Starters stay at race spawn on auto init; player `tele` resets to hearth.
            if (!fromPlayerCommand)
                return false;
            return TeleportToHomebind(bot);
        }

        uint32 const team = bot->GetTeam();
        uint8 const race = bot->getRace();

        std::vector<TeleAnchor const*> picks;
        picks.reserve(16);
        for (TeleAnchor const& a : kAnchors)
        {
            if (level < a.minLevel || level > a.maxLevel)
                continue;
            if (a.teamFilter && a.teamFilter != team)
                continue;
            if (!RaceMatchesAnchor(race, a.raceFilter))
                continue;
            if (!CanBotTeleportTo(bot, a.mapId, a.zoneId))
                continue;
            picks.push_back(&a);
        }

        TeleAnchor fallback = {};
        TeleAnchor const* dest = nullptr;
        if (!picks.empty())
            dest = picks[std::rand() % picks.size()];
        else if (PlayerInfo const* info = sObjectMgr->GetPlayerInfo(bot->getRace(), bot->getClass()))
        {
            // Race create point as last resort (still gated by map allowlist).
            if (CanBotTeleportTo(bot, info->mapId, info->areaId))
            {
                fallback.mapId = info->mapId;
                fallback.x = info->positionX;
                fallback.y = info->positionY;
                fallback.z = info->positionZ;
                fallback.o = info->orientation;
                fallback.zoneId = info->areaId;
                dest = &fallback;
            }
        }

        if (!dest)
        {
            SF_LOG_WARN("modules",
                "[mod-playerbots] No init-teleport anchor for '%s' (level %u, team %u).",
                bot->GetName().c_str(), uint32(level), team);
            return false;
        }

        if (!bot->TeleportTo(dest->mapId, dest->x, dest->y, dest->z, dest->o))
            return false;

        FinalizeIfNeeded(bot);
        if (MotionMaster* mm = bot->GetMotionMaster())
        {
            mm->Clear();
            mm->MoveIdle();
        }

        SF_LOG_INFO("modules",
            "[mod-playerbots] Init-teleport '%s' -> map %u (%.1f, %.1f, %.1f) level %u.",
            bot->GetName().c_str(), dest->mapId, dest->x, dest->y, dest->z, uint32(level));
        return true;
    }
}
