/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*
* Per-character preferred ground/flying mount spells (characters DB table).
*/

#ifndef _SF_BOT_PREFERRED_MOUNTS_H
#define _SF_BOT_PREFERRED_MOUNTS_H

#include "Define.h"
#include <unordered_map>

class BotPreferredMounts
{
public:
    enum Type : uint8 { Ground = 0, Flying = 1 };

    static BotPreferredMounts* instance();

    void Load();
    uint32 Get(uint32 characterGuidLow, Type type) const;
    void Set(uint32 characterGuidLow, Type type, uint32 spellId);
    void Clear(uint32 characterGuidLow, Type type);
    void ClearAll(uint32 characterGuidLow);

private:
    BotPreferredMounts() = default;

    // key = (guid << 1) | type
    static uint64 MakeKey(uint32 guid, Type type)
    {
        return (uint64(guid) << 1) | uint64(type);
    }

    std::unordered_map<uint64, uint32> _spells;
    bool _loaded = false;
};

#define sBotPreferredMounts BotPreferredMounts::instance()

#endif
