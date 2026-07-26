// Arcane / Fire / Frost — modules/mod-playerbots/rotation.md
// Unknown (low-level) spells are skipped via CanTryCast.

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum MageSpells : uint32
    {
        MAGE_ARMOR          = 6117,
        MOLTEN_ARMOR        = 30482,
        FROST_ARMOR         = 7302,
        ARCANE_BLAST        = 30451,
        ARCANE_MISSILES     = 5143,
        ARCANE_BARRAGE      = 44425,
        ARCANE_CHARGE       = 36032,
        ARCANE_MISSILES_PROC = 79683,
        LIVING_BOMB         = 44457,
        INFERNO_BLAST       = 108853,
        PYROBLAST           = 11366,
        HEATING_UP          = 48107,
        HOT_STREAK          = 48108,
        FIREBALL            = 133,
        NETHER_TEMPEST      = 114923,
        FROZEN_ORB          = 84714,
        ICE_LANCE           = 30455,
        FINGERS_OF_FROST    = 44544,
        FROSTFIRE_BOLT      = 44614,
        BRAIN_FREEZE        = 57761,
        FROSTBOLT           = 116,
    };

    bool NeedsDot(Player* bot, Unit* target, uint32 spellId)
    {
        return NeedsMyAuraRefresh(bot, target, spellId, 3.0f);
    }
}

uint32 SelectArcane(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    if (!HasAuraUp(bot, MAGE_ARMOR) && CanTryCast(bot, MAGE_ARMOR))
        return MAGE_ARMOR;

    uint32 const charges = AuraStacks(bot, ARCANE_CHARGE);

    if (charges < 4 && CanTryCast(bot, ARCANE_BLAST))
        return ARCANE_BLAST;
    if (HasAuraUp(bot, ARCANE_MISSILES_PROC) && CanTryCast(bot, ARCANE_MISSILES))
        return ARCANE_MISSILES;
    if (charges >= 4 && CanTryCast(bot, ARCANE_BARRAGE))
        return ARCANE_BARRAGE;
    if (CanTryCast(bot, ARCANE_BLAST))
        return ARCANE_BLAST;

    return 0;
}

uint32 SelectFire(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    if (!HasAuraUp(bot, MOLTEN_ARMOR) && CanTryCast(bot, MOLTEN_ARMOR))
        return MOLTEN_ARMOR;
    if (NeedsDot(bot, target, LIVING_BOMB) && CanTryCast(bot, LIVING_BOMB))
        return LIVING_BOMB;
    if (HasAuraUp(bot, HEATING_UP) && CanTryCast(bot, INFERNO_BLAST))
        return INFERNO_BLAST;
    if (HasAuraUp(bot, HOT_STREAK) && CanTryCast(bot, PYROBLAST))
        return PYROBLAST;
    if (CanTryCast(bot, FIREBALL))
        return FIREBALL;

    return 0;
}

uint32 SelectFrostMage(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    if (!HasAuraUp(bot, FROST_ARMOR) && CanTryCast(bot, FROST_ARMOR))
        return FROST_ARMOR;
    if (NeedsDot(bot, target, LIVING_BOMB) && CanTryCast(bot, LIVING_BOMB))
        return LIVING_BOMB;
    if (NeedsDot(bot, target, NETHER_TEMPEST) && CanTryCast(bot, NETHER_TEMPEST))
        return NETHER_TEMPEST;
    if (CanTryCast(bot, FROZEN_ORB))
        return FROZEN_ORB;
    if (AuraStacks(bot, FINGERS_OF_FROST) >= 1 && CanTryCast(bot, ICE_LANCE))
        return ICE_LANCE;
    if (HasAuraUp(bot, BRAIN_FREEZE) && CanTryCast(bot, FROSTFIRE_BOLT))
        return FROSTFIRE_BOLT;
    if (CanTryCast(bot, FROSTBOLT))
        return FROSTBOLT;

    return 0;
}

} // namespace BotRotation
