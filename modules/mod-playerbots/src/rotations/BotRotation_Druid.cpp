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

} // namespace BotRotation
