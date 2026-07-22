/*
 * Frost Mage - simplified from Hekili MageFrost.simc
 */

#include "BotRotationLists.h"
#include "Pet.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum FrostMageSpells : uint32
    {
        FROSTBOLT           = 116,
        ICE_LANCE           = 30455,
        FROSTFIRE_BOLT      = 44614,
        FROZEN_ORB          = 84714,
        ICY_VEINS           = 12472,
        DEEP_FREEZE         = 44572,
        WATER_ELEMENTAL     = 31687,
        BRAIN_FREEZE        = 57761,
        FINGERS_OF_FROST    = 44544,
        MIRROR_IMAGE        = 55342,
        ALTER_TIME          = 108978,
    };
}

uint32 SelectFrostMage(Context const& ctx)
{
    Player* bot = ctx.bot;

    Pet* pet = bot->GetPet();
    if ((!pet || !pet->IsAlive()) && CanTryCast(bot, WATER_ELEMENTAL))
        return WATER_ELEMENTAL;

    if (CanTryCast(bot, ICY_VEINS))
        return ICY_VEINS;
    if (CanTryCast(bot, MIRROR_IMAGE))
        return MIRROR_IMAGE;

    if (CanTryCast(bot, FROZEN_ORB))
        return FROZEN_ORB;

    if (HasAuraUp(bot, BRAIN_FREEZE) && CanTryCast(bot, FROSTFIRE_BOLT))
        return FROSTFIRE_BOLT;

    if (AuraStacks(bot, FINGERS_OF_FROST) >= 1)
    {
        if (CanTryCast(bot, DEEP_FREEZE))
            return DEEP_FREEZE;
        if (CanTryCast(bot, ICE_LANCE))
            return ICE_LANCE;
    }

    if (CanTryCast(bot, FROSTBOLT))
        return FROSTBOLT;

    return 0;
}

} // namespace BotRotation
