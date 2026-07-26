/*
 * Arms / Fury / Protection — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum WarriorSpells : uint32
    {
        BATTLE_SHOUT       = 6673,
        BATTLE_STANCE      = 2457,
        DEFENSIVE_STANCE   = 71,
        BERSERKER_STANCE   = 2458,

        // Arms
        MORTAL_STRIKE      = 12294,
        COLOSSUS_SMASH     = 86346,
        OVERPOWER          = 7384,
        SLAM               = 1464,
        HEROIC_STRIKE      = 78,
        EXECUTE            = 5308,
        TASTE_FOR_BLOOD    = 60503,

        // Fury
        BLOODTHIRST        = 23881,
        RAGING_BLOW        = 85288,
        RAGING_BLOW_STACKS = 131116,
        WILD_STRIKE        = 100130,
        BLOODSURGE         = 46916,

        // Protection
        SHIELD_SLAM        = 23922,
        REVENGE            = 6572,
        DEVASTATE          = 20243,
        SUNDER_ARMOR       = 7386,   // low-level until Devastate
        SHIELD_BLOCK       = 2565,   // aura 132404
        SHIELD_BARRIER     = 112048,
        THUNDER_CLAP       = 6343,
        HEROIC_THROW       = 57755,
    };
}

uint32 SelectArms(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const rage = bot->GetPower(POWER_RAGE) / 10;

    if (bot->GetShapeshiftForm() != FORM_BATTLESTANCE
        && !HasAuraUp(bot, BATTLE_STANCE)
        && CanTryCast(bot, BATTLE_STANCE))
        return BATTLE_STANCE;
    if (!HasAuraUp(bot, BATTLE_SHOUT) && CanTryCast(bot, BATTLE_SHOUT))
        return BATTLE_SHOUT;

    // Execute below 20%
    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, EXECUTE))
        return EXECUTE;

    // Mortal Strike > Colossus Smash > Overpower > Slam > Heroic Strike
    if (CanTryCast(bot, MORTAL_STRIKE))
        return MORTAL_STRIKE;
    if (CanTryCast(bot, COLOSSUS_SMASH))
        return COLOSSUS_SMASH;
    if ((HasAuraUp(bot, TASTE_FOR_BLOOD) || CanTryCast(bot, OVERPOWER))
        && CanTryCast(bot, OVERPOWER))
        return OVERPOWER;
    if (CanTryCast(bot, SLAM))
        return SLAM;
    if (rage >= 70 && CanTryCast(bot, HEROIC_STRIKE))
        return HEROIC_STRIKE;

    return 0;
}

uint32 SelectFury(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const rage = bot->GetPower(POWER_RAGE) / 10;

    if (!HasAuraUp(bot, BERSERKER_STANCE) && CanTryCast(bot, BERSERKER_STANCE))
        return BERSERKER_STANCE;
    if (!HasAuraUp(bot, BATTLE_SHOUT) && CanTryCast(bot, BATTLE_SHOUT))
        return BATTLE_SHOUT;

    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, EXECUTE))
        return EXECUTE;

    // Bloodthirst > Colossus Smash > Raging Blow > Wild Strike > Heroic Strike
    if (CanTryCast(bot, BLOODTHIRST))
        return BLOODTHIRST;
    if (CanTryCast(bot, COLOSSUS_SMASH))
        return COLOSSUS_SMASH;
    if (AuraStacks(bot, RAGING_BLOW_STACKS) >= 1 && CanTryCast(bot, RAGING_BLOW))
        return RAGING_BLOW;
    if (HasAuraUp(bot, BLOODSURGE) && CanTryCast(bot, WILD_STRIKE))
        return WILD_STRIKE;
    if (rage >= 70 && CanTryCast(bot, HEROIC_STRIKE))
        return HEROIC_STRIKE;
    // Wild Strike as rage dump if known and no Bloodsurge
    if (rage >= 80 && CanTryCast(bot, WILD_STRIKE))
        return WILD_STRIKE;

    return 0;
}

uint32 SelectProtectionWarrior(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot)
        return 0;

    uint32 const rage = bot->GetPower(POWER_RAGE) / 10;
    float const hpPct = bot->GetMaxHealth()
        ? (100.0f * float(bot->GetHealth()) / float(bot->GetMaxHealth())) : 100.0f;
    bool const hasShield = HasShieldEquipped(bot);

    bool const inDefensive = bot->GetShapeshiftForm() == FORM_DEFENSIVESTANCE
        || HasAuraUp(bot, DEFENSIVE_STANCE);
    if (!inDefensive && CanTryCast(bot, DEFENSIVE_STANCE))
        return DEFENSIVE_STANCE;
    if (!HasAuraUp(bot, BATTLE_SHOUT) && CanTryCast(bot, BATTLE_SHOUT))
        return BATTLE_SHOUT;

    // Shield Slam > Revenge; pack pulls Thunder Clap early for Weakened Blows / threat.
    if (hasShield && CanTryCast(bot, SHIELD_SLAM))
        return SHIELD_SLAM;
    if (CanTryCast(bot, REVENGE))
        return REVENGE;

    if (ctx.enemies >= 2 && CanTryCast(bot, THUNDER_CLAP))
        return THUNDER_CLAP;

    if (ctx.targetHealthPct <= 20.0f && CanTryCast(bot, EXECUTE))
        return EXECUTE;

    if (CanTryCast(bot, DEVASTATE))
        return DEVASTATE;
    if (CanTryCast(bot, SUNDER_ARMOR))
        return SUNDER_ARMOR;

    // Shield Block or Shield Barrier (spend Rage to survive)
    if (hasShield && inDefensive && rage >= 60 && hpPct < 85.0f)
    {
        if (AuraRemains(bot, SHIELD_BLOCK) < 1.0f && CanTryCast(bot, SHIELD_BLOCK))
            return SHIELD_BLOCK;
        if (AuraRemains(bot, SHIELD_BARRIER) < 1.0f && CanTryCast(bot, SHIELD_BARRIER))
            return SHIELD_BARRIER;
    }

    if (rage >= 70 && CanTryCast(bot, HEROIC_STRIKE))
        return HEROIC_STRIKE;

    if (target && !bot->IsWithinMeleeRange(target) && CanTryCast(bot, HEROIC_THROW))
        return HEROIC_THROW;

    return 0;
}

} // namespace BotRotation
