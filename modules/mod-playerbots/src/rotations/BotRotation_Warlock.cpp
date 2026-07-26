/*
 * Affliction — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum AffSpells : uint32
    {
        DARK_INTENT         = 109773,
        AGONY               = 980,
        CORRUPTION          = 172,    // aura 146739 via AuraIdForSpell
        UNSTABLE_AFFLICTION = 30108,
        HAUNT               = 48181,
        MALEFIC_GRASP       = 103103,
        DRAIN_LIFE          = 689,    // low-level filler if MG unknown
        SHADOW_BOLT         = 686,
    };

    bool NeedsDot(Player* bot, Unit* target, uint32 spellId)
    {
        return NeedsMyAuraRefresh(bot, target, spellId, 3.0f);
    }
}

uint32 SelectAffliction(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    // Buff: Dark Intent
    if (!HasAuraUp(bot, DARK_INTENT) && CanTryCast(bot, DARK_INTENT))
        return DARK_INTENT;

    // Agony > Corruption > Unstable Affliction (keep active / refresh)
    if (NeedsDot(bot, target, AGONY) && CanTryCast(bot, AGONY))
        return AGONY;
    if (NeedsDot(bot, target, CORRUPTION) && CanTryCast(bot, CORRUPTION))
        return CORRUPTION;
    if (NeedsDot(bot, target, UNSTABLE_AFFLICTION) && CanTryCast(bot, UNSTABLE_AFFLICTION))
        return UNSTABLE_AFFLICTION;

    // Haunt (buff DoT damage) — skip if unknown / no shards / on CD
    if (CanTryCast(bot, HAUNT))
        return HAUNT;

    // Filler: Malefic Grasp, else Drain Life / Shadow Bolt at low level
    if (CanTryCast(bot, MALEFIC_GRASP))
        return MALEFIC_GRASP;
    if (CanTryCast(bot, DRAIN_LIFE))
        return DRAIN_LIFE;
    if (CanTryCast(bot, SHADOW_BOLT))
        return SHADOW_BOLT;

    return 0;
}

} // namespace BotRotation
