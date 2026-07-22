/*
 * Wave 4 healer priorities - simplified MoP / Hekili dungeon lines.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{

uint32 SelectHolyPaladin(HealContext const& ctx)
{
    Player* bot = ctx.bot;
    Player* ally = ctx.healTarget;
    uint32 const hp = bot->GetPower(POWER_HOLY_POWER);
    bool const urgent = ctx.healTargetHealthPct < 40.0f;

    enum HolyPalaSpells : uint32
    {
        SEAL_OF_INSIGHT     = 20165,
        BLESSING_OF_KINGS   = 20217,
        BEACON_OF_LIGHT     = 53563,
        DIVINE_PLEA         = 54428,
        DIVINE_FAVOR        = 31842,
        AVENGING_WRATH      = 31884,
        HOLY_SHOCK          = 20473,
        FLASH_OF_LIGHT      = 19750,
        HOLY_LIGHT          = 635,
        ETERNAL_FLAME       = 114163,
        WORD_OF_GLORY       = 85673,
        LIGHT_OF_DAWN       = 85222,
        HOLY_RADIANCE       = 82327,
        DIVINE_PURPOSE      = 90174,
    };

    if (!HasAuraUp(bot, SEAL_OF_INSIGHT) && CanTryCast(bot, SEAL_OF_INSIGHT))
        return SEAL_OF_INSIGHT;
    if (!HasAuraUp(bot, BLESSING_OF_KINGS) && CanTryCast(bot, BLESSING_OF_KINGS))
        return BLESSING_OF_KINGS;
    if (!HasAuraUp(ally, BEACON_OF_LIGHT) && CanTryCast(bot, BEACON_OF_LIGHT))
        return BEACON_OF_LIGHT;

    if (ctx.manaPct < 60.0f && CanTryCast(bot, DIVINE_PLEA))
        return DIVINE_PLEA;
    if (CanTryCast(bot, DIVINE_FAVOR))
        return DIVINE_FAVOR;
    if (CanTryCast(bot, AVENGING_WRATH))
        return AVENGING_WRATH;

    if ((hp >= 3 || HasAuraUp(bot, DIVINE_PURPOSE)))
    {
        if (ctx.injuredAllies >= 3 && CanTryCast(bot, LIGHT_OF_DAWN))
            return LIGHT_OF_DAWN;
        if (CanTryCast(bot, ETERNAL_FLAME))
            return ETERNAL_FLAME;
        if (CanTryCast(bot, WORD_OF_GLORY))
            return WORD_OF_GLORY;
    }

    if (ctx.injuredAllies >= 3 && CanTryCast(bot, HOLY_RADIANCE))
        return HOLY_RADIANCE;

    if (urgent && CanTryCast(bot, FLASH_OF_LIGHT))
        return FLASH_OF_LIGHT;
    if (CanTryCast(bot, HOLY_SHOCK))
        return HOLY_SHOCK;
    if (CanTryCast(bot, HOLY_LIGHT))
        return HOLY_LIGHT;

    return FLASH_OF_LIGHT;
}

uint32 SelectDiscipline(HealContext const& ctx)
{
    Player* bot = ctx.bot;
    Player* ally = ctx.healTarget;
    bool const urgent = ctx.healTargetHealthPct < 40.0f;

    enum DiscSpells : uint32
    {
        INNER_FIRE          = 588,
        POWER_WORD_FORT     = 21562,
        POWER_WORD_SHIELD   = 17,
        PENANCE             = 47540,
        FLASH_HEAL          = 2061,
        GREATER_HEAL        = 2060,
        PRAYER_OF_MENDING   = 33076,
        RENEW               = 139,
        SPIRIT_SHELL        = 109964,
        PAIN_SUPPRESSION    = 33206,
        POWER_INFUSION      = 10060,
    };

    if (!HasAuraUp(bot, INNER_FIRE) && CanTryCast(bot, INNER_FIRE))
        return INNER_FIRE;
    if (!HasAuraUp(bot, POWER_WORD_FORT) && CanTryCast(bot, POWER_WORD_FORT))
        return POWER_WORD_FORT;

    if (ctx.healTargetHealthPct < 30.0f && CanTryCast(bot, PAIN_SUPPRESSION))
        return PAIN_SUPPRESSION;
    if (CanTryCast(bot, POWER_INFUSION))
        return POWER_INFUSION;
    if (CanTryCast(bot, SPIRIT_SHELL))
        return SPIRIT_SHELL;

    if (urgent && !HasAuraUp(ally, POWER_WORD_SHIELD) && CanTryCast(bot, POWER_WORD_SHIELD))
        return POWER_WORD_SHIELD;
    if (!HasAuraUp(ally, PRAYER_OF_MENDING) && CanTryCast(bot, PRAYER_OF_MENDING))
        return PRAYER_OF_MENDING;

    if (CanTryCast(bot, PENANCE))
        return PENANCE;

    if (urgent && CanTryCast(bot, FLASH_HEAL))
        return FLASH_HEAL;
    if (!urgent && CanTryCast(bot, GREATER_HEAL))
        return GREATER_HEAL;
    if (!HasAuraUp(ally, RENEW) && CanTryCast(bot, RENEW))
        return RENEW;

    return FLASH_HEAL;
}

uint32 SelectHolyPriest(HealContext const& ctx)
{
    Player* bot = ctx.bot;
    Player* ally = ctx.healTarget;
    bool const urgent = ctx.healTargetHealthPct < 40.0f;
    bool const critical = ctx.healTargetHealthPct < 25.0f;

    enum HolyPriestSpells : uint32
    {
        INNER_FIRE          = 588,
        POWER_WORD_FORT     = 21562,
        RENEW               = 139,
        PRAYER_OF_MENDING   = 33076,
        FLASH_HEAL          = 2061,
        HEAL                = 2060,
        CIRCLE_OF_HEALING   = 34861,
        PRAYER_OF_HEALING   = 596,
        GUARDIAN_SPIRIT     = 47788,
        DIVINE_HYMN         = 64843,
        HOLY_WORD_SERENITY  = 88684,
        BINDING_HEAL        = 32546,
    };

    if (!HasAuraUp(bot, INNER_FIRE) && CanTryCast(bot, INNER_FIRE))
        return INNER_FIRE;
    if (!HasAuraUp(bot, POWER_WORD_FORT) && CanTryCast(bot, POWER_WORD_FORT))
        return POWER_WORD_FORT;

    if (critical && CanTryCast(bot, GUARDIAN_SPIRIT))
        return GUARDIAN_SPIRIT;
    if (ctx.injuredAllies >= 4 && CanTryCast(bot, DIVINE_HYMN))
        return DIVINE_HYMN;

    if (!HasAuraUp(ally, RENEW) && CanTryCast(bot, RENEW))
        return RENEW;
    if (!HasAuraUp(ally, PRAYER_OF_MENDING) && CanTryCast(bot, PRAYER_OF_MENDING))
        return PRAYER_OF_MENDING;

    if (ctx.injuredAllies >= 3)
    {
        if (CanTryCast(bot, CIRCLE_OF_HEALING))
            return CIRCLE_OF_HEALING;
        if (CanTryCast(bot, PRAYER_OF_HEALING))
            return PRAYER_OF_HEALING;
    }

    if (CanTryCast(bot, HOLY_WORD_SERENITY))
        return HOLY_WORD_SERENITY;
    if (urgent && CanTryCast(bot, FLASH_HEAL))
        return FLASH_HEAL;
    if (urgent && CanTryCast(bot, BINDING_HEAL))
        return BINDING_HEAL;
    if (CanTryCast(bot, HEAL))
        return HEAL;

    return FLASH_HEAL;
}

uint32 SelectRestorationShaman(HealContext const& ctx)
{
    Player* bot = ctx.bot;
    Player* ally = ctx.healTarget;
    bool const urgent = ctx.healTargetHealthPct < 40.0f;

    enum RestoShamSpells : uint32
    {
        WATER_SHIELD            = 52127,
        EARTH_SHIELD            = 974,
        RIPTIDE                 = 61295,
        HEALING_SURGE           = 8004,
        HEALING_WAVE            = 331,
        GREATER_HEALING_WAVE    = 77472,
        CHAIN_HEAL              = 1064,
        HEALING_STREAM_TOTEM    = 5394,
        HEALING_RAIN            = 73920,
        ASCENDANCE              = 114049,
    };

    if (!HasAuraUp(bot, WATER_SHIELD) && CanTryCast(bot, WATER_SHIELD))
        return WATER_SHIELD;
    if (!HasAuraUp(ally, EARTH_SHIELD) && CanTryCast(bot, EARTH_SHIELD))
        return EARTH_SHIELD;

    if (CanTryCast(bot, ASCENDANCE))
        return ASCENDANCE;
    if (CanTryCast(bot, HEALING_STREAM_TOTEM))
        return HEALING_STREAM_TOTEM;

    if ((!HasAuraUp(ally, RIPTIDE) || AuraRemains(ally, RIPTIDE) <= 3.0f)
        && CanTryCast(bot, RIPTIDE))
        return RIPTIDE;

    if (ctx.injuredAllies >= 3)
    {
        if (CanTryCast(bot, HEALING_RAIN))
            return HEALING_RAIN;
        if (CanTryCast(bot, CHAIN_HEAL))
            return CHAIN_HEAL;
    }

    if (urgent && CanTryCast(bot, HEALING_SURGE))
        return HEALING_SURGE;
    if (CanTryCast(bot, GREATER_HEALING_WAVE))
        return GREATER_HEALING_WAVE;
    if (CanTryCast(bot, HEALING_WAVE))
        return HEALING_WAVE;

    return HEALING_SURGE;
}

uint32 SelectRestorationDruid(HealContext const& ctx)
{
    Player* bot = ctx.bot;
    Player* ally = ctx.healTarget;
    bool const urgent = ctx.healTargetHealthPct < 40.0f;

    enum RestoDruidSpells : uint32
    {
        MARK_OF_THE_WILD    = 1126,
        REJUVENATION        = 774,
        LIFEBLOOM           = 33763,
        REGROWTH            = 8936,
        HEALING_TOUCH       = 5185,
        WILD_GROWTH         = 48438,
        SWIFTMEND           = 18562,
        NATURES_SWIFTNESS   = 132158,
        TREE_OF_LIFE        = 33891,
    };

    if (!HasAuraUp(bot, MARK_OF_THE_WILD) && CanTryCast(bot, MARK_OF_THE_WILD))
        return MARK_OF_THE_WILD;
    if (CanTryCast(bot, TREE_OF_LIFE))
        return TREE_OF_LIFE;

    if ((!HasAuraUp(ally, LIFEBLOOM) || AuraStacks(ally, LIFEBLOOM) < 3
        || AuraRemains(ally, LIFEBLOOM) <= 3.0f) && CanTryCast(bot, LIFEBLOOM))
        return LIFEBLOOM;
    if (!HasAuraUp(ally, REJUVENATION) && CanTryCast(bot, REJUVENATION))
        return REJUVENATION;

    if ((HasAuraUp(ally, REJUVENATION) || HasAuraUp(ally, REGROWTH))
        && CanTryCast(bot, SWIFTMEND))
        return SWIFTMEND;

    if (ctx.injuredAllies >= 3 && CanTryCast(bot, WILD_GROWTH))
        return WILD_GROWTH;

    if (urgent && CanTryCast(bot, NATURES_SWIFTNESS))
        return NATURES_SWIFTNESS;
    if (urgent && CanTryCast(bot, REGROWTH))
        return REGROWTH;
    if (CanTryCast(bot, HEALING_TOUCH))
        return HEALING_TOUCH;

    return REJUVENATION;
}

uint32 SelectMistweaver(HealContext const& ctx)
{
    Player* bot = ctx.bot;
    Player* ally = ctx.healTarget;
    uint32 const chi = bot->GetPower(POWER_CHI);
    bool const urgent = ctx.healTargetHealthPct < 40.0f;

    enum MistweaverSpells : uint32
    {
        LEGACY_EMPEROR      = 115921,
        SURGING_MIST        = 116694,
        SOOTHING_MIST       = 115175,
        ENVELOPING_MIST     = 124682,
        RENEWING_MIST       = 115151,
        UPLIFT              = 116670,
        REVIVAL             = 115310,
        LIFE_COCOON         = 116849,
        THUNDER_FOCUS_TEA   = 116680,
    };

    if (!HasAuraUp(bot, LEGACY_EMPEROR) && CanTryCast(bot, LEGACY_EMPEROR))
        return LEGACY_EMPEROR;

    if (ctx.healTargetHealthPct < 30.0f && CanTryCast(bot, LIFE_COCOON))
        return LIFE_COCOON;
    if (ctx.injuredAllies >= 4 && CanTryCast(bot, REVIVAL))
        return REVIVAL;
    if (CanTryCast(bot, THUNDER_FOCUS_TEA))
        return THUNDER_FOCUS_TEA;

    if (!HasAuraUp(ally, RENEWING_MIST) && CanTryCast(bot, RENEWING_MIST))
        return RENEWING_MIST;

    if (chi >= 2 && ctx.injuredAllies >= 2 && CanTryCast(bot, UPLIFT))
        return UPLIFT;

    if (urgent && CanTryCast(bot, SURGING_MIST))
        return SURGING_MIST;
    if (chi >= 3 && !HasAuraUp(ally, ENVELOPING_MIST) && CanTryCast(bot, ENVELOPING_MIST))
        return ENVELOPING_MIST;
    if (CanTryCast(bot, SOOTHING_MIST))
        return SOOTHING_MIST;

    return SURGING_MIST;
}

} // namespace BotRotation
