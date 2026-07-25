/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*
* In-memory vendor/repair hub index for same-map travel when no vendor is nearby.
*/

#ifndef _SF_BOT_VENDOR_HUBS_H
#define _SF_BOT_VENDOR_HUBS_H

#include "Define.h"
#include <unordered_map>
#include <vector>

class Player;

struct BotVendorHub
{
    uint32 spawnGuid = 0;
    uint32 entry = 0;
    uint16 mapId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    uint32 faction = 0;
    bool canRepair = false;
};

class BotVendorHubs
{
public:
    static BotVendorHubs* instance();

    void Load();
    bool IsLoaded() const { return _loaded; }
    uint32 GetCount() const;

    // Nearest same-map hub the player is friendly toward. preferRepair biases
    // repair-capable vendors when both sell and repair are needed.
    BotVendorHub const* FindNearest(Player const* player, bool preferRepair) const;

private:
    BotVendorHubs() = default;

    bool _loaded = false;
    std::unordered_map<uint16 /*mapId*/, std::vector<BotVendorHub>> _byMap;
};

#define sBotVendorHubs BotVendorHubs::instance()

#endif
