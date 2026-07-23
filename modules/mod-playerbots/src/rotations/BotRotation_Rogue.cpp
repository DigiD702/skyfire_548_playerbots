/*
 * Combat Rogue - simplified from Hekili RogueCombat.simc
 */

#include "BotRotationLists.h"
#include "Item.h"
#include "ItemPrototype.h"
#include "Player.h"
#include "SharedDefines.h"
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

uint32 SelectAssassination(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    int8 const cp = ctx.comboPoints;

    enum MutSpells : uint32
    {
        MUTILATE            = 1329,
        DISPATCH            = 111240,
        ENVENOM             = 32645,
        GARROTE             = 703,
        RUPTURE             = 1943,
        VENDETTA            = 79140,
        SLICE_AND_DICE      = 5171,
        BLINDSIDE           = 121153,
        FAN_OF_KNIVES       = 51723,
        CRIMSON_TEMPEST     = 121411,
        SHADOW_BLADES       = 121471,
        SINISTER_STRIKE     = 1752,
        HEMORRHAGE          = 16511,
        MARKED_FOR_DEATH    = 137619,
    };

    auto hasDagger = [](Player* p, WeaponAttackType at) -> bool
    {
        Item* item = p->GetWeaponForAttack(at, true);
        return item && item->GetTemplate() && item->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER;
    };
    bool const canMutilate = hasDagger(bot, WeaponAttackType::BASE_ATTACK)
        && hasDagger(bot, WeaponAttackType::OFF_ATTACK);

    // Open with Vendetta + Shadow Blades so trinket sync (IsBursting) can fire.
    if (CanTryCast(bot, VENDETTA))
        return VENDETTA;
    if (CanTryCast(bot, SHADOW_BLADES))
        return SHADOW_BLADES;

    // Marked for Death: force a finisher window when low on CP.
    if (cp <= 1 && CanTryCast(bot, MARKED_FOR_DEATH))
        return MARKED_FOR_DEATH;

    if ((!HasAuraUp(bot, SLICE_AND_DICE) || AuraRemains(bot, SLICE_AND_DICE) <= 2.0f)
        && cp >= 1 && CanTryCast(bot, SLICE_AND_DICE))
        return SLICE_AND_DICE;

    if (cp >= 5)
    {
        if (ctx.enemies >= 4 && CanTryCast(bot, CRIMSON_TEMPEST))
            return CRIMSON_TEMPEST;
        if ((!HasAuraUp(target, RUPTURE) || AuraRemains(target, RUPTURE) <= 4.0f)
            && CanTryCast(bot, RUPTURE))
            return RUPTURE;
        if (CanTryCast(bot, ENVENOM))
            return ENVENOM;
    }

    // Multi-DoT: keep Garrote up from stealth; maintain Rupture early at 4+ CP.
    if (cp >= 4 && (!HasAuraUp(target, RUPTURE) || AuraRemains(target, RUPTURE) <= 2.0f)
        && CanTryCast(bot, RUPTURE))
        return RUPTURE;

    if (ctx.enemies >= 4 && CanTryCast(bot, FAN_OF_KNIVES))
        return FAN_OF_KNIVES;

    // Garrote requires stealth — never pick it in open combat or we stall the GCD.
    if (bot->HasStealthAura()
        && (!HasAuraUp(target, GARROTE) || AuraRemains(target, GARROTE) <= 3.0f)
        && CanTryCast(bot, GARROTE))
        return GARROTE;

    if ((ctx.targetHealthPct < 35.0f || HasAuraUp(bot, BLINDSIDE))
        && hasDagger(bot, WeaponAttackType::BASE_ATTACK)
        && CanTryCast(bot, DISPATCH))
        return DISPATCH;

    if (canMutilate && CanTryCast(bot, MUTILATE))
        return MUTILATE;

    // Wrong weapons / missing Dual Wield: fall back to any known builder.
    if (CanTryCast(bot, HEMORRHAGE))
        return HEMORRHAGE;
    if (CanTryCast(bot, SINISTER_STRIKE))
        return SINISTER_STRIKE;
    if (CanTryCast(bot, FAN_OF_KNIVES))
        return FAN_OF_KNIVES;

    return 0;
}

uint32 SelectSubtlety(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    int8 const cp = ctx.comboPoints;

    enum SubSpells : uint32
    {
        BACKSTAB            = 53,
        HEMORRHAGE          = 16511,
        EVISCERATE          = 2098,
        RUPTURE             = 1943,
        SLICE_AND_DICE      = 5171,
        SHADOW_DANCE        = 51713,
        PREMEDITATION       = 14183,
        AMBUSH              = 8676,
        FAN_OF_KNIVES       = 51723,
        CRIMSON_TEMPEST     = 121411,
        SHADOW_BLADES       = 121471,
        MARKED_FOR_DEATH    = 137619,
        VANISH              = 1856,
        FIND_WEAKNESS       = 91021,
    };

    if (CanTryCast(bot, SHADOW_BLADES))
        return SHADOW_BLADES;
    if (CanTryCast(bot, SHADOW_DANCE))
        return SHADOW_DANCE;
    if (CanTryCast(bot, PREMEDITATION))
        return PREMEDITATION;

    // Vanish into Ambush when Find Weakness is missing and Dance is down.
    if (!HasAuraUp(bot, SHADOW_DANCE) && !HasAuraUp(target, FIND_WEAKNESS)
        && !bot->HasStealthAura() && CanTryCast(bot, VANISH))
        return VANISH;

    if (cp <= 1 && CanTryCast(bot, MARKED_FOR_DEATH))
        return MARKED_FOR_DEATH;

    if ((!HasAuraUp(bot, SLICE_AND_DICE) || AuraRemains(bot, SLICE_AND_DICE) <= 2.0f)
        && cp >= 1 && CanTryCast(bot, SLICE_AND_DICE))
        return SLICE_AND_DICE;

    if (cp >= 5)
    {
        if (ctx.enemies >= 4 && CanTryCast(bot, CRIMSON_TEMPEST))
            return CRIMSON_TEMPEST;
        if ((!HasAuraUp(target, RUPTURE) || AuraRemains(target, RUPTURE) <= 4.0f)
            && CanTryCast(bot, RUPTURE))
            return RUPTURE;
        if (CanTryCast(bot, EVISCERATE))
            return EVISCERATE;
    }

    if (ctx.enemies >= 4 && CanTryCast(bot, FAN_OF_KNIVES))
        return FAN_OF_KNIVES;

    if ((HasAuraUp(bot, SHADOW_DANCE) || bot->HasStealthAura()) && CanTryCast(bot, AMBUSH))
        return AMBUSH;

    // Hemorrhage maintains the bleed when not behind the target for Backstab.
    if ((!HasAuraUp(target, HEMORRHAGE) || AuraRemains(target, HEMORRHAGE) <= 3.0f)
        && CanTryCast(bot, HEMORRHAGE))
        return HEMORRHAGE;

    if (CanTryCast(bot, BACKSTAB))
        return BACKSTAB;
    if (CanTryCast(bot, HEMORRHAGE))
        return HEMORRHAGE;

    return 0;
}

} // namespace BotRotation
