/*
 * Mage DPS - Arcane / Fire / Frost (MoP 5.4.8 simplified priorities).
 */

#include "BotRotationLists.h"
#include "Pet.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
uint32 SelectFrostMage(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;

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
        FROST_BOMB          = 112948,
        NETHER_TEMPEST      = 114923,
        BLIZZARD            = 10,
        CONE_OF_COLD        = 120,
        MIRROR_IMAGE        = 55342,
        RUNE_OF_POWER       = 116011,
    };

    Pet* pet = bot->GetPet();
    if ((!pet || !pet->IsAlive()) && CanTryCast(bot, WATER_ELEMENTAL))
        return WATER_ELEMENTAL;

    if (CanTryCast(bot, ICY_VEINS))
        return ICY_VEINS;
    if (CanTryCast(bot, MIRROR_IMAGE))
        return MIRROR_IMAGE;
    if (CanTryCast(bot, RUNE_OF_POWER) && !HasAuraUp(bot, RUNE_OF_POWER))
        return RUNE_OF_POWER;

    if (ctx.enemies >= 3)
    {
        if (CanTryCast(bot, FROZEN_ORB))
            return FROZEN_ORB;
        if (CanTryCast(bot, BLIZZARD))
            return BLIZZARD;
        if (CanTryCast(bot, CONE_OF_COLD))
            return CONE_OF_COLD;
    }

    if (CanTryCast(bot, FROZEN_ORB))
        return FROZEN_ORB;

    if ((!HasAuraUp(target, FROST_BOMB) || AuraRemains(target, FROST_BOMB) <= 1.0f)
        && CanTryCast(bot, FROST_BOMB))
        return FROST_BOMB;
    if ((!HasAuraUp(target, NETHER_TEMPEST) || AuraRemains(target, NETHER_TEMPEST) <= 1.5f)
        && CanTryCast(bot, NETHER_TEMPEST))
        return NETHER_TEMPEST;

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
    if (CanTryCast(bot, ICE_LANCE))
        return ICE_LANCE;

    return 0;
}

uint32 SelectFire(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;

    enum FireSpells : uint32
    {
        FIREBALL            = 133,
        PYROBLAST           = 11366,
        HOT_STREAK          = 48108,
        INFERNO_BLAST       = 108853,
        COMBUSTION          = 11129,
        LIVING_BOMB         = 44457,
        NETHER_TEMPEST      = 114923,
        SCORCH              = 2948,
        MOLTEN_ARMOR        = 30482,
        DRAGONS_BREATH      = 31661,
        FLAMESTRIKE         = 2120,
        BLAST_WAVE          = 11113,
        MIRROR_IMAGE        = 55342,
        RUNE_OF_POWER       = 116011,
    };

    if (!HasAuraUp(bot, MOLTEN_ARMOR) && CanTryCast(bot, MOLTEN_ARMOR))
        return MOLTEN_ARMOR;

    if (CanTryCast(bot, MIRROR_IMAGE))
        return MIRROR_IMAGE;
    if (CanTryCast(bot, RUNE_OF_POWER) && !HasAuraUp(bot, RUNE_OF_POWER))
        return RUNE_OF_POWER;

    if (HasAuraUp(bot, HOT_STREAK) && CanTryCast(bot, PYROBLAST))
        return PYROBLAST;

    // Combustion with Ignite/DoTs up (Living Bomb as proxy for Ignite window).
    if (CanTryCast(bot, COMBUSTION)
        && (HasAuraUp(target, LIVING_BOMB) || HasAuraUp(target, NETHER_TEMPEST)))
        return COMBUSTION;

    if (ctx.enemies >= 3)
    {
        if (CanTryCast(bot, DRAGONS_BREATH))
            return DRAGONS_BREATH;
        if (CanTryCast(bot, FLAMESTRIKE))
            return FLAMESTRIKE;
        if (CanTryCast(bot, BLAST_WAVE))
            return BLAST_WAVE;
    }

    if ((!HasAuraUp(target, LIVING_BOMB) || AuraRemains(target, LIVING_BOMB) <= 2.0f)
        && CanTryCast(bot, LIVING_BOMB))
        return LIVING_BOMB;
    if ((!HasAuraUp(target, NETHER_TEMPEST) || AuraRemains(target, NETHER_TEMPEST) <= 1.5f)
        && CanTryCast(bot, NETHER_TEMPEST))
        return NETHER_TEMPEST;

    if (CanTryCast(bot, INFERNO_BLAST))
        return INFERNO_BLAST;

    if (CanTryCast(bot, FIREBALL))
        return FIREBALL;
    if (CanTryCast(bot, SCORCH))
        return SCORCH;

    return 0;
}

uint32 SelectArcane(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;

    enum ArcaneSpells : uint32
    {
        ARCANE_BLAST        = 30451,
        ARCANE_MISSILES     = 5143,
        ARCANE_BARRAGE      = 44425,
        ARCANE_POWER        = 12042,
        ARCANE_CHARGE       = 36032,
        ARCANE_BRILLIANCE   = 1459,
        MAGE_ARMOR          = 6117,
        ARCANE_MISSILES_PROC = 79683,
        ARCANE_EXPLOSION    = 1449,
        NETHER_TEMPEST      = 114923,
        LIVING_BOMB         = 44457,
        PRESENCE_OF_MIND    = 12043,
        EVOCATION           = 12051,
        MIRROR_IMAGE        = 55342,
        RUNE_OF_POWER       = 116011,
    };

    if (!HasAuraUp(bot, ARCANE_BRILLIANCE) && CanTryCast(bot, ARCANE_BRILLIANCE))
        return ARCANE_BRILLIANCE;
    if (!HasAuraUp(bot, MAGE_ARMOR) && CanTryCast(bot, MAGE_ARMOR))
        return MAGE_ARMOR;

    float const manaPct = bot->GetMaxPower(POWER_MANA)
        ? (100.0f * float(bot->GetPower(POWER_MANA)) / float(bot->GetMaxPower(POWER_MANA))) : 100.0f;

    if (manaPct < 20.0f && CanTryCast(bot, EVOCATION))
        return EVOCATION;

    if (CanTryCast(bot, MIRROR_IMAGE))
        return MIRROR_IMAGE;
    if (CanTryCast(bot, RUNE_OF_POWER) && !HasAuraUp(bot, RUNE_OF_POWER))
        return RUNE_OF_POWER;

    uint32 const charges = AuraStacks(bot, ARCANE_CHARGE);

    if (ctx.enemies >= 4 && charges >= 1 && CanTryCast(bot, ARCANE_EXPLOSION))
        return ARCANE_EXPLOSION;

    if ((!HasAuraUp(target, NETHER_TEMPEST) || AuraRemains(target, NETHER_TEMPEST) <= 1.5f)
        && CanTryCast(bot, NETHER_TEMPEST))
        return NETHER_TEMPEST;
    if ((!HasAuraUp(target, LIVING_BOMB) || AuraRemains(target, LIVING_BOMB) <= 2.0f)
        && CanTryCast(bot, LIVING_BOMB))
        return LIVING_BOMB;

    if (charges >= 4 && CanTryCast(bot, ARCANE_POWER))
        return ARCANE_POWER;
    if (charges >= 4 && CanTryCast(bot, PRESENCE_OF_MIND))
        return PRESENCE_OF_MIND;

    if (charges >= 4 && HasAuraUp(bot, ARCANE_MISSILES_PROC) && CanTryCast(bot, ARCANE_MISSILES))
        return ARCANE_MISSILES;

    if (charges >= 4 && manaPct < 50.0f && CanTryCast(bot, ARCANE_BARRAGE))
        return ARCANE_BARRAGE;

    if (CanTryCast(bot, ARCANE_BLAST))
        return ARCANE_BLAST;
    if (CanTryCast(bot, ARCANE_BARRAGE))
        return ARCANE_BARRAGE;

    return 0;
}

} // namespace BotRotation
