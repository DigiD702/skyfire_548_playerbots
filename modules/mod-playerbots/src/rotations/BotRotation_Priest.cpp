/*
 * Shadow Priest - simplified from Hekili PriestShadow.simc
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "SpellAuras.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum ShadowSpells : uint32
    {
        SHADOWFORM          = 15473,
        INNER_FIRE          = 588,
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
        SMITE               = 585,
        HOLY_FIRE           = 14914,
    };

    bool NeedsRefresh(Player* bot, Unit* target, uint32 spellId, float refreshAt)
    {
        if (!bot || !target)
            return true;
        // Prefer our own DoT — ignore other casters' auras for refresh logic.
        Aura* aura = target->GetAura(spellId, bot->GetGUID());
        if (!aura)
            aura = target->GetAuraOfRankedSpell(spellId, bot->GetGUID());
        if (!aura)
            return true;
        if (aura->IsPermanent() || aura->GetDuration() < 0)
            return false;
        return (aura->GetDuration() / 1000.0f) <= refreshAt;
    }
}

uint32 SelectShadow(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const orbs = bot->GetPower(POWER_SHADOW_ORBS);

    // Form / armor first — required for shadow damage, then fight.
    if (!HasAuraUp(bot, SHADOWFORM) && CanTryCast(bot, SHADOWFORM))
        return SHADOWFORM;
    if (!HasAuraUp(bot, INNER_FIRE) && CanTryCast(bot, INNER_FIRE))
        return INNER_FIRE;

    if (bot->GetHealthPct() <= 40.0f && CanTryCast(bot, VAMPIRIC_EMBRACE))
        return VAMPIRIC_EMBRACE;

    // Keep SW:P up, but never re-cast every GCD when our DoT is already present.
    if (NeedsRefresh(bot, target, SHADOW_WORD_PAIN, 2.0f) && CanTryCast(bot, SHADOW_WORD_PAIN))
        return SHADOW_WORD_PAIN;
    if (NeedsRefresh(bot, target, VAMPIRIC_TOUCH, 2.5f) && CanTryCast(bot, VAMPIRIC_TOUCH))
        return VAMPIRIC_TOUCH;

    if (orbs >= 3 && CanTryCast(bot, DEVOURING_PLAGUE))
        return DEVOURING_PLAGUE;

    // Low-level / missing-spec fillers before long CDs so bots actually deal damage.
    if (CanTryCast(bot, MIND_BLAST))
        return MIND_BLAST;

    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, SHADOW_WORD_DEATH))
        return SHADOW_WORD_DEATH;

    uint32 const surge = AuraStacks(bot, SURGE_OF_DARKNESS);
    if (surge >= 1 && CanTryCast(bot, MIND_SPIKE))
        return MIND_SPIKE;

    if (CanTryCast(bot, MIND_FLAY))
        return MIND_FLAY;

    if (CanTryCast(bot, HOLY_FIRE))
        return HOLY_FIRE;
    if (CanTryCast(bot, SMITE))
        return SMITE;

    if (CanTryCast(bot, SHADOWFIEND))
        return SHADOWFIEND;
    if (CanTryCast(bot, POWER_INFUSION))
        return POWER_INFUSION;

    if (CanTryCast(bot, HALO))
        return HALO;
    if (ctx.enemies > 1 && CanTryCast(bot, CASCADE))
        return CASCADE;
    if (ctx.enemies > 1 && CanTryCast(bot, DIVINE_STAR))
        return DIVINE_STAR;

    if (ctx.enemies >= 3 && CanTryCast(bot, MIND_SEAR))
        return MIND_SEAR;

    return 0;
}

} // namespace BotRotation
