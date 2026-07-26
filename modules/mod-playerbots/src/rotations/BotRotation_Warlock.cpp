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
        DRAIN_LIFE          = 689,
        SHADOW_BOLT         = 686,
        LIFE_TAP            = 1454,
        DARK_SOUL_MISERY    = 113860,
        SOULBURN            = 74434,
        SOUL_SWAP           = 86121,
        FEL_FLAME           = 77799,
        SUMMON_DOOMGUARD    = 18540,
        SEED_OF_CORRUPTION  = 27243,
    };
}

uint32 SelectAffliction(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

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

    // Multi-target: Seed of Corruption (Soulburn when available).
    if (ctx.enemies >= 3)
    {
        if (shards >= 1 && CanTryCast(bot, SOULBURN) && CanTryCast(bot, SEED_OF_CORRUPTION))
            return SOULBURN;
        if (CanTryCast(bot, SEED_OF_CORRUPTION))
            return SEED_OF_CORRUPTION;
    }

    // Apply / refresh OUR DoTs only (caster-filtered). Use a short pandemic
    // window so we do not thrash Agony <-> Corruption every GCD.
    if (NeedsMyAuraRefresh(bot, target, AGONY, 3.0f) && CanTryCast(bot, AGONY))
        return AGONY;
    if (NeedsMyAuraRefresh(bot, target, CORRUPTION, 3.0f) && CanTryCast(bot, CORRUPTION))
        return CORRUPTION;
    if (NeedsMyAuraRefresh(bot, target, UNSTABLE_AFFLICTION, 3.0f) && CanTryCast(bot, UNSTABLE_AFFLICTION))
        return UNSTABLE_AFFLICTION;

    bool const agony = HasMyAura(bot, target, AGONY);
    bool const corr = HasMyAura(bot, target, CORRUPTION);
    bool const ua = HasMyAura(bot, target, UNSTABLE_AFFLICTION);
    bool const dotsReady = agony && corr && (ua || !bot->HasSpell(UNSTABLE_AFFLICTION));

    if (ctx.targetHealthPct <= 20.0f && dotsReady && shards < 4 && CanTryCast(bot, DRAIN_SOUL))
        return DRAIN_SOUL;

    float const hauntRemains = MyAuraRemains(bot, target, HAUNT);
    if (dotsReady && shards >= 1 && hauntRemains < 3.0f && CanTryCast(bot, HAUNT))
        return HAUNT;
    if (shards >= 4 && dotsReady && CanTryCast(bot, HAUNT))
        return HAUNT;

    // Critical mana only — do not Life Tap every GCD under 80%.
    if (manaPct <= 20.0f && CanTryCast(bot, LIFE_TAP))
        return LIFE_TAP;

    if (dotsReady && manaPct > 20.0f && CanTryCast(bot, MALEFIC_GRASP))
        return MALEFIC_GRASP;

    // Low-level fillers when MG / Haunt are unknown.
    if (dotsReady && CanTryCast(bot, DRAIN_LIFE))
        return DRAIN_LIFE;
    if (CanTryCast(bot, FEL_FLAME))
        return FEL_FLAME;
    if (CanTryCast(bot, SHADOW_BOLT))
        return SHADOW_BOLT;

    if (manaPct < 35.0f && CanTryCast(bot, LIFE_TAP))
        return LIFE_TAP;

    return 0;
}

} // namespace BotRotation
