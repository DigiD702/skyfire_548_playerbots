/*
 * Feral Druid - simplified from Hekili DruidFeral.simc
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum FeralSpells : uint32
    {
        CAT_FORM            = 768,
        SAVAGE_ROAR         = 52610,
        RIP                 = 1079,
        RAKE                = 1822,
        SHRED               = 5221,
        MANGLE_CAT          = 33876,
        FEROCIOUS_BITE      = 22568,
        TIGERS_FURY         = 5217,
        BERSERK             = 106951,
        THRASH_CAT          = 106830,
        SWIPE_CAT           = 62078,
        FAERIE_FIRE         = 770,
        MARK_OF_THE_WILD    = 1126,
    };
}

uint32 SelectFeral(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    int8 const cp = ctx.comboPoints;
    uint32 const energy = bot->GetPower(POWER_ENERGY);

    if (!HasAuraUp(bot, MARK_OF_THE_WILD) && CanTryCast(bot, MARK_OF_THE_WILD))
        return MARK_OF_THE_WILD;

    if (!HasAuraUp(bot, CAT_FORM) && CanTryCast(bot, CAT_FORM))
        return CAT_FORM;

    if (!HasAuraUp(bot, CAT_FORM))
        return 0;

    if (energy <= 35 && !HasAuraUp(bot, BERSERK) && CanTryCast(bot, TIGERS_FURY))
        return TIGERS_FURY;

    if (HasAuraUp(bot, TIGERS_FURY) && CanTryCast(bot, BERSERK))
        return BERSERK;

    if ((!HasAuraUp(bot, SAVAGE_ROAR) || AuraRemains(bot, SAVAGE_ROAR) <= 3.0f)
        && cp > 0 && CanTryCast(bot, SAVAGE_ROAR))
        return SAVAGE_ROAR;

    if (!HasAuraUp(target, FAERIE_FIRE) && CanTryCast(bot, FAERIE_FIRE))
        return FAERIE_FIRE;

    if (ctx.enemies >= 3)
    {
        if ((!HasAuraUp(target, THRASH_CAT) || AuraRemains(target, THRASH_CAT) <= 3.0f)
            && CanTryCast(bot, THRASH_CAT))
            return THRASH_CAT;
        if (CanTryCast(bot, SWIPE_CAT))
            return SWIPE_CAT;
    }

    if (cp >= 5)
    {
        if ((!HasAuraUp(target, RIP) || AuraRemains(target, RIP) <= 4.0f)
            && HasAuraUp(bot, SAVAGE_ROAR) && CanTryCast(bot, RIP))
            return RIP;
        if (ctx.targetHealthPct <= 25.0f && CanTryCast(bot, FEROCIOUS_BITE))
            return FEROCIOUS_BITE;
        if (HasAuraUp(target, RIP) && AuraRemains(target, RIP) > 6.0f
            && CanTryCast(bot, FEROCIOUS_BITE))
            return FEROCIOUS_BITE;
    }

    if ((!HasAuraUp(target, RAKE) || AuraRemains(target, RAKE) <= 3.0f)
        && HasAuraUp(bot, SAVAGE_ROAR) && CanTryCast(bot, RAKE))
        return RAKE;

    if ((!HasAuraUp(target, THRASH_CAT) || AuraRemains(target, THRASH_CAT) <= 3.0f)
        && CanTryCast(bot, THRASH_CAT))
        return THRASH_CAT;

    if (CanTryCast(bot, SHRED))
        return SHRED;
    if (CanTryCast(bot, MANGLE_CAT))
        return MANGLE_CAT;

    return 0;
}

uint32 SelectBalance(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;

    enum BalanceSpells : uint32
    {
        MARK_OF_THE_WILD    = 1126,
        MOONKIN_FORM        = 24858,
        MOONFIRE            = 8921,
        SUNFIRE             = 93402,
        STARSURGE           = 78674,
        STARFIRE            = 2912,
        WRATH               = 5176,
        STARFALL            = 48505,
        CELESTIAL_ALIGNMENT = 112071,
        HURRICANE           = 16914,
        SHOOTING_STARS      = 93400,
    };

    if (!HasAuraUp(bot, MARK_OF_THE_WILD) && CanTryCast(bot, MARK_OF_THE_WILD))
        return MARK_OF_THE_WILD;

    if (!HasAuraUp(bot, MOONKIN_FORM) && CanTryCast(bot, MOONKIN_FORM))
        return MOONKIN_FORM;

    if (CanTryCast(bot, CELESTIAL_ALIGNMENT))
        return CELESTIAL_ALIGNMENT;

    if (ctx.enemies >= 3 && CanTryCast(bot, STARFALL))
        return STARFALL;
    if (ctx.enemies >= 4 && CanTryCast(bot, HURRICANE))
        return HURRICANE;

    if ((!HasAuraUp(target, MOONFIRE) || AuraRemains(target, MOONFIRE) <= 3.0f)
        && CanTryCast(bot, MOONFIRE))
        return MOONFIRE;
    if ((!HasAuraUp(target, SUNFIRE) || AuraRemains(target, SUNFIRE) <= 3.0f)
        && CanTryCast(bot, SUNFIRE))
        return SUNFIRE;

    if ((HasAuraUp(bot, SHOOTING_STARS) || CanTryCast(bot, STARSURGE))
        && CanTryCast(bot, STARSURGE))
        return STARSURGE;

    // Alternate fillers; without eclipse tracking Starfire is a safe default.
    if (CanTryCast(bot, STARFIRE))
        return STARFIRE;
    if (CanTryCast(bot, WRATH))
        return WRATH;

    return 0;
}

uint32 SelectGuardian(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;

    enum GuardianSpells : uint32
    {
        BEAR_FORM       = 5487,
        MANGLE_BEAR     = 33878,
        THRASH_BEAR     = 77758,
        LACERATE        = 33745,
        MAUL            = 6807,
        FAERIE_FIRE     = 770,
        SAVAGE_DEFENSE  = 62606,
        MARK_OF_THE_WILD = 1126,
    };

    if (!HasAuraUp(bot, MARK_OF_THE_WILD) && CanTryCast(bot, MARK_OF_THE_WILD))
        return MARK_OF_THE_WILD;
    if (!HasAuraUp(bot, BEAR_FORM) && CanTryCast(bot, BEAR_FORM))
        return BEAR_FORM;

    float const hpPct = bot->GetMaxHealth()
        ? (100.0f * float(bot->GetHealth()) / float(bot->GetMaxHealth())) : 100.0f;
    if (hpPct < 70.0f && CanTryCast(bot, SAVAGE_DEFENSE))
        return SAVAGE_DEFENSE;

    if (!HasAuraUp(target, FAERIE_FIRE) && CanTryCast(bot, FAERIE_FIRE))
        return FAERIE_FIRE;

    if (ctx.enemies >= 2 && CanTryCast(bot, THRASH_BEAR))
        return THRASH_BEAR;

    if ((!HasAuraUp(target, LACERATE) || AuraStacks(target, LACERATE) < 3
        || AuraRemains(target, LACERATE) <= 3.0f) && CanTryCast(bot, LACERATE))
        return LACERATE;

    if (CanTryCast(bot, MANGLE_BEAR))
        return MANGLE_BEAR;

    if (bot->GetPower(POWER_RAGE) >= 60 && CanTryCast(bot, MAUL))
        return MAUL;

    if (CanTryCast(bot, THRASH_BEAR))
        return THRASH_BEAR;

    return 0;
}

} // namespace BotRotation
