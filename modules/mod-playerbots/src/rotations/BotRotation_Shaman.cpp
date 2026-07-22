/*
 * Elemental Shaman - simplified from Hekili ShamanElemental.simc
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
        FLAMETONGUE_WEAPON  = 8024,
        FLAME_SHOCK         = 8050,
        LAVA_BURST          = 51505,
        EARTH_SHOCK         = 8042,
        LIGHTNING_BOLT      = 403,
        CHAIN_LIGHTNING     = 421,
        ASCENDANCE          = 114049,
        ASCENDANCE_BUFF     = 114050,
        ELEMENTAL_BLAST     = 117014,
        ELEMENTAL_MASTERY   = 16166,
        SEARING_TOTEM       = 3599,
        MAGMA_TOTEM         = 8190,
        EARTHQUAKE          = 61882,
        UNLEASH_ELEMENTS    = 73680,
        LAVA_SURGE          = 77762,
        FIRE_ELEMENTAL      = 2894,
        EARTH_ELEMENTAL     = 2062,
        THUNDERSTORM        = 51490,
        SPIRITWALKERS_GRACE = 79206,
        LAVA_BEAM           = 114209,
        ANCESTRAL_SWIFTNESS = 16188,
    };

    bool HasMainhandWeaponImbue(Player* bot)
    {
        if (!bot)
            return false;
        if (Item* mh = bot->GetWeaponForAttack(WeaponAttackType::BASE_ATTACK))
            return mh->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT) != 0;
        return false;
    }

    bool HasActiveTotem(Player* bot)
    {
        if (!bot)
            return false;
        for (uint8 i = SUMMON_SLOT_TOTEM; i < MAX_TOTEM_SLOT; ++i)
            if (bot->m_SummonSlot[i])
                return true;
        return false;
    }
}

uint32 SelectElemental(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;

    // Weapon imbues are temporary enchants, not auras — only refresh when missing.
    if (!bot->IsInCombat())
    {
        if (!HasAuraUp(bot, LIGHTNING_SHIELD) && CanTryCast(bot, LIGHTNING_SHIELD))
            return LIGHTNING_SHIELD;
        if (!HasMainhandWeaponImbue(bot) && CanTryCast(bot, FLAMETONGUE_WEAPON))
            return FLAMETONGUE_WEAPON;
    }
    else if (!HasAuraUp(bot, LIGHTNING_SHIELD) && CanTryCast(bot, LIGHTNING_SHIELD))
        return LIGHTNING_SHIELD;

    bool const fsUp = HasAuraUp(target, FLAME_SHOCK);
    float const fsRemains = AuraRemains(target, FLAME_SHOCK);
    bool const ascendance = HasAuraUp(bot, ASCENDANCE_BUFF) || HasAuraUp(bot, ASCENDANCE);
    uint32 const lsStacks = AuraStacks(bot, LIGHTNING_SHIELD);
    // Glyph of Chain Lightning (55449): CL earlier on 2+ targets.
    bool const glyphCL = HasGlyphSpell(bot, 55449);
    uint32 const aoeThresh = glyphCL ? 2u : 3u;

    if (!bot->IsStopped() && CanTryCast(bot, SPIRITWALKERS_GRACE) && !HasAuraUp(bot, SPIRITWALKERS_GRACE))
        return SPIRITWALKERS_GRACE;

    if (CanTryCast(bot, ELEMENTAL_MASTERY))
        return ELEMENTAL_MASTERY;

    if (fsUp && CanTryCast(bot, ASCENDANCE))
        return ASCENDANCE;

    if (CanTryCast(bot, FIRE_ELEMENTAL))
        return FIRE_ELEMENTAL;

    // AoE
    if (ctx.enemies >= aoeThresh)
    {
        if (ctx.enemies >= 4 && !HasActiveTotem(bot) && CanTryCast(bot, MAGMA_TOTEM))
            return MAGMA_TOTEM;
        if (ctx.enemies >= 4 && CanTryCast(bot, EARTHQUAKE))
            return EARTHQUAKE;
        if ((!fsUp || fsRemains < 2.0f) && CanTryCast(bot, FLAME_SHOCK))
            return FLAME_SHOCK;
        if (ascendance && CanTryCast(bot, LAVA_BEAM))
            return LAVA_BEAM;
        if (HasAuraUp(bot, LAVA_SURGE) && CanTryCast(bot, LAVA_BURST))
            return LAVA_BURST;
        if (lsStacks >= 7 && !ascendance && CanTryCast(bot, EARTH_SHOCK))
            return EARTH_SHOCK;
        if (CanTryCast(bot, CHAIN_LIGHTNING))
            return CHAIN_LIGHTNING;
    }

    // Unleash needs a weapon imbue; without one CheckCast always fails.
    if (HasMainhandWeaponImbue(bot) && CanTryCast(bot, UNLEASH_ELEMENTS) && !ascendance)
        return UNLEASH_ELEMENTS;

    if ((!fsUp || fsRemains < 3.0f) && CanTryCast(bot, FLAME_SHOCK))
        return FLAME_SHOCK;

    // Ascendance turns LB into Lava Beam — prefer it over Lava Burst spam.
    if (ascendance && CanTryCast(bot, LAVA_BEAM))
        return LAVA_BEAM;
    if (ascendance && CanTryCast(bot, LAVA_BURST))
        return LAVA_BURST;

    if (HasAuraUp(bot, LAVA_SURGE) && CanTryCast(bot, LAVA_BURST))
        return LAVA_BURST;

    // Do not refresh totems every GCD — that blocked the entire damage rotation.
    if (!HasActiveTotem(bot) && CanTryCast(bot, SEARING_TOTEM))
        return SEARING_TOTEM;

    if (CanTryCast(bot, ELEMENTAL_BLAST))
        return ELEMENTAL_BLAST;

    if (fsUp && CanTryCast(bot, LAVA_BURST))
        return LAVA_BURST;

    if (lsStacks >= 7 && !ascendance && (fsRemains > 6.0f) && CanTryCast(bot, EARTH_SHOCK))
        return EARTH_SHOCK;

    if (CanTryCast(bot, ANCESTRAL_SWIFTNESS) && CanTryCast(bot, ELEMENTAL_BLAST))
        return ANCESTRAL_SWIFTNESS;

    if (CanTryCast(bot, LIGHTNING_BOLT))
        return LIGHTNING_BOLT;

    if (CanTryCast(bot, THUNDERSTORM))
        return THUNDERSTORM;

    return 0;
}

uint32 SelectEnhancement(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;

    enum EnhSpells : uint32
    {
        LIGHTNING_SHIELD    = 324,
        WINDFURY_WEAPON     = 8232,
        FLAMETONGUE_WEAPON  = 8024,
        STORMSTRIKE         = 17364,
        STORMBLAST          = 115356,
        LAVA_LASH           = 60103,
        FLAME_SHOCK         = 8050,
        UNLEASH_ELEMENTS    = 73680,
        UNLEASH_FLAME       = 73683,
        LIGHTNING_BOLT      = 403,
        CHAIN_LIGHTNING     = 421,
        EARTH_SHOCK         = 8042,
        FROST_SHOCK         = 8056,
        FIRE_NOVA           = 1535,
        FERAL_SPIRIT        = 51533,
        ASCENDANCE          = 114049,
        ASCENDANCE_BUFF     = 114051,
        MAELSTROM_WEAPON    = 53817,
        SEARING_TOTEM       = 3599,
        MAGMA_TOTEM         = 8190,
        ELEMENTAL_BLAST     = 117014,
        FIRE_ELEMENTAL      = 2894,
    };

    if (!bot->IsInCombat())
    {
        if (!HasAuraUp(bot, LIGHTNING_SHIELD) && CanTryCast(bot, LIGHTNING_SHIELD))
            return LIGHTNING_SHIELD;
        if (!HasMainhandWeaponImbue(bot) && CanTryCast(bot, WINDFURY_WEAPON))
            return WINDFURY_WEAPON;
        if (!HasMainhandWeaponImbue(bot) && CanTryCast(bot, FLAMETONGUE_WEAPON))
            return FLAMETONGUE_WEAPON;
    }
    else if (!HasAuraUp(bot, LIGHTNING_SHIELD) && CanTryCast(bot, LIGHTNING_SHIELD))
        return LIGHTNING_SHIELD;

    bool const ascendance = HasAuraUp(bot, ASCENDANCE_BUFF) || HasAuraUp(bot, ASCENDANCE);
    uint32 const mw = AuraStacks(bot, MAELSTROM_WEAPON);
    bool const fsUp = HasAuraUp(target, FLAME_SHOCK);

    if (CanTryCast(bot, FIRE_ELEMENTAL))
        return FIRE_ELEMENTAL;
    if (!ascendance && CanTryCast(bot, ASCENDANCE))
        return ASCENDANCE;
    if (CanTryCast(bot, FERAL_SPIRIT))
        return FERAL_SPIRIT;

    if (ctx.enemies >= 2)
    {
        if (ctx.enemies >= 4 && !HasActiveTotem(bot) && CanTryCast(bot, MAGMA_TOTEM))
            return MAGMA_TOTEM;
        if (!HasActiveTotem(bot) && CanTryCast(bot, SEARING_TOTEM))
            return SEARING_TOTEM;
        if (fsUp && CanTryCast(bot, FIRE_NOVA))
            return FIRE_NOVA;
        if (mw >= 3 && CanTryCast(bot, CHAIN_LIGHTNING))
            return CHAIN_LIGHTNING;
        if (!fsUp && CanTryCast(bot, FLAME_SHOCK))
            return FLAME_SHOCK;
        if (ascendance && CanTryCast(bot, STORMBLAST))
            return STORMBLAST;
        if (fsUp && CanTryCast(bot, LAVA_LASH))
            return LAVA_LASH;
        if (!ascendance && CanTryCast(bot, STORMSTRIKE))
            return STORMSTRIKE;
        if (CanTryCast(bot, EARTH_SHOCK))
            return EARTH_SHOCK;
    }

    if (!HasActiveTotem(bot) && CanTryCast(bot, SEARING_TOTEM))
        return SEARING_TOTEM;

    if (HasMainhandWeaponImbue(bot) && CanTryCast(bot, UNLEASH_ELEMENTS))
        return UNLEASH_ELEMENTS;

    if (mw >= 1 && CanTryCast(bot, ELEMENTAL_BLAST))
        return ELEMENTAL_BLAST;

    if (mw >= 5 && CanTryCast(bot, LIGHTNING_BOLT))
        return LIGHTNING_BOLT;

    if (ascendance && CanTryCast(bot, STORMBLAST))
        return STORMBLAST;
    if (!ascendance && CanTryCast(bot, STORMSTRIKE))
        return STORMSTRIKE;

    if (ascendance && CanTryCast(bot, LAVA_LASH))
        return LAVA_LASH;

    if ((HasAuraUp(bot, UNLEASH_FLAME) || !fsUp) && CanTryCast(bot, FLAME_SHOCK))
        return FLAME_SHOCK;

    if (!ascendance && CanTryCast(bot, LAVA_LASH))
        return LAVA_LASH;

    if (mw >= 3 && CanTryCast(bot, LIGHTNING_BOLT))
        return LIGHTNING_BOLT;

    if (!ascendance && CanTryCast(bot, EARTH_SHOCK))
        return EARTH_SHOCK;

    if (CanTryCast(bot, FROST_SHOCK))
        return FROST_SHOCK;

    if (mw >= 1 && CanTryCast(bot, LIGHTNING_BOLT))
        return LIGHTNING_BOLT;

    return 0;
}

} // namespace BotRotation
