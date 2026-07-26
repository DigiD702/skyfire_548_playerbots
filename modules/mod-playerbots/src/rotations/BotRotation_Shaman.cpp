/*
 * Elemental / Enhancement — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 * Restoration healers live in BotRotation_Healers.cpp — not modified here.
 */

#include "BotRotationLists.h"
#include "Item.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum EleSpells : uint32
    {
        LIGHTNING_SHIELD    = 324,
        FLAME_SHOCK         = 8050,
        LAVA_BURST          = 51505,
        EARTH_SHOCK         = 8042,
        LIGHTNING_BOLT      = 403,
    };

    enum EnhSpells : uint32
    {
        LIGHTNING_SHIELD_ENH = 324,
        WINDFURY_WEAPON      = 8232,
        FLAMETONGUE_WEAPON   = 8024,
        FLAME_SHOCK_ENH      = 8050,
        STORMSTRIKE          = 17364,
        LAVA_LASH            = 60103,
        UNLEASH_ELEMENTS     = 73680,
        LIGHTNING_BOLT_ENH   = 403,
        MAELSTROM_WEAPON     = 53817,
    };

    bool HasMainhandWeaponImbue(Player* bot)
    {
        if (!bot)
            return false;
        if (Item* mh = bot->GetWeaponForAttack(WeaponAttackType::BASE_ATTACK))
            return mh->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT) != 0;
        return false;
    }

    bool HasOffhandWeaponImbue(Player* bot)
    {
        if (!bot)
            return false;
        if (Item* oh = bot->GetWeaponForAttack(WeaponAttackType::OFF_ATTACK))
            return oh->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT) != 0;
        return false;
    }
}

uint32 SelectElemental(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const lsStacks = AuraStacks(bot, LIGHTNING_SHIELD);

    // Buff: Lightning Shield
    if (!HasAuraUp(bot, LIGHTNING_SHIELD) && CanTryCast(bot, LIGHTNING_SHIELD))
        return LIGHTNING_SHIELD;

    if (NeedsMyAuraRefresh(bot, target, FLAME_SHOCK, 3.0f) && CanTryCast(bot, FLAME_SHOCK))
        return FLAME_SHOCK;

    if (CanTryCast(bot, LAVA_BURST))
        return LAVA_BURST;

    if (lsStacks >= 6 && CanTryCast(bot, EARTH_SHOCK))
        return EARTH_SHOCK;

    if (CanTryCast(bot, LIGHTNING_BOLT))
        return LIGHTNING_BOLT;

    return 0;
}

uint32 SelectEnhancement(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const mw = AuraStacks(bot, MAELSTROM_WEAPON);

    // Buffs: Windfury (main hand), Flametongue (off-hand), Lightning Shield
    if (!HasMainhandWeaponImbue(bot) && CanTryCast(bot, WINDFURY_WEAPON))
        return WINDFURY_WEAPON;
    if (!HasOffhandWeaponImbue(bot) && CanTryCast(bot, FLAMETONGUE_WEAPON))
        return FLAMETONGUE_WEAPON;
    if (!HasAuraUp(bot, LIGHTNING_SHIELD_ENH) && CanTryCast(bot, LIGHTNING_SHIELD_ENH))
        return LIGHTNING_SHIELD_ENH;

    if (NeedsMyAuraRefresh(bot, target, FLAME_SHOCK_ENH, 3.0f) && CanTryCast(bot, FLAME_SHOCK_ENH))
        return FLAME_SHOCK_ENH;

    if (CanTryCast(bot, STORMSTRIKE))
        return STORMSTRIKE;

    if (CanTryCast(bot, LAVA_LASH))
        return LAVA_LASH;

    if (CanTryCast(bot, UNLEASH_ELEMENTS))
        return UNLEASH_ELEMENTS;

    if (mw >= 5 && CanTryCast(bot, LIGHTNING_BOLT_ENH))
        return LIGHTNING_BOLT_ENH;

    return 0;
}

} // namespace BotRotation
