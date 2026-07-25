/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "BotPreferredMounts.h"
#include "DatabaseEnv.h"
#include "Log.h"

BotPreferredMounts* BotPreferredMounts::instance()
{
    static BotPreferredMounts inst;
    return &inst;
}

void BotPreferredMounts::Load()
{
    _spells.clear();
    _loaded = false;

    // Ensure table exists so a fresh characters DB does not error every boot.
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `playerbots_preferred_mounts` ("
        "  `id` INT(11) NOT NULL AUTO_INCREMENT,"
        "  `guid` INT(11) NOT NULL,"
        "  `type` TINYINT(3) NOT NULL COMMENT '0: Ground, 1: Flying',"
        "  `spellid` INT(11) NOT NULL,"
        "  PRIMARY KEY (`id`),"
        "  KEY `guid` (`guid`),"
        "  KEY `type` (`type`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8");

    QueryResult result = CharacterDatabase.Query(
        "SELECT guid, type, spellid FROM playerbots_preferred_mounts");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 const guid = fields[0].GetUInt32();
            uint8 const type = fields[1].GetUInt8();
            uint32 const spell = fields[2].GetUInt32();
            if (type > Flying || !spell)
                continue;
            _spells[MakeKey(guid, Type(type))] = spell;
        } while (result->NextRow());
    }

    _loaded = true;
    SF_LOG_INFO("modules", "[mod-playerbots] BotPreferredMounts: loaded %u preferred mount rows.",
        uint32(_spells.size()));
}

uint32 BotPreferredMounts::Get(uint32 characterGuidLow, Type type) const
{
    auto it = _spells.find(MakeKey(characterGuidLow, type));
    return it != _spells.end() ? it->second : 0;
}

void BotPreferredMounts::Set(uint32 characterGuidLow, Type type, uint32 spellId)
{
    if (!characterGuidLow || !spellId)
        return;

    _spells[MakeKey(characterGuidLow, type)] = spellId;

    CharacterDatabase.PExecute(
        "DELETE FROM playerbots_preferred_mounts WHERE guid = %u AND type = %u",
        characterGuidLow, uint32(type));
    CharacterDatabase.PExecute(
        "INSERT INTO playerbots_preferred_mounts (guid, type, spellid) VALUES (%u, %u, %u)",
        characterGuidLow, uint32(type), spellId);
}

void BotPreferredMounts::Clear(uint32 characterGuidLow, Type type)
{
    _spells.erase(MakeKey(characterGuidLow, type));
    CharacterDatabase.PExecute(
        "DELETE FROM playerbots_preferred_mounts WHERE guid = %u AND type = %u",
        characterGuidLow, uint32(type));
}

void BotPreferredMounts::ClearAll(uint32 characterGuidLow)
{
    Clear(characterGuidLow, Ground);
    Clear(characterGuidLow, Flying);
}
