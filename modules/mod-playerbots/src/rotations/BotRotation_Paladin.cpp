/*
 * Retribution Paladin - simplified from Hekili PaladinRetribution.simc
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum RetSpells : uint32
    {
        SEAL_OF_TRUTH           = 31801,
        SEAL_OF_RIGHTEOUSNESS   = 20154,
        INQUISITION             = 84963,
        AVENGING_WRATH          = 31884,
        GUARDIAN_ANCIENT_KINGS  = 86698,
        HOLY_AVENGER            = 105809,
        EXECUTION_SENTENCE      = 114157,
        LIGHTS_HAMMER           = 114158,
        HOLY_PRISM              = 114852,
        DIVINE_STORM            = 53385,
        TEMPLARS_VERDICT        = 85256,
        HAMMER_OF_WRATH         = 24275,
        CRUSADER_STRIKE         = 35395,
        HAMMER_OF_THE_RIGHTEOUS = 53595,
        JUDGMENT                = 20271,
        EXORCISM                = 879,
        DIVINE_PURPOSE          = 90174,
    };
}

uint32 SelectRetribution(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const hp = bot->GetPower(POWER_HOLY_POWER);
    bool const divinePurpose = HasAuraUp(bot, DIVINE_PURPOSE);
    bool const inquisitionUp = HasAuraUp(bot, INQUISITION);
    float const inqRemains = AuraRemains(bot, INQUISITION);
    bool const awUp = HasAuraUp(bot, AVENGING_WRATH);

    // Maintain Seal of Truth (Righteousness on heavy AoE).
    if (ctx.enemies >= 4)
    {
        if (!HasAuraUp(bot, SEAL_OF_RIGHTEOUSNESS) && CanTryCast(bot, SEAL_OF_RIGHTEOUSNESS))
            return SEAL_OF_RIGHTEOUSNESS;
    }
    else if (!HasAuraUp(bot, SEAL_OF_TRUTH) && CanTryCast(bot, SEAL_OF_TRUTH))
        return SEAL_OF_TRUTH;

    // Inquisition
    if ((!inquisitionUp || inqRemains <= 2.0f) && (hp >= 3 || divinePurpose) && CanTryCast(bot, INQUISITION))
        return INQUISITION;

    if (inquisitionUp)
    {
        if (CanTryCast(bot, AVENGING_WRATH))
            return AVENGING_WRATH;
        if (CanTryCast(bot, GUARDIAN_ANCIENT_KINGS))
            return GUARDIAN_ANCIENT_KINGS;
        if (CanTryCast(bot, HOLY_AVENGER) && hp <= 2)
            return HOLY_AVENGER;
        if (CanTryCast(bot, EXECUTION_SENTENCE))
            return EXECUTION_SENTENCE;
        if (CanTryCast(bot, LIGHTS_HAMMER))
            return LIGHTS_HAMMER;
    }

    // Spenders
    if (ctx.enemies >= 2 && (hp >= 5 || divinePurpose || (HasAuraUp(bot, HOLY_AVENGER) && hp >= 3))
        && CanTryCast(bot, DIVINE_STORM))
        return DIVINE_STORM;

    if ((hp >= 5 || (HasAuraUp(bot, HOLY_AVENGER) && hp >= 3) || divinePurpose)
        && CanTryCast(bot, TEMPLARS_VERDICT))
        return TEMPLARS_VERDICT;

    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, HAMMER_OF_WRATH))
        return HAMMER_OF_WRATH;
    // HoW also usable during Avenging Wrath regardless of HP in MoP.
    if (awUp && CanTryCast(bot, HAMMER_OF_WRATH))
        return HAMMER_OF_WRATH;

    if (ctx.enemies >= 4 && CanTryCast(bot, HAMMER_OF_THE_RIGHTEOUS))
        return HAMMER_OF_THE_RIGHTEOUS;

    if (CanTryCast(bot, CRUSADER_STRIKE))
        return CRUSADER_STRIKE;

    if (CanTryCast(bot, JUDGMENT))
        return JUDGMENT;

    if (CanTryCast(bot, EXORCISM))
        return EXORCISM;

    if (ctx.enemies >= 2 && inqRemains > 4.0f && hp >= 3 && CanTryCast(bot, DIVINE_STORM))
        return DIVINE_STORM;

    if (inqRemains > 4.0f && hp >= 3 && CanTryCast(bot, TEMPLARS_VERDICT))
        return TEMPLARS_VERDICT;

    if (CanTryCast(bot, HOLY_PRISM))
        return HOLY_PRISM;

    return 0;
}

} // namespace BotRotation
