/*
 * Affliction Warlock - simplified from Hekili WarlockAffliction.simc
 */

#include "BotRotationLists.h"
#include "Pet.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum AffSpells : uint32
    {
        DARK_INTENT         = 109773,
        SUMMON_FELHUNTER    = 691,
        AGONY               = 980,
        CORRUPTION          = 172,
        UNSTABLE_AFFLICTION = 30108,
        HAUNT               = 48181,
        MALEFIC_GRASP       = 103103,
        DRAIN_SOUL          = 1120,
        LIFE_TAP            = 1454,
        DARK_SOUL_MISERY    = 113860,
        SOULBURN            = 74434,
        SOUL_SWAP           = 86121,
        FEL_FLAME           = 77799,
        SUMMON_DOOMGUARD    = 18540,
    };
}

uint32 SelectAffliction(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const shards = bot->GetPower(POWER_SOUL_SHARDS);
    uint32 const manaMax = bot->GetMaxPower(POWER_MANA);
    float const manaPct = manaMax ? (100.0f * float(bot->GetPower(POWER_MANA)) / float(manaMax)) : 100.0f;

    if (!HasAuraUp(bot, DARK_INTENT) && CanTryCast(bot, DARK_INTENT))
        return DARK_INTENT;

    Pet* pet = bot->GetPet();
    if ((!pet || !pet->IsAlive()) && CanTryCast(bot, SUMMON_FELHUNTER))
        return SUMMON_FELHUNTER;

    if (CanTryCast(bot, SUMMON_DOOMGUARD))
        return SUMMON_DOOMGUARD;

    if (!HasAuraUp(bot, DARK_SOUL_MISERY)
        && (ctx.targetHealthPct <= 20.0f || shards >= 3)
        && CanTryCast(bot, DARK_SOUL_MISERY))
        return DARK_SOUL_MISERY;

    bool const agony = HasAuraUp(target, AGONY);
    bool const corr = HasAuraUp(target, CORRUPTION);
    bool const ua = HasAuraUp(target, UNSTABLE_AFFLICTION);

    if (!agony && CanTryCast(bot, AGONY))
        return AGONY;
    if (!corr && CanTryCast(bot, CORRUPTION))
        return CORRUPTION;
    if (!ua && CanTryCast(bot, UNSTABLE_AFFLICTION))
        return UNSTABLE_AFFLICTION;

    if (ctx.targetHealthPct <= 20.0f && agony && corr && ua && shards < 4 && CanTryCast(bot, DRAIN_SOUL))
        return DRAIN_SOUL;

    float const hauntRemains = AuraRemains(target, HAUNT);
    if (agony && corr && ua && shards >= 1 && hauntRemains < 3.0f && CanTryCast(bot, HAUNT))
        return HAUNT;
    if (shards >= 4 && agony && corr && ua && CanTryCast(bot, HAUNT))
        return HAUNT;

    if (AuraRemains(target, AGONY) <= 12.0f && CanTryCast(bot, AGONY))
        return AGONY;
    if (AuraRemains(target, UNSTABLE_AFFLICTION) <= 7.0f && CanTryCast(bot, UNSTABLE_AFFLICTION))
        return UNSTABLE_AFFLICTION;
    if (AuraRemains(target, CORRUPTION) <= 9.0f && CanTryCast(bot, CORRUPTION))
        return CORRUPTION;

    if (manaPct <= 15.0f && CanTryCast(bot, LIFE_TAP))
        return LIFE_TAP;

    if (agony && corr && ua && manaPct > 20.0f && CanTryCast(bot, MALEFIC_GRASP))
        return MALEFIC_GRASP;

    if (manaPct < 80.0f && CanTryCast(bot, LIFE_TAP))
        return LIFE_TAP;

    if (CanTryCast(bot, FEL_FLAME))
        return FEL_FLAME;

    return 0;
}

} // namespace BotRotation
