/*
 * Damage priorities for healer specs when co +healer dps is enabled.
 * Keep these short dungeon-assist lines (not full healer sims).
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
uint32 SelectHolyPaladinDps(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;

    enum Spells : uint32
    {
        SEAL_OF_TRUTH       = 31801,
        SEAL_OF_RIGHTEOUSNESS = 20154,
        JUDGMENT            = 20271,
        CRUSADER_STRIKE     = 35395,
        EXORCISM            = 879,
        HOLY_SHOCK          = 20473,
        DENOUNCE            = 2812,
        HAMMER_OF_WRATH     = 24275,
        HOLY_PRISM          = 114165,
        AVENGING_WRATH      = 31884,
    };

    if (ctx.enemies >= 3)
    {
        if (!HasAuraUp(bot, SEAL_OF_RIGHTEOUSNESS) && CanTryCast(bot, SEAL_OF_RIGHTEOUSNESS))
            return SEAL_OF_RIGHTEOUSNESS;
    }
    else if (!HasAuraUp(bot, SEAL_OF_TRUTH) && CanTryCast(bot, SEAL_OF_TRUTH))
        return SEAL_OF_TRUTH;

    if (CanTryCast(bot, AVENGING_WRATH))
        return AVENGING_WRATH;
    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, HAMMER_OF_WRATH))
        return HAMMER_OF_WRATH;
    if (CanTryCast(bot, HOLY_PRISM))
        return HOLY_PRISM;
    if (CanTryCast(bot, HOLY_SHOCK))
        return HOLY_SHOCK;
    if (CanTryCast(bot, JUDGMENT))
        return JUDGMENT;
    if (CanTryCast(bot, CRUSADER_STRIKE))
        return CRUSADER_STRIKE;
    if (CanTryCast(bot, EXORCISM))
        return EXORCISM;
    if (CanTryCast(bot, DENOUNCE))
        return DENOUNCE;
    return 0;
}

uint32 SelectDisciplineDps(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;

    enum Spells : uint32
    {
        INNER_FIRE          = 588,
        SHADOWFORM          = 15473, // not typically disc; skip
        SMITE               = 585,
        HOLY_FIRE           = 14914,
        PENANCE             = 47540,
        SHADOW_WORD_PAIN    = 589,
        HOLY_WORD_CHASTISE  = 88625,
        CASCADE             = 121135,
        HALO                = 120517,
        DIVINE_STAR         = 110744,
        POWER_INFUSION      = 10060,
    };

    if (!HasAuraUp(bot, INNER_FIRE) && CanTryCast(bot, INNER_FIRE))
        return INNER_FIRE;
    if (CanTryCast(bot, POWER_INFUSION))
        return POWER_INFUSION;

    if (ctx.enemies >= 3)
    {
        if (CanTryCast(bot, HALO))
            return HALO;
        if (CanTryCast(bot, CASCADE))
            return CASCADE;
        if (CanTryCast(bot, DIVINE_STAR))
            return DIVINE_STAR;
    }

    if ((!HasAuraUp(target, SHADOW_WORD_PAIN) || AuraRemains(target, SHADOW_WORD_PAIN) <= 2.0f)
        && CanTryCast(bot, SHADOW_WORD_PAIN))
        return SHADOW_WORD_PAIN;
    if (CanTryCast(bot, PENANCE))
        return PENANCE;
    if (CanTryCast(bot, HOLY_FIRE))
        return HOLY_FIRE;
    if (CanTryCast(bot, HOLY_WORD_CHASTISE))
        return HOLY_WORD_CHASTISE;
    if (CanTryCast(bot, SMITE))
        return SMITE;
    return 0;
}

uint32 SelectHolyPriestDps(Context const& ctx)
{
    // Holy shares most of the Disc damage toolkit in MoP.
    return SelectDisciplineDps(ctx);
}

uint32 SelectRestorationShamanDps(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;

    enum Spells : uint32
    {
        // Keep Water Shield while DPSing — Lightning Shield replaced it every
        // healer-dps GCD (worse under save-mana when heal GCDs are skipped).
        WATER_SHIELD        = 52127,
        FLAME_SHOCK         = 8050,
        LAVA_BURST          = 51505,
        LIGHTNING_BOLT      = 403,
        CHAIN_LIGHTNING     = 421,
        EARTH_SHOCK         = 8042,
        ELEMENTAL_BLAST     = 117014,
        UNLEASH_ELEMENTS    = 73680,
    };

    if (!HasAuraUp(bot, WATER_SHIELD) && CanTryCast(bot, WATER_SHIELD))
        return WATER_SHIELD;

    if (ctx.enemies >= 3 && CanTryCast(bot, CHAIN_LIGHTNING))
        return CHAIN_LIGHTNING;

    if ((!HasAuraUp(target, FLAME_SHOCK) || AuraRemains(target, FLAME_SHOCK) <= 3.0f)
        && CanTryCast(bot, FLAME_SHOCK))
        return FLAME_SHOCK;
    if (HasAuraUp(target, FLAME_SHOCK) && CanTryCast(bot, LAVA_BURST))
        return LAVA_BURST;
    if (CanTryCast(bot, ELEMENTAL_BLAST))
        return ELEMENTAL_BLAST;
    if (CanTryCast(bot, UNLEASH_ELEMENTS))
        return UNLEASH_ELEMENTS;
    if (CanTryCast(bot, EARTH_SHOCK))
        return EARTH_SHOCK;
    if (CanTryCast(bot, LIGHTNING_BOLT))
        return LIGHTNING_BOLT;
    return 0;
}

uint32 SelectRestorationDruidDps(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;

    enum Spells : uint32
    {
        MARK_OF_THE_WILD    = 1126,
        MOONKIN_FORM        = 24858, // may be unknown on pure resto
        WRATH               = 5176,
        STARFIRE            = 2912,
        MOONFIRE            = 8921,
        SUNFIRE             = 93402,
        STARSURGE           = 78674,
        HURRICANE           = 16914,
    };

    if (!HasAuraUp(bot, MARK_OF_THE_WILD) && CanTryCast(bot, MARK_OF_THE_WILD))
        return MARK_OF_THE_WILD;
    if (CanTryCast(bot, MOONKIN_FORM) && !HasAuraUp(bot, MOONKIN_FORM))
        return MOONKIN_FORM;

    if (ctx.enemies >= 3 && CanTryCast(bot, HURRICANE))
        return HURRICANE;

    if ((!HasAuraUp(target, MOONFIRE) || AuraRemains(target, MOONFIRE) <= 2.0f)
        && CanTryCast(bot, MOONFIRE))
        return MOONFIRE;
    if ((!HasAuraUp(target, SUNFIRE) || AuraRemains(target, SUNFIRE) <= 2.0f)
        && CanTryCast(bot, SUNFIRE))
        return SUNFIRE;
    if (CanTryCast(bot, STARSURGE))
        return STARSURGE;
    if (CanTryCast(bot, STARFIRE))
        return STARFIRE;
    if (CanTryCast(bot, WRATH))
        return WRATH;
    return 0;
}

uint32 SelectMistweaverDps(Context const& ctx)
{
    Player* bot = ctx.bot;

    enum Spells : uint32
    {
        JAB                 = 100780,
        TIGER_PALM          = 100787,
        BLACKOUT_KICK       = 100784,
        RISING_SUN_KICK     = 107428,
        SPINNING_CRANE_KICK = 101546,
        FISTS_OF_FURY       = 113656,
        TOUCH_OF_DEATH      = 115080,
        TIGERSEYE_BREW      = 116740,
    };

    uint32 const chi = bot->GetPower(POWER_CHI);

    if (ctx.targetHealthPct <= 10.0f && CanTryCast(bot, TOUCH_OF_DEATH))
        return TOUCH_OF_DEATH;
    if (chi >= 10 && CanTryCast(bot, TIGERSEYE_BREW))
        return TIGERSEYE_BREW;

    if (ctx.enemies >= 3 && CanTryCast(bot, SPINNING_CRANE_KICK))
        return SPINNING_CRANE_KICK;
    if (chi >= 3 && CanTryCast(bot, FISTS_OF_FURY))
        return FISTS_OF_FURY;
    if (CanTryCast(bot, RISING_SUN_KICK))
        return RISING_SUN_KICK;
    if (chi >= 2 && CanTryCast(bot, BLACKOUT_KICK))
        return BLACKOUT_KICK;
    if (chi >= 1 && CanTryCast(bot, TIGER_PALM))
        return TIGER_PALM;
    if (CanTryCast(bot, JAB))
        return JAB;
    return 0;
}

} // namespace BotRotation
