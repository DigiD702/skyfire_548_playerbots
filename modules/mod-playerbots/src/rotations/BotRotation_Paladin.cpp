/*
 * Retribution / Protection — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum PalaSpells : uint32
    {
        SEAL_OF_TRUTH           = 31801,
        INQUISITION             = 84963,
        TEMPLARS_VERDICT        = 85256,
        CRUSADER_STRIKE         = 35395,
        JUDGMENT                = 20271,
        EXORCISM                = 879,
        RIGHTEOUS_FURY          = 25780,
        SEAL_OF_INSIGHT         = 20165,
        AVENGERS_SHIELD         = 31935,
        HAMMER_OF_THE_RIGHTEOUS = 53595,
        SHIELD_OF_THE_RIGHTEOUS = 53600,
        CONSECRATION            = 26573,
    };
}

uint32 SelectRetribution(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const hp = bot->GetPower(POWER_HOLY_POWER);

    // Buff: Seal of Truth
    if (!HasAuraUp(bot, SEAL_OF_TRUTH) && CanTryCast(bot, SEAL_OF_TRUTH))
        return SEAL_OF_TRUTH;

    // Inquisition (keep Holy Power buff active)
    if (NeedsMyAuraRefresh(bot, bot, INQUISITION, 3.0f) && hp >= 3 && CanTryCast(bot, INQUISITION))
        return INQUISITION;

    if (CanTryCast(bot, CRUSADER_STRIKE))
        return CRUSADER_STRIKE;

    if (CanTryCast(bot, JUDGMENT))
        return JUDGMENT;

    if (CanTryCast(bot, EXORCISM))
        return EXORCISM;

    if (hp >= 3 && CanTryCast(bot, TEMPLARS_VERDICT))
        return TEMPLARS_VERDICT;

    return 0;
}

uint32 SelectProtectionPaladin(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const hp = bot->GetPower(POWER_HOLY_POWER);

    // Buffs: Righteous Fury, Seal of Insight
    if (!HasAuraUp(bot, RIGHTEOUS_FURY) && CanTryCast(bot, RIGHTEOUS_FURY))
        return RIGHTEOUS_FURY;
    if (!HasAuraUp(bot, SEAL_OF_INSIGHT) && CanTryCast(bot, SEAL_OF_INSIGHT))
        return SEAL_OF_INSIGHT;

    if (ctx.enemies >= 2 && CanTryCast(bot, HAMMER_OF_THE_RIGHTEOUS))
        return HAMMER_OF_THE_RIGHTEOUS;
    if (CanTryCast(bot, CRUSADER_STRIKE))
        return CRUSADER_STRIKE;

    if (CanTryCast(bot, JUDGMENT))
        return JUDGMENT;

    if (CanTryCast(bot, AVENGERS_SHIELD))
        return AVENGERS_SHIELD;

    if (CanTryCast(bot, CONSECRATION))
        return CONSECRATION;

    if (hp >= 3 && CanTryCast(bot, SHIELD_OF_THE_RIGHTEOUS))
        return SHIELD_OF_THE_RIGHTEOUS;

    return 0;
}

} // namespace BotRotation
