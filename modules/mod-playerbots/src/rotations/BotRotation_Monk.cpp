/*
 * Windwalker Monk - simplified from Hekili MonkWindwalker.simc
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum WwSpells : uint32
    {
        JAB                 = 100780,
        TIGER_PALM          = 100787,
        BLACKOUT_KICK       = 100784,
        RISING_SUN_KICK     = 107428,
        RISING_SUN_DEBUFF   = 130320,
        FISTS_OF_FURY       = 113656,
        SPINNING_CRANE_KICK = 101546,
        TIGEREYE_BREW_STACK = 125195,
        TIGEREYE_BREW_USE   = 116740,
        ENERGIZING_BREW     = 115288,
        TOUCH_OF_DEATH      = 115080,
        CHI_WAVE            = 115098,
        CHI_BURST           = 123986,
        RUSHING_JADE_WIND   = 116847,
        TIGER_POWER         = 125359,
        COMBO_BREAKER_BOK   = 116768,
        COMBO_BREAKER_TP    = 118864,
        DEATH_NOTE          = 121125,
        INVOKE_XUEN         = 123904,
    };
}

uint32 SelectWindwalker(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const chi = bot->GetPower(POWER_CHI);
    uint32 const chiMax = bot->GetMaxPower(POWER_CHI);
    uint32 const energy = bot->GetPower(POWER_ENERGY);
    bool const tigerPower = HasAuraUp(bot, TIGER_POWER);
    float const tigerPowerRemains = AuraRemains(bot, TIGER_POWER);
    bool const rskUp = HasAuraUp(target, RISING_SUN_DEBUFF);
    uint32 const tebStacks = AuraStacks(bot, TIGEREYE_BREW_STACK);
    bool const tebUse = HasAuraUp(bot, TIGEREYE_BREW_USE);

    // Maintain Tiger Power
    if ((!tigerPower || tigerPowerRemains <= 3.0f) && chi >= 1 && CanTryCast(bot, TIGER_PALM))
        return TIGER_PALM;

    // Dump Tigereye Brew at high stacks
    if (!tebUse && tebStacks >= 15 && CanTryCast(bot, TIGEREYE_BREW_USE))
        return TIGEREYE_BREW_USE;

    if (energy < 40 && CanTryCast(bot, ENERGIZING_BREW))
        return ENERGIZING_BREW;

    if (!rskUp && chi >= 2 && CanTryCast(bot, RISING_SUN_KICK))
        return RISING_SUN_KICK;

    if (HasAuraUp(bot, DEATH_NOTE) && chi >= 3 && CanTryCast(bot, TOUCH_OF_DEATH))
        return TOUCH_OF_DEATH;

    if (CanTryCast(bot, INVOKE_XUEN))
        return INVOKE_XUEN;

    // AoE branch
    if (ctx.enemies >= 3)
    {
        if (CanTryCast(bot, RUSHING_JADE_WIND))
            return RUSHING_JADE_WIND;
        if (CanTryCast(bot, CHI_WAVE))
            return CHI_WAVE;
        if (CanTryCast(bot, CHI_BURST))
            return CHI_BURST;
        if (!CanTryCast(bot, RUSHING_JADE_WIND) && CanTryCast(bot, SPINNING_CRANE_KICK))
            return SPINNING_CRANE_KICK;
        if (chi == chiMax && CanTryCast(bot, RISING_SUN_KICK))
            return RISING_SUN_KICK;
    }

    // Single target
    if (tebUse && tigerPower && chi >= 3 && CanTryCast(bot, FISTS_OF_FURY))
        return FISTS_OF_FURY;

    if (chi >= 2 && CanTryCast(bot, RISING_SUN_KICK))
        return RISING_SUN_KICK;

    if (CanTryCast(bot, CHI_WAVE))
        return CHI_WAVE;
    if (CanTryCast(bot, CHI_BURST))
        return CHI_BURST;

    if (HasAuraUp(bot, COMBO_BREAKER_BOK) && chi >= 2 && CanTryCast(bot, BLACKOUT_KICK))
        return BLACKOUT_KICK;
    if (HasAuraUp(bot, COMBO_BREAKER_TP) && chi >= 1 && CanTryCast(bot, TIGER_PALM))
        return TIGER_PALM;

    if (chiMax >= chi + 2 && CanTryCast(bot, JAB))
        return JAB;

    if (chi >= 2 && CanTryCast(bot, BLACKOUT_KICK))
        return BLACKOUT_KICK;

    if (CanTryCast(bot, JAB))
        return JAB;

    return 0;
}

} // namespace BotRotation
