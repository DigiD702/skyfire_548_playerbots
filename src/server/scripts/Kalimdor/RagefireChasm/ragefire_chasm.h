/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef RAGEFIRE_CHASM_H
#define RAGEFIRE_CHASM_H

#include "Define.h"
#include <cstdlib>

inline uint32 urand(uint32 min, uint32 max)
{
    if (max <= min)
        return min;
    return min + uint32(std::rand()) % (max - min + 1);
}

enum RfcBosses
{
    BOSS_ADAROGG                = 0,
    BOSS_DARK_SHAMAN_CORANTHAL  = 1,
    BOSS_SLAGMAW                = 2,
    BOSS_GORDOTH                = 3,
    TOTAL_ENCOUNTERS           = 4
};

enum RfcCreatures
{
    NPC_ADAROGG                 = 61408,
    NPC_DARK_SHAMAN_CORANTHAL   = 61412,
    NPC_SLAGMAW                 = 61463,
    NPC_GORDOTH                 = 61528
};

enum RfcDataTypes
{
    TYPE_INFERNO_TARGET = 100
};

#endif // RAGEFIRE_CHASM_H
