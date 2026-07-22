/*
 * Combat Rogue - simplified from Hekili RogueCombat.simc
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum CombatSpells : uint32
    {
        SINISTER_STRIKE     = 1752,
        REVEALING_STRIKE    = 84617,
        EVISCERATE          = 2098,
        SLICE_AND_DICE      = 5171,
        ADRENALINE_RUSH     = 13750,
        KILLING_SPREE       = 51690,
        SHADOW_BLADES       = 121471,
        BLADE_FLURRY        = 13877,
        RUPTURE             = 1943,
        FAN_OF_KNIVES       = 51723,
        CRIMSON_TEMPEST     = 121411,
    };
}

uint32 SelectCombat(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    int8 const cp = ctx.comboPoints;
    uint32 const energy = bot->GetPower(POWER_ENERGY);

    if (ctx.enemies >= 2 && !HasAuraUp(bot, BLADE_FLURRY) && CanTryCast(bot, BLADE_FLURRY))
        return BLADE_FLURRY;
    if (ctx.enemies < 2 && HasAuraUp(bot, BLADE_FLURRY) && CanTryCast(bot, BLADE_FLURRY))
        return BLADE_FLURRY; // toggle off when ST

    if (CanTryCast(bot, SHADOW_BLADES))
        return SHADOW_BLADES;

    if (energy < 50 && CanTryCast(bot, KILLING_SPREE))
        return KILLING_SPREE;

    if ((energy < 35 || HasAuraUp(bot, SHADOW_BLADES)) && CanTryCast(bot, ADRENALINE_RUSH))
        return ADRENALINE_RUSH;

    if ((!HasAuraUp(bot, SLICE_AND_DICE) || AuraRemains(bot, SLICE_AND_DICE) <= 2.0f)
        && cp >= 1 && CanTryCast(bot, SLICE_AND_DICE))
        return SLICE_AND_DICE;

    if (cp >= 5)
    {
        if (ctx.enemies >= 4 && CanTryCast(bot, CRIMSON_TEMPEST))
            return CRIMSON_TEMPEST;
        if (ctx.enemies <= 2 && !HasAuraUp(bot, BLADE_FLURRY)
            && (!HasAuraUp(target, RUPTURE) || AuraRemains(target, RUPTURE) <= 4.0f)
            && CanTryCast(bot, RUPTURE))
            return RUPTURE;
        if (CanTryCast(bot, EVISCERATE))
            return EVISCERATE;
    }

    if (ctx.enemies >= 4 && CanTryCast(bot, FAN_OF_KNIVES))
        return FAN_OF_KNIVES;

    if ((!HasAuraUp(target, REVEALING_STRIKE) || AuraRemains(target, REVEALING_STRIKE) <= 3.0f)
        && CanTryCast(bot, REVEALING_STRIKE))
        return REVEALING_STRIKE;

    if (CanTryCast(bot, SINISTER_STRIKE))
        return SINISTER_STRIKE;

    return 0;
}

} // namespace BotRotation
