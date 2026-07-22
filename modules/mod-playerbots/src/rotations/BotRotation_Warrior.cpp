/*
 * Arms / Fury Warrior - simplified from Hekili WarriorArms/Fury.simc
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum WarriorSpells : uint32
    {
        BATTLE_SHOUT        = 6673,
        BERSERKER_STANCE    = 2458,
        COLOSSUS_SMASH      = 86346,
        COLOSSUS_SMASH_DEBUFF = 108126, // may also be 86346 as debuff
        MORTAL_STRIKE       = 12294,
        OVERPOWER           = 7384,
        SLAM                = 1464,
        EXECUTE             = 5308,
        RECKLESSNESS        = 1719,
        THUNDER_CLAP        = 6343,
        SWEEPING_STRIKES    = 12328,
        BLOODTHIRST         = 23881,
        RAGING_BLOW         = 85288,
        WILD_STRIKE         = 100130,
        BERSERKER_RAGE      = 18499,
        SUDDEN_DEATH        = 52437,
        TASTE_FOR_BLOOD     = 60503,
        RAGING_BLOW_STACKS  = 131116,
        ENRAGE              = 12880,
        DRAGON_ROAR         = 118000,
        STORM_BOLT          = 107570,
        BLADESTORM          = 46924,
    };
}

uint32 SelectArms(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const rage = bot->GetPower(POWER_RAGE);
    bool const cs = HasAuraUp(target, COLOSSUS_SMASH) || HasAuraUp(target, COLOSSUS_SMASH_DEBUFF)
        || AuraRemains(target, COLOSSUS_SMASH) > 0.0f;

    if (!HasAuraUp(bot, BATTLE_SHOUT) && CanTryCast(bot, BATTLE_SHOUT))
        return BATTLE_SHOUT;

    if (ctx.enemies >= 2 && CanTryCast(bot, SWEEPING_STRIKES))
        return SWEEPING_STRIKES;

    if (AuraRemains(target, COLOSSUS_SMASH) < 1.5f && CanTryCast(bot, COLOSSUS_SMASH))
        return COLOSSUS_SMASH;

    if (cs && CanTryCast(bot, RECKLESSNESS))
        return RECKLESSNESS;

    if (CanTryCast(bot, BERSERKER_RAGE))
        return BERSERKER_RAGE;

    if (ctx.enemies >= 2 && CanTryCast(bot, THUNDER_CLAP))
        return THUNDER_CLAP;
    if (ctx.enemies >= 2 && CanTryCast(bot, BLADESTORM))
        return BLADESTORM;
    if (ctx.enemies >= 2 && CanTryCast(bot, DRAGON_ROAR))
        return DRAGON_ROAR;

    if (ctx.targetHealthPct < 20.0f || HasAuraUp(bot, SUDDEN_DEATH))
        if (CanTryCast(bot, EXECUTE))
            return EXECUTE;

    if (CanTryCast(bot, MORTAL_STRIKE))
        return MORTAL_STRIKE;

    if (cs && AuraStacks(bot, TASTE_FOR_BLOOD) >= 1 && CanTryCast(bot, OVERPOWER))
        return OVERPOWER;

    if (CanTryCast(bot, STORM_BOLT))
        return STORM_BOLT;
    if (cs && CanTryCast(bot, DRAGON_ROAR))
        return DRAGON_ROAR;

    if ((cs && rage >= 25) || rage >= 60)
        if (CanTryCast(bot, SLAM))
            return SLAM;

    if (CanTryCast(bot, OVERPOWER))
        return OVERPOWER;

    return 0;
}

uint32 SelectFury(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const rage = bot->GetPower(POWER_RAGE);
    bool const cs = HasAuraUp(target, COLOSSUS_SMASH) || AuraRemains(target, COLOSSUS_SMASH) > 0.0f;

    if (!HasAuraUp(bot, BERSERKER_STANCE) && CanTryCast(bot, BERSERKER_STANCE))
        return BERSERKER_STANCE;
    if (!HasAuraUp(bot, BATTLE_SHOUT) && CanTryCast(bot, BATTLE_SHOUT))
        return BATTLE_SHOUT;

    if (!HasAuraUp(bot, ENRAGE) && CanTryCast(bot, BERSERKER_RAGE))
        return BERSERKER_RAGE;

    if (AuraRemains(target, COLOSSUS_SMASH) < 1.5f && CanTryCast(bot, COLOSSUS_SMASH))
        return COLOSSUS_SMASH;

    if (cs && CanTryCast(bot, RECKLESSNESS))
        return RECKLESSNESS;

    if (ctx.enemies >= 2 && CanTryCast(bot, BLADESTORM))
        return BLADESTORM;
    if (ctx.enemies >= 2 && CanTryCast(bot, DRAGON_ROAR))
        return DRAGON_ROAR;
    if (ctx.enemies >= 2 && CanTryCast(bot, THUNDER_CLAP))
        return THUNDER_CLAP;

    if (ctx.targetHealthPct < 20.0f && CanTryCast(bot, EXECUTE))
        return EXECUTE;

    if (!HasAuraUp(bot, ENRAGE) || CanTryCast(bot, BLOODTHIRST))
        if (CanTryCast(bot, BLOODTHIRST))
            return BLOODTHIRST;

    if (cs && AuraStacks(bot, RAGING_BLOW_STACKS) >= 1 && CanTryCast(bot, RAGING_BLOW))
        return RAGING_BLOW;

    if (CanTryCast(bot, STORM_BOLT))
        return STORM_BOLT;
    if (cs && CanTryCast(bot, DRAGON_ROAR))
        return DRAGON_ROAR;

    if (AuraStacks(bot, RAGING_BLOW_STACKS) >= 1 && CanTryCast(bot, RAGING_BLOW))
        return RAGING_BLOW;

    if (rage >= 60 && CanTryCast(bot, WILD_STRIKE))
        return WILD_STRIKE;

    if (CanTryCast(bot, BLOODTHIRST))
        return BLOODTHIRST;

    return 0;
}

} // namespace BotRotation
