/*
 * Shadow Priest — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
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
        SHADOWFORM       = 15473,
        INNER_FIRE       = 588,
        VAMPIRIC_TOUCH   = 34914,
        SHADOW_WORD_PAIN = 589,
        MIND_BLAST       = 8092,
        DEVOURING_PLAGUE = 2944,
        MIND_FLAY        = 15407,
    };
}

uint32 SelectShadow(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const orbs = bot->GetPower(POWER_SHADOW_ORBS);

    if (!HasAuraUp(bot, SHADOWFORM) && CanTryCast(bot, SHADOWFORM))
        return SHADOWFORM;
    if (!HasAuraUp(bot, INNER_FIRE) && CanTryCast(bot, INNER_FIRE))
        return INNER_FIRE;

    if (NeedsMyAuraRefresh(bot, target, VAMPIRIC_TOUCH, 3.0f) && CanTryCast(bot, VAMPIRIC_TOUCH))
        return VAMPIRIC_TOUCH;
    if (NeedsMyAuraRefresh(bot, target, SHADOW_WORD_PAIN, 3.0f) && CanTryCast(bot, SHADOW_WORD_PAIN))
        return SHADOW_WORD_PAIN;

    if (CanTryCast(bot, MIND_BLAST))
        return MIND_BLAST;

    if (orbs >= 3 && CanTryCast(bot, DEVOURING_PLAGUE))
        return DEVOURING_PLAGUE;

    if (CanTryCast(bot, MIND_FLAY))
        return MIND_FLAY;

    return 0;
}

} // namespace BotRotation
