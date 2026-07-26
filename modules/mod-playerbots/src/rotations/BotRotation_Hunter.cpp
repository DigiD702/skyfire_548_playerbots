/*
 * Hunter rotations — priority lists; unknown / not-ready spells are skipped.
 * Auto Shot is started once by PlayerbotAI; the core keeps it firing.
 */

#include "BotRotationLists.h"
#include "Pet.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    bool NeedsDot(Unit* target, uint32 castSpellId, uint32 auraId)
    {
        if (!target || HasAuraUp(target, auraId))
            return false;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(castSpellId);
        if (!info)
            return false;
        if (target->IsImmunedToSpell(info) || target->IsImmunedToDamage(info))
            return false;
        return true;
    }

    bool PetAlive(Player* bot)
    {
        Pet* pet = bot->GetPet();
        return pet && pet->IsAlive();
    }

    bool PetInKillCommandRange(Player* bot, Unit* target)
    {
        Pet* pet = bot ? bot->GetPet() : nullptr;
        return pet && pet->IsAlive() && target && pet->IsWithinDist(target, 25.0f);
    }

    // Core hunter shot priority (AC BM/MM default order, MoP focus):
    //   Serpent → Arcane (focus >= 30) → Cobra/Steady
    uint32 SelectHunterShots(Context const& ctx)
    {
        Player* bot = ctx.bot;
        Unit* target = ctx.target;
        uint32 const focus = bot->GetPower(POWER_FOCUS);

        if (NeedsDot(target, 1978, 118253) && CanTryCast(bot, 1978))
            return 1978;

        if (focus >= 30 && CanTryCast(bot, 3044))
            return 3044;

        if (CanTryCast(bot, 77767)) // Cobra Shot
            return 77767;
        if (CanTryCast(bot, 56641)) // Steady Shot
            return 56641;

        return 0;
    }
}

uint32 SelectBeastMastery(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const focus = bot->GetPower(POWER_FOCUS);
    bool const bw = HasAuraUp(bot, 19574);
    bool const petOk = PetAlive(bot);

    // Low level / no spec: shots only (CanTryCast already skips unknowns).
    if (bot->getLevel() < 10)
        return SelectHunterShots(ctx);

    if (!HasAuraUp(bot, 109260) && !HasAuraUp(bot, 13165))
    {
        if (CanTryCast(bot, 109260))
            return 109260;
        if (CanTryCast(bot, 13165))
            return 13165;
    }

    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, 53351)) // Kill Shot
        return 53351;

    if (petOk)
        if (Pet* pet = bot->GetPet())
            if (pet->GetHealthPct() <= 50.0f && CanTryCast(bot, 136))
                return 136; // Mend Pet

    if (petOk && focus >= 50 && CanTryCast(bot, 19574)) // Bestial Wrath
        return 19574;
    if (CanTryCast(bot, 3045)) // Rapid Fire
        return 3045;
    if (bw && CanTryCast(bot, 121818)) // Stampede
        return 121818;
    if (CanTryCast(bot, 131894)) // Murder of Crows
        return 131894;
    if (CanTryCast(bot, 120679)) // Dire Beast
        return 120679;
    if (petOk && AuraStacks(bot->GetPet(), 19615) >= 5 && !bw && CanTryCast(bot, 82692))
        return 82692; // Focus Fire
    if (ctx.enemies >= 3 && focus >= 40 && CanTryCast(bot, 2643))
        return 2643; // Multi-Shot
    if (petOk && PetInKillCommandRange(bot, target) && CanTryCast(bot, 34026))
        return 34026; // Kill Command
    if (focus <= 40 && CanTryCast(bot, 82726))
        return 82726; // Fervor
    if (focus >= 15 && CanTryCast(bot, 117050))
        return 117050; // Glaive Toss
    if (focus >= 40 && CanTryCast(bot, 120360))
        return 120360; // Barrage

    return SelectHunterShots(ctx);
}

uint32 SelectMarksmanship(Context const& ctx)
{
    Player* bot = ctx.bot;
    uint32 const focus = bot->GetPower(POWER_FOCUS);

    if (bot->getLevel() < 10)
        return SelectHunterShots(ctx);

    if (!HasAuraUp(bot, 109260) && !HasAuraUp(bot, 13165))
    {
        if (CanTryCast(bot, 109260))
            return 109260;
        if (CanTryCast(bot, 13165))
            return 13165;
    }

    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, 53351))
        return 53351;
    if (CanTryCast(bot, 3045))
        return 3045;
    if (CanTryCast(bot, 121818))
        return 121818;
    if (CanTryCast(bot, 120679))
        return 120679;
    if (CanTryCast(bot, 131894))
        return 131894;
    if (ctx.enemies >= 3 && focus >= 40 && CanTryCast(bot, 2643))
        return 2643;
    if (focus >= 40 && CanTryCast(bot, 120360))
        return 120360;
    if (focus >= 15 && CanTryCast(bot, 117050))
        return 117050;
    if (CanTryCast(bot, 53209)) // Chimera
        return 53209;
    if (focus >= 50 && CanTryCast(bot, 19434)) // Aimed
        return 19434;

    return SelectHunterShots(ctx);
}

uint32 SelectSurvival(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const focus = bot->GetPower(POWER_FOCUS);

    if (bot->getLevel() < 10)
        return SelectHunterShots(ctx);

    if (!HasAuraUp(bot, 109260) && !HasAuraUp(bot, 13165))
    {
        if (CanTryCast(bot, 109260))
            return 109260;
        if (CanTryCast(bot, 13165))
            return 13165;
    }

    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, 53351))
        return 53351;
    if (CanTryCast(bot, 3045))
        return 3045;
    if (CanTryCast(bot, 131894))
        return 131894;
    if (CanTryCast(bot, 120679))
        return 120679;
    if (NeedsDot(target, 3674, 3674) && CanTryCast(bot, 3674))
        return 3674; // Black Arrow
    if (HasAuraUp(bot, 56453) && CanTryCast(bot, 53301))
        return 53301; // Explosive (LnL)
    if (CanTryCast(bot, 53301))
        return 53301;
    if (ctx.enemies >= 2 && focus >= 40 && CanTryCast(bot, 2643))
        return 2643;
    if (ctx.enemies >= 3 && CanTryCast(bot, 13813))
        return 13813;
    if (focus >= 40 && CanTryCast(bot, 120360))
        return 120360;
    if (focus >= 15 && CanTryCast(bot, 117050))
        return 117050;

    return SelectHunterShots(ctx);
}

} // namespace BotRotation
