/*
 * Guardian / Feral / Balance — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum GuardianSpells : uint32
    {
        BEAR_FORM       = 5487,
        MANGLE_BEAR     = 33878,
        THRASH_BEAR     = 77758,
        LACERATE        = 33745,
        SWIPE           = 779,
        SAVAGE_DEFENSE  = 62606,
    };

    enum FeralSpells : uint32
    {
        CAT_FORM        = 768,
        SAVAGE_ROAR     = 52610,
        RAKE            = 1822,
        MANGLE_CAT      = 33876,
        SHRED           = 5221,
        RIP             = 1079,
        FEROCIOUS_BITE  = 22568,
    };

    enum BalanceSpells : uint32
    {
        MOONKIN_FORM    = 24858,
        MOONFIRE        = 8921,
        SUNFIRE         = 93402,
        STARSURGE       = 78674,
        WRATH           = 5176,
        STARFIRE        = 2912,
        ECLIPSE_SOLAR   = 48517,
        ECLIPSE_LUNAR   = 48518,
    };
}

uint32 SelectGuardian(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    if (!HasAuraUp(bot, BEAR_FORM) && CanTryCast(bot, BEAR_FORM))
        return BEAR_FORM;

    if (CanTryCast(bot, MANGLE_BEAR))
        return MANGLE_BEAR;

    if (NeedsMyAuraRefresh(bot, target, THRASH_BEAR, 3.0f) && CanTryCast(bot, THRASH_BEAR))
        return THRASH_BEAR;

    uint32 const lacStacks = AuraStacks(target, LACERATE);
    if ((lacStacks < 3 || NeedsMyAuraRefresh(bot, target, LACERATE, 3.0f))
        && CanTryCast(bot, LACERATE))
        return LACERATE;

    if (CanTryCast(bot, SWIPE))
        return SWIPE;

    if (CanTryCast(bot, SAVAGE_DEFENSE))
        return SAVAGE_DEFENSE;

    return 0;
}

uint32 SelectFeral(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    int8 const cp = ctx.comboPoints;

    if (!HasAuraUp(bot, CAT_FORM) && CanTryCast(bot, CAT_FORM))
        return CAT_FORM;

    if (cp > 0 && NeedsMyAuraRefresh(bot, bot, SAVAGE_ROAR, 3.0f) && CanTryCast(bot, SAVAGE_ROAR))
        return SAVAGE_ROAR;

    if (NeedsMyAuraRefresh(bot, target, RAKE, 3.0f) && CanTryCast(bot, RAKE))
        return RAKE;

    if (cp >= 5)
    {
        if (NeedsMyAuraRefresh(bot, target, RIP, 3.0f) && CanTryCast(bot, RIP))
            return RIP;
        if (HasMyAura(bot, target, RIP) && CanTryCast(bot, FEROCIOUS_BITE))
            return FEROCIOUS_BITE;
    }

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
    if (!bot || !target)
        return 0;

    if (!HasAuraUp(bot, MOONKIN_FORM) && CanTryCast(bot, MOONKIN_FORM))
        return MOONKIN_FORM;

    if (NeedsMyAuraRefresh(bot, target, MOONFIRE, 3.0f) && CanTryCast(bot, MOONFIRE))
        return MOONFIRE;
    if (NeedsMyAuraRefresh(bot, target, SUNFIRE, 3.0f) && CanTryCast(bot, SUNFIRE))
        return SUNFIRE;

    if (CanTryCast(bot, STARSURGE))
        return STARSURGE;

    bool const solar = HasAuraUp(bot, ECLIPSE_SOLAR);
    bool const lunar = HasAuraUp(bot, ECLIPSE_LUNAR);
    if (solar && CanTryCast(bot, WRATH))
        return WRATH;
    if (lunar && CanTryCast(bot, STARFIRE))
        return STARFIRE;

    if (CanTryCast(bot, WRATH))
        return WRATH;
    if (CanTryCast(bot, STARFIRE))
        return STARFIRE;

    return 0;
}

} // namespace BotRotation
