/*
 * SkyFire playerbots — level/faction gates for open-world teleport destinations.
 *
 * Summon / follow-to-master is unrestricted (the master chose the place).
 * Random / init scatter teleports must call CanBotTeleportTo() first.
 */

#ifndef _SF_BOT_TELEPORT_MAPS_H
#define _SF_BOT_TELEPORT_MAPS_H

#include "Define.h"

class Player;

namespace BotTeleportMaps
{
    // Expansion / starter continents by bot level:
    //   < 69  : Eastern Kingdoms, Kalimdor, worgen/goblin starts
    //   69-79 : + Outland
    //   80-84 : + Northrend
    //   85+   : + Cataclysm / Pandaria continents
    bool IsMapAllowedForLevel(uint32 mapId, uint32 level);

    // Hostile capital zones (Orgrimmar for Alliance, Stormwind for Horde, etc.).
    bool IsHostileCapitalZone(uint32 zoneId, uint32 team /* ALLIANCE or HORDE */);

    // True when map is in the level band and the zone is not an enemy capital.
    bool CanBotTeleportTo(Player const* bot, uint32 mapId, uint32 zoneId = 0);
}

#endif
