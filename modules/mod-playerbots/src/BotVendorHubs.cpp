/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "BotVendorHubs.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Unit.h"

#include <cmath>

BotVendorHubs* BotVendorHubs::instance()
{
    static BotVendorHubs inst;
    return &inst;
}

namespace
{
    bool IsFriendlyFaction(Player const* player, uint32 factionId)
    {
        if (!player || !factionId)
            return true;
        FactionTemplateEntry const* a = player->GetFactionTemplateEntry();
        FactionTemplateEntry const* b = sFactionTemplateStore.LookupEntry(factionId);
        if (!a || !b)
            return true;
        return a->IsFriendlyTo(*b);
    }
}

void BotVendorHubs::Load()
{
    _byMap.clear();
    _loaded = false;

    // creature.npcflag can override template; take either with VENDOR bit.
    QueryResult result = WorldDatabase.Query(
        "SELECT c.guid, c.id, c.map, c.position_x, c.position_y, c.position_z, "
        "ct.npcflag, c.npcflag, ct.faction_A "
        "FROM creature c INNER JOIN creature_template ct ON c.id = ct.entry "
        "WHERE ((ct.npcflag | c.npcflag) & 128) != 0");

    if (!result)
    {
        SF_LOG_WARN("modules", "[mod-playerbots] BotVendorHubs: no vendor creatures found.");
        _loaded = true;
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        BotVendorHub hub;
        hub.spawnGuid = fields[0].GetUInt32();
        hub.entry = fields[1].GetUInt32();
        hub.mapId = fields[2].GetUInt16();
        hub.x = fields[3].GetFloat();
        hub.y = fields[4].GetFloat();
        hub.z = fields[5].GetFloat();
        uint32 const tplFlags = fields[6].GetUInt32();
        uint32 const spawnFlags = fields[7].GetUInt32();
        uint32 const flags = tplFlags | spawnFlags;
        hub.canRepair = (flags & UNIT_NPC_FLAG_REPAIR) != 0;
        hub.faction = fields[8].GetUInt32();
        _byMap[hub.mapId].push_back(hub);
        ++count;
    } while (result->NextRow());

    _loaded = true;
    SF_LOG_INFO("modules", "[mod-playerbots] BotVendorHubs: loaded %u vendor hubs across %u maps.",
        count, uint32(_byMap.size()));
}

uint32 BotVendorHubs::GetCount() const
{
    uint32 n = 0;
    for (auto const& kv : _byMap)
        n += uint32(kv.second.size());
    return n;
}

BotVendorHub const* BotVendorHubs::FindNearest(Player const* player, bool preferRepair) const
{
    if (!player || !_loaded)
        return nullptr;

    auto it = _byMap.find(uint16(player->GetMapId()));
    if (it == _byMap.end() || it->second.empty())
        return nullptr;

    float const px = player->GetPositionX();
    float const py = player->GetPositionY();
    float const pz = player->GetPositionZ();

    BotVendorHub const* best = nullptr;
    float bestDist2 = 0.0f;
    BotVendorHub const* bestRepair = nullptr;
    float bestRepairDist2 = 0.0f;

    for (BotVendorHub const& hub : it->second)
    {
        if (!IsFriendlyFaction(player, hub.faction))
            continue;

        float const dx = hub.x - px;
        float const dy = hub.y - py;
        float const dz = hub.z - pz;
        float const d2 = dx * dx + dy * dy + dz * dz;
        if (!best || d2 < bestDist2)
        {
            best = &hub;
            bestDist2 = d2;
        }
        if (hub.canRepair && (!bestRepair || d2 < bestRepairDist2))
        {
            bestRepair = &hub;
            bestRepairDist2 = d2;
        }
    }

    if (preferRepair && bestRepair)
        return bestRepair;
    return best;
}
