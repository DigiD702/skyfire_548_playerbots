/*
 * SkyFire playerbots — level/faction gates and init scatter teleports.
 *
 * Summon / follow-to-master is unrestricted (the master chose the place).
 * Random / init scatter teleports must call CanBotTeleportTo() / TeleportForLevel().
 */

#ifndef _SF_BOT_TELEPORT_MAPS_H
#define _SF_BOT_TELEPORT_MAPS_H

#include "Define.h"

class Player;

namespace BotTeleportMaps
{
    // Expansion / starter continents by bot level:
    //   always : Eastern Kingdoms, Kalimdor, Outland (incl. Draenei/BE starts),
    //            worgen/goblin/pandaren starts (race-gated in CanBotTeleportTo)
    //   80-84  : + Northrend
    //   85+    : + Cataclysm / Pandaria continents
    bool IsMapAllowedForLevel(uint32 mapId, uint32 level);

    // Hostile capital zones (Orgrimmar for Alliance, Stormwind for Horde, etc.).
    bool IsHostileCapitalZone(uint32 zoneId, uint32 team /* ALLIANCE or HORDE */);

    // True when map is in the level band, the zone is not an enemy capital, and
    // race-exclusive starts (Gilneas / Kezan / Lost Isles / Wandering Isle)
    // match the bot's race.
    bool CanBotTeleportTo(Player const* bot, uint32 mapId, uint32 zoneId = 0);

    // Pick a safe open-world anchor for the bot's *current* level and faction,
    // then TeleportTo + FinalizeBotTeleport. Call after relevel/init so the
    // destination matches the post-init level.
    //
    // Levels 1–5: auto/first-time init leaves them at race spawn (no-op).
    // Explicit player `.playerbots init … tele` sends them to homebind / race
    // create point instead of a random leveling hub.
    bool TeleportForLevel(Player* bot, bool fromPlayerCommand = false);

    // Hearth / homebind location (falls back to race create coords).
    bool TeleportToHomebind(Player* bot);
}

#endif
