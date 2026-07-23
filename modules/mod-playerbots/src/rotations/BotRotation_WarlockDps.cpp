/*
 * Destruction / Demonology Warlock - simplified from Hekili .simc
 */

#include "BotRotationLists.h"
#include "Pet.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum DestroSpells : uint32
    {
        INCINERATE          = 29722,
        IMMOLATE            = 348,
        CONFLAGRATE         = 17962,
        CHAOS_BOLT          = 116858,
        SHADOWBURN          = 17877,
        HAVOC               = 80240,
        RAIN_OF_FIRE        = 104232,
        DARK_SOUL_INSTABILITY = 113858,
        DARK_INTENT         = 109773,
        SUMMON_IMP          = 688,
        SUMMON_INFERNAL     = 1122,
        SUMMON_DOOMGUARD    = 18540,
        LIFE_TAP            = 1454,
        BACKDRAFT           = 117828,
        FIRE_AND_BRIMSTONE  = 108683,
    };

    enum DemoSpells : uint32
    {
        SHADOW_BOLT         = 686,
        CORRUPTION          = 172,
        HAND_OF_GULDAN      = 105174,
        SOUL_FIRE           = 6353,
        METAMORPHOSIS       = 103958,
        TOUCH_OF_CHAOS      = 103964,
        DOOM                = 603,
        VOID_RAY            = 115422,
        HELLFIRE            = 1949,
        DARK_SOUL_KNOWLEDGE = 113861,
        SUMMON_FELGUARD     = 30146,
        SUMMON_DOOMGUARD_DEMO = 18540,
        MOLTEN_CORE         = 122351,
        DEMONIC_FURY_CAP    = 1000, // resource threshold for meta dump
    };
}

uint32 SelectDestruction(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const embers = bot->GetPower(POWER_BURNING_EMBERS);
    uint32 const manaMax = bot->GetMaxPower(POWER_MANA);
    float const manaPct = manaMax ? (100.0f * float(bot->GetPower(POWER_MANA)) / float(manaMax)) : 100.0f;

    if (!HasAuraUp(bot, DARK_INTENT) && CanTryCast(bot, DARK_INTENT))
        return DARK_INTENT;

    Pet* pet = bot->GetPet();
    if ((!pet || !pet->IsAlive()) && CanTryCast(bot, SUMMON_IMP))
        return SUMMON_IMP;

    if (!HasAuraUp(bot, DARK_SOUL_INSTABILITY) && CanTryCast(bot, DARK_SOUL_INSTABILITY))
        return DARK_SOUL_INSTABILITY;

    // Major pet CD: Infernal on AoE, Doomguard on ST.
    if (ctx.enemies >= 3 && CanTryCast(bot, SUMMON_INFERNAL))
        return SUMMON_INFERNAL;
    if (ctx.enemies < 3 && CanTryCast(bot, SUMMON_DOOMGUARD))
        return SUMMON_DOOMGUARD;

    // Havoc on a second target so Chaos Bolt / Incinerate cleave.
    if (ctx.enemies >= 2 && FindSecondaryEnemy(bot, target, 40.0f) && CanTryCast(bot, HAVOC))
        return HAVOC;

    if (ctx.enemies >= 3 && CanTryCast(bot, FIRE_AND_BRIMSTONE))
        return FIRE_AND_BRIMSTONE;
    if (ctx.enemies >= 3 && CanTryCast(bot, RAIN_OF_FIRE))
        return RAIN_OF_FIRE;

    if ((!HasAuraUp(target, IMMOLATE) || AuraRemains(target, IMMOLATE) <= 3.0f)
        && CanTryCast(bot, IMMOLATE))
        return IMMOLATE;

    if (ctx.targetHealthPct <= 20.0f && embers >= 1 && CanTryCast(bot, SHADOWBURN))
        return SHADOWBURN;

    if (embers >= 3 && CanTryCast(bot, CHAOS_BOLT))
        return CHAOS_BOLT;
    if (embers >= 1 && HasAuraUp(bot, DARK_SOUL_INSTABILITY) && CanTryCast(bot, CHAOS_BOLT))
        return CHAOS_BOLT;

    if (CanTryCast(bot, CONFLAGRATE))
        return CONFLAGRATE;

    if (manaPct <= 20.0f && CanTryCast(bot, LIFE_TAP))
        return LIFE_TAP;

    if (CanTryCast(bot, INCINERATE))
        return INCINERATE;

    return 0;
}

uint32 SelectDemonology(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const fury = bot->GetPower(POWER_DEMONIC_FURY);
    bool const meta = HasAuraUp(bot, METAMORPHOSIS);

    if (!HasAuraUp(bot, DARK_INTENT) && CanTryCast(bot, DARK_INTENT))
        return DARK_INTENT;

    Pet* pet = bot->GetPet();
    if ((!pet || !pet->IsAlive()) && CanTryCast(bot, SUMMON_FELGUARD))
        return SUMMON_FELGUARD;

    if (!HasAuraUp(bot, DARK_SOUL_KNOWLEDGE) && CanTryCast(bot, DARK_SOUL_KNOWLEDGE))
        return DARK_SOUL_KNOWLEDGE;

    if (CanTryCast(bot, SUMMON_DOOMGUARD_DEMO))
        return SUMMON_DOOMGUARD_DEMO;

    if (!meta && fury >= 800 && CanTryCast(bot, METAMORPHOSIS))
        return METAMORPHOSIS;

    if (meta)
    {
        if ((!HasAuraUp(target, DOOM) || AuraRemains(target, DOOM) <= 15.0f)
            && CanTryCast(bot, DOOM))
            return DOOM;
        if (ctx.enemies >= 2 && CanTryCast(bot, VOID_RAY))
            return VOID_RAY;
        if (ctx.enemies >= 3 && CanTryCast(bot, HELLFIRE))
            return HELLFIRE;
        if (CanTryCast(bot, TOUCH_OF_CHAOS))
            return TOUCH_OF_CHAOS;
    }

    if ((!HasAuraUp(target, CORRUPTION) || AuraRemains(target, CORRUPTION) <= 3.0f)
        && CanTryCast(bot, CORRUPTION))
        return CORRUPTION;

    if (CanTryCast(bot, HAND_OF_GULDAN))
        return HAND_OF_GULDAN;

    if (HasAuraUp(bot, MOLTEN_CORE) && CanTryCast(bot, SOUL_FIRE))
        return SOUL_FIRE;

    if (ctx.targetHealthPct <= 25.0f && CanTryCast(bot, SOUL_FIRE))
        return SOUL_FIRE;

    if (ctx.enemies >= 3 && !meta && CanTryCast(bot, HELLFIRE))
        return HELLFIRE;

    if (CanTryCast(bot, SHADOW_BOLT))
        return SHADOW_BOLT;

    return 0;
}

} // namespace BotRotation
