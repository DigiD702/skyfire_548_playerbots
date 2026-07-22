/*
 * Shadow Priest - simplified from Hekili PriestShadow.simc
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum ShadowSpells : uint32
    {
        SHADOWFORM          = 15473,
        INNER_FIRE          = 588,
        POWER_WORD_FORTITUDE = 21562,
        SHADOW_WORD_PAIN    = 589,
        VAMPIRIC_TOUCH      = 34914,
        DEVOURING_PLAGUE    = 2944,
        MIND_BLAST          = 8092,
        MIND_FLAY           = 15407,
        MIND_SPIKE          = 73510,
        SHADOW_WORD_DEATH   = 32379,
        MIND_SEAR           = 48045,
        SHADOWFIEND         = 34433,
        SURGE_OF_DARKNESS   = 87160,
        HALO                = 120517,
        CASCADE             = 121135,
        DIVINE_STAR         = 110744,
        POWER_INFUSION      = 10060,
        VAMPIRIC_EMBRACE    = 15286,
    };
}

uint32 SelectShadow(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const orbs = bot->GetPower(POWER_SHADOW_ORBS);

    if (!HasAuraUp(bot, SHADOWFORM) && CanTryCast(bot, SHADOWFORM))
        return SHADOWFORM;
    if (!HasAuraUp(bot, INNER_FIRE) && CanTryCast(bot, INNER_FIRE))
        return INNER_FIRE;
    if (!HasAuraUp(bot, POWER_WORD_FORTITUDE) && CanTryCast(bot, POWER_WORD_FORTITUDE))
        return POWER_WORD_FORTITUDE;

    if (bot->GetHealthPct() <= 40.0f && CanTryCast(bot, VAMPIRIC_EMBRACE))
        return VAMPIRIC_EMBRACE;

    if (CanTryCast(bot, SHADOWFIEND))
        return SHADOWFIEND;
    if (CanTryCast(bot, POWER_INFUSION))
        return POWER_INFUSION;

    float const swp = AuraRemains(target, SHADOW_WORD_PAIN);
    float const vt = AuraRemains(target, VAMPIRIC_TOUCH);

    if (orbs >= 3 && swp > 3.0f && vt > 3.0f && CanTryCast(bot, DEVOURING_PLAGUE))
        return DEVOURING_PLAGUE;

    if (ctx.enemies <= 5 && CanTryCast(bot, MIND_BLAST))
        return MIND_BLAST;

    if (ctx.enemies <= 5 && CanTryCast(bot, SHADOW_WORD_DEATH))
        return SHADOW_WORD_DEATH;

    if (swp <= 1.0f && CanTryCast(bot, SHADOW_WORD_PAIN))
        return SHADOW_WORD_PAIN;
    if (vt <= 2.5f && CanTryCast(bot, VAMPIRIC_TOUCH))
        return VAMPIRIC_TOUCH;

    if (orbs >= 3 && CanTryCast(bot, DEVOURING_PLAGUE))
        return DEVOURING_PLAGUE;

    uint32 const surge = AuraStacks(bot, SURGE_OF_DARKNESS);
    if (surge >= 1 && ctx.enemies <= 5 && CanTryCast(bot, MIND_SPIKE))
        return MIND_SPIKE;

    if (CanTryCast(bot, HALO))
        return HALO;
    if (ctx.enemies > 1 && CanTryCast(bot, CASCADE))
        return CASCADE;
    if (ctx.enemies > 1 && CanTryCast(bot, DIVINE_STAR))
        return DIVINE_STAR;

    if (ctx.enemies >= 3 && CanTryCast(bot, MIND_SEAR))
        return MIND_SEAR;

    if (CanTryCast(bot, MIND_FLAY))
        return MIND_FLAY;

    return 0;
}

} // namespace BotRotation
