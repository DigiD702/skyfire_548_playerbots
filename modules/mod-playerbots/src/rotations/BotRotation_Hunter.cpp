/*
 * Beast Mastery Hunter - simplified from Hekili HunterBeastMastery.simc
 */

#include "BotRotationLists.h"
#include "Pet.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum BmSpells : uint32
    {
        ASPECT_HAWK         = 13165,
        ASPECT_IRON_HAWK    = 109260,
        CALL_PET            = 883,
        MEND_PET            = 136,
        BESTIAL_WRATH       = 19574,
        KILL_COMMAND        = 34026,
        KILL_SHOT           = 53351,
        ARCANE_SHOT         = 3044,
        COBRA_SHOT          = 77767,
        MULTI_SHOT          = 2643,
        SERPENT_STING       = 1978,
        SERPENT_STING_DOT   = 118253,
        FOCUS_FIRE          = 82692,
        RAPID_FIRE          = 3045,
        STAMPEDE            = 121818,
        DIRE_BEAST          = 120679,
        MURDER_OF_CROWS     = 131894,
        GLAIVE_TOSS         = 117050,
        BARRAGE             = 120360,
        FERVOR              = 82726,
        FRENZY              = 19615,
    };

    bool PetAlive(Player* bot)
    {
        Pet* pet = bot->GetPet();
        return pet && pet->IsAlive();
    }
}

uint32 SelectBeastMastery(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const focus = bot->GetPower(POWER_FOCUS);
    bool const bw = HasAuraUp(bot, BESTIAL_WRATH);
    bool const petOk = PetAlive(bot);

    if (!HasAuraUp(bot, ASPECT_IRON_HAWK) && !HasAuraUp(bot, ASPECT_HAWK))
    {
        if (CanTryCast(bot, ASPECT_IRON_HAWK))
            return ASPECT_IRON_HAWK;
        if (CanTryCast(bot, ASPECT_HAWK))
            return ASPECT_HAWK;
    }

    if (!petOk && CanTryCast(bot, CALL_PET))
        return CALL_PET;

    if (petOk)
    {
        if (Pet* pet = bot->GetPet())
            if (pet->GetHealthPct() <= 50.0f && CanTryCast(bot, MEND_PET))
                return MEND_PET;
    }

    if (petOk && focus >= 50 && CanTryCast(bot, BESTIAL_WRATH))
        return BESTIAL_WRATH;

    if ((bw || !SpellReady(bot, BESTIAL_WRATH)) && CanTryCast(bot, RAPID_FIRE))
        return RAPID_FIRE;

    if ((bw || ctx.targetHealthPct <= 20.0f) && CanTryCast(bot, STAMPEDE))
        return STAMPEDE;

    if (CanTryCast(bot, MURDER_OF_CROWS))
        return MURDER_OF_CROWS;

    if (CanTryCast(bot, DIRE_BEAST))
        return DIRE_BEAST;

    if (petOk && AuraStacks(bot->GetPet(), FRENZY) >= 5 && !bw && CanTryCast(bot, FOCUS_FIRE))
        return FOCUS_FIRE;

    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, KILL_SHOT))
        return KILL_SHOT;

    if (ctx.enemies >= 3 && focus >= 40 && CanTryCast(bot, MULTI_SHOT))
        return MULTI_SHOT;

    if (petOk && CanTryCast(bot, KILL_COMMAND))
        return KILL_COMMAND;

    if (focus <= 40 && CanTryCast(bot, FERVOR))
        return FERVOR;

    if (CanTryCast(bot, GLAIVE_TOSS) && focus >= 15)
        return GLAIVE_TOSS;

    if (CanTryCast(bot, BARRAGE) && focus >= 40)
        return BARRAGE;

    if (!HasAuraUp(target, SERPENT_STING_DOT) && CanTryCast(bot, SERPENT_STING))
        return SERPENT_STING;

    if (AuraRemains(target, SERPENT_STING_DOT) <= 6.0f && CanTryCast(bot, COBRA_SHOT))
        return COBRA_SHOT;

    if ((bw && focus >= 35) || focus >= 79)
        if (CanTryCast(bot, ARCANE_SHOT))
            return ARCANE_SHOT;

    if (CanTryCast(bot, COBRA_SHOT))
        return COBRA_SHOT;

    return 0;
}

uint32 SelectMarksmanship(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const focus = bot->GetPower(POWER_FOCUS);

    enum MmSpells : uint32
    {
        ASPECT_HAWK         = 13165,
        CHIMERA_SHOT        = 53209,
        AIMED_SHOT          = 19434,
        STEADY_SHOT         = 56641,
        KILL_SHOT           = 53351,
        ARCANE_SHOT         = 3044,
        MULTI_SHOT          = 2643,
        SERPENT_STING       = 1978,
        SERPENT_STING_DOT   = 118253,
        RAPID_FIRE          = 3045,
        STAMPEDE            = 121818,
        DIRE_BEAST          = 120679,
        MURDER_OF_CROWS     = 131894,
        GLAIVE_TOSS         = 117050,
        BARRAGE             = 120360,
    };

    if (!HasAuraUp(bot, ASPECT_HAWK) && CanTryCast(bot, ASPECT_HAWK))
        return ASPECT_HAWK;

    if (CanTryCast(bot, RAPID_FIRE))
        return RAPID_FIRE;
    if (CanTryCast(bot, STAMPEDE))
        return STAMPEDE;
    if (CanTryCast(bot, DIRE_BEAST))
        return DIRE_BEAST;
    if (CanTryCast(bot, MURDER_OF_CROWS))
        return MURDER_OF_CROWS;

    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, KILL_SHOT))
        return KILL_SHOT;

    if (ctx.enemies >= 3 && focus >= 40 && CanTryCast(bot, MULTI_SHOT))
        return MULTI_SHOT;
    if (CanTryCast(bot, BARRAGE) && focus >= 40)
        return BARRAGE;
    if (CanTryCast(bot, GLAIVE_TOSS) && focus >= 15)
        return GLAIVE_TOSS;

    if (!HasAuraUp(target, SERPENT_STING_DOT) && CanTryCast(bot, SERPENT_STING))
        return SERPENT_STING;

    if (CanTryCast(bot, CHIMERA_SHOT))
        return CHIMERA_SHOT;

    // Careful Aim window (target above 80%): dump Aimed Shot.
    if (ctx.targetHealthPct >= 80.0f && focus >= 50 && CanTryCast(bot, AIMED_SHOT))
        return AIMED_SHOT;

    if (focus >= 50 && CanTryCast(bot, AIMED_SHOT))
        return AIMED_SHOT;

    if (focus >= 70 && CanTryCast(bot, ARCANE_SHOT))
        return ARCANE_SHOT;

    if (CanTryCast(bot, STEADY_SHOT))
        return STEADY_SHOT;

    return 0;
}

uint32 SelectSurvival(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const focus = bot->GetPower(POWER_FOCUS);

    enum SurvSpells : uint32
    {
        ASPECT_HAWK         = 13165,
        EXPLOSIVE_SHOT      = 53301,
        BLACK_ARROW         = 3674,
        LOCK_AND_LOAD       = 56453,
        SERPENT_STING       = 1978,
        SERPENT_STING_DOT   = 118253,
        ARCANE_SHOT         = 3044,
        COBRA_SHOT          = 77767,
        MULTI_SHOT          = 2643,
        KILL_SHOT           = 53351,
        RAPID_FIRE          = 3045,
        EXPLOSIVE_TRAP      = 13813,
        GLAIVE_TOSS         = 117050,
        BARRAGE             = 120360,
        DIRE_BEAST          = 120679,
        A_MURDER_OF_CROWS   = 131894,
    };

    if (!HasAuraUp(bot, ASPECT_HAWK) && CanTryCast(bot, ASPECT_HAWK))
        return ASPECT_HAWK;

    if (CanTryCast(bot, RAPID_FIRE))
        return RAPID_FIRE;
    if (CanTryCast(bot, A_MURDER_OF_CROWS))
        return A_MURDER_OF_CROWS;
    if (CanTryCast(bot, DIRE_BEAST))
        return DIRE_BEAST;

    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, KILL_SHOT))
        return KILL_SHOT;

    if (!HasAuraUp(target, BLACK_ARROW) && CanTryCast(bot, BLACK_ARROW))
        return BLACK_ARROW;

    if (!HasAuraUp(target, SERPENT_STING_DOT) && CanTryCast(bot, SERPENT_STING))
        return SERPENT_STING;

    if (HasAuraUp(bot, LOCK_AND_LOAD) && CanTryCast(bot, EXPLOSIVE_SHOT))
        return EXPLOSIVE_SHOT;
    if (CanTryCast(bot, EXPLOSIVE_SHOT))
        return EXPLOSIVE_SHOT;

    if (ctx.enemies >= 2 && focus >= 40 && CanTryCast(bot, MULTI_SHOT))
        return MULTI_SHOT;
    if (ctx.enemies >= 3 && CanTryCast(bot, EXPLOSIVE_TRAP))
        return EXPLOSIVE_TRAP;
    if (CanTryCast(bot, BARRAGE) && focus >= 40)
        return BARRAGE;
    if (CanTryCast(bot, GLAIVE_TOSS) && focus >= 15)
        return GLAIVE_TOSS;

    if (focus >= 70 && CanTryCast(bot, ARCANE_SHOT))
        return ARCANE_SHOT;

    if (CanTryCast(bot, COBRA_SHOT))
        return COBRA_SHOT;

    return 0;
}

} // namespace BotRotation
