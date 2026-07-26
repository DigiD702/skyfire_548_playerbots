/*
 * Arms / Fury Warrior - simplified from Hekili WarriorArms/Fury.simc
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
        BATTLE_SHOUT        = 6673,
        BERSERKER_STANCE    = 2458,
        COLOSSUS_SMASH      = 86346,
        COLOSSUS_SMASH_DEBUFF = 108126, // may also be 86346 as debuff
        MORTAL_STRIKE       = 12294,
        OVERPOWER           = 7384,
        SLAM                = 1464,
        EXECUTE             = 5308,
        RECKLESSNESS        = 1719,
        AVATAR              = 107574,
        BLOODBATH           = 12292,
        SKULL_BANNER        = 114207,
        THUNDER_CLAP        = 6343,
        SWEEPING_STRIKES    = 12328,
        BLOODTHIRST         = 23881,
        RAGING_BLOW         = 85288,
        WILD_STRIKE         = 100130,
        BERSERKER_RAGE      = 18499,
        SUDDEN_DEATH        = 52437,
        TASTE_FOR_BLOOD     = 60503,
        RAGING_BLOW_STACKS  = 131116,
        ENRAGE              = 12880,
        DRAGON_ROAR         = 118000,
        STORM_BOLT          = 107570,
        BLADESTORM          = 46924,
    };
}

uint32 SelectArms(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const rage = bot->GetPower(POWER_RAGE);
    bool const cs = HasAuraUp(target, COLOSSUS_SMASH) || HasAuraUp(target, COLOSSUS_SMASH_DEBUFF)
        || AuraRemains(target, COLOSSUS_SMASH) > 0.0f;

    if (!HasAuraUp(bot, BATTLE_SHOUT) && CanTryCast(bot, BATTLE_SHOUT))
        return BATTLE_SHOUT;

    if (ctx.enemies >= 2 && CanTryCast(bot, SWEEPING_STRIKES))
        return SWEEPING_STRIKES;

    if (AuraRemains(target, COLOSSUS_SMASH) < 1.5f && CanTryCast(bot, COLOSSUS_SMASH))
        return COLOSSUS_SMASH;

    if (cs && CanTryCast(bot, AVATAR))
        return AVATAR;
    if (cs && CanTryCast(bot, RECKLESSNESS))
        return RECKLESSNESS;
    if ((HasAuraUp(bot, RECKLESSNESS) || HasAuraUp(bot, AVATAR)) && CanTryCast(bot, SKULL_BANNER))
        return SKULL_BANNER;
    if (CanTryCast(bot, BLOODBATH))
        return BLOODBATH;

    if (CanTryCast(bot, BERSERKER_RAGE))
        return BERSERKER_RAGE;

    if (ctx.enemies >= 2 && CanTryCast(bot, THUNDER_CLAP))
        return THUNDER_CLAP;
    if (ctx.enemies >= 2 && CanTryCast(bot, BLADESTORM))
        return BLADESTORM;
    if (ctx.enemies >= 2 && CanTryCast(bot, DRAGON_ROAR))
        return DRAGON_ROAR;

    if (ctx.targetHealthPct < 20.0f || HasAuraUp(bot, SUDDEN_DEATH))
        if (CanTryCast(bot, EXECUTE))
            return EXECUTE;

    if (CanTryCast(bot, MORTAL_STRIKE))
        return MORTAL_STRIKE;

    if (cs && AuraStacks(bot, TASTE_FOR_BLOOD) >= 1 && CanTryCast(bot, OVERPOWER))
        return OVERPOWER;

    if (CanTryCast(bot, STORM_BOLT))
        return STORM_BOLT;
    if (cs && CanTryCast(bot, DRAGON_ROAR))
        return DRAGON_ROAR;

    if ((cs && rage >= 25) || rage >= 60)
        if (CanTryCast(bot, SLAM))
            return SLAM;

    if (CanTryCast(bot, OVERPOWER))
        return OVERPOWER;

    return 0;
}

uint32 SelectFury(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const rage = bot->GetPower(POWER_RAGE);
    bool const cs = HasAuraUp(target, COLOSSUS_SMASH) || AuraRemains(target, COLOSSUS_SMASH) > 0.0f;

    if (!HasAuraUp(bot, BERSERKER_STANCE) && CanTryCast(bot, BERSERKER_STANCE))
        return BERSERKER_STANCE;
    if (!HasAuraUp(bot, BATTLE_SHOUT) && CanTryCast(bot, BATTLE_SHOUT))
        return BATTLE_SHOUT;

    if (!HasAuraUp(bot, ENRAGE) && CanTryCast(bot, BERSERKER_RAGE))
        return BERSERKER_RAGE;

    if (AuraRemains(target, COLOSSUS_SMASH) < 1.5f && CanTryCast(bot, COLOSSUS_SMASH))
        return COLOSSUS_SMASH;

    if (cs && CanTryCast(bot, RECKLESSNESS))
        return RECKLESSNESS;

    if (ctx.enemies >= 2 && CanTryCast(bot, BLADESTORM))
        return BLADESTORM;
    if (ctx.enemies >= 2 && CanTryCast(bot, DRAGON_ROAR))
        return DRAGON_ROAR;
    if (ctx.enemies >= 2 && CanTryCast(bot, THUNDER_CLAP))
        return THUNDER_CLAP;

    if (ctx.targetHealthPct < 20.0f && CanTryCast(bot, EXECUTE))
        return EXECUTE;

    if (!HasAuraUp(bot, ENRAGE) || CanTryCast(bot, BLOODTHIRST))
        if (CanTryCast(bot, BLOODTHIRST))
            return BLOODTHIRST;

    if (cs && AuraStacks(bot, RAGING_BLOW_STACKS) >= 1 && CanTryCast(bot, RAGING_BLOW))
        return RAGING_BLOW;

    if (CanTryCast(bot, STORM_BOLT))
        return STORM_BOLT;
    if (cs && CanTryCast(bot, DRAGON_ROAR))
        return DRAGON_ROAR;

    if (AuraStacks(bot, RAGING_BLOW_STACKS) >= 1 && CanTryCast(bot, RAGING_BLOW))
        return RAGING_BLOW;

    if (rage >= 60 && CanTryCast(bot, WILD_STRIKE))
        return WILD_STRIKE;

    if (CanTryCast(bot, BLOODTHIRST))
        return BLOODTHIRST;

    return 0;
}

uint32 SelectProtectionWarrior(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const rage = bot->GetPower(POWER_RAGE);

    enum ProtWarSpells : uint32
    {
        DEFENSIVE_STANCE    = 71,
        BATTLE_SHOUT        = 6673,
        SHIELD_SLAM         = 23922,
        REVENGE             = 6572,
        DEVASTATE           = 20243,
        SUNDER_ARMOR        = 7386,
        WEAKENED_ARMOR      = 113746,
        THUNDER_CLAP        = 6343,
        WEAKENED_BLOWS      = 115798, // applied by Thunder Clap
        SHIELD_BLOCK        = 2565,
        SHIELD_BLOCK_BUFF   = 132404,
        HEROIC_STRIKE       = 78,
        CLEAVE              = 845,
        SHIELD_WALL         = 871,
        DEMORALIZING_SHOUT  = 1160,
        SHOCKWAVE           = 46968,
        HEROIC_THROW        = 57755,
    };

    bool const inDefensive = bot->GetShapeshiftForm() == FORM_DEFENSIVESTANCE
        || HasAuraUp(bot, DEFENSIVE_STANCE);
    if (!inDefensive && CanTryCast(bot, DEFENSIVE_STANCE))
        return DEFENSIVE_STANCE;
    if (!HasAuraUp(bot, BATTLE_SHOUT) && CanTryCast(bot, BATTLE_SHOUT))
        return BATTLE_SHOUT;

    // Must be a real shield — any off-hand item used to starve the rotation on
    // failed Shield Block / Shield Slam casts.
    bool const hasShield = HasShieldEquipped(bot);

    float const hpPct = bot->GetMaxHealth()
        ? (100.0f * float(bot->GetHealth()) / float(bot->GetMaxHealth())) : 100.0f;
    if (hasShield && hpPct < 40.0f && CanTryCast(bot, SHIELD_WALL))
        return SHIELD_WALL;

    // Only attempt Shield Block when stance + shield are valid; otherwise the
    // selector would return it every GCD, CastSpell would fail, and warriors
    // would sit on auto-attacks (no class filler).
    if (hasShield && inDefensive
        && !HasAuraUp(bot, SHIELD_BLOCK_BUFF) && !HasAuraUp(bot, SHIELD_BLOCK)
        && CanTryCast(bot, SHIELD_BLOCK))
        return SHIELD_BLOCK;

    // Single-target threat builders first — Thunder Clap used to sit above these
    // whenever enemies >= 2, so peel/pack tanks never Shield Slam / Revenge.
    if (hasShield && CanTryCast(bot, SHIELD_SLAM))
        return SHIELD_SLAM;
    if (CanTryCast(bot, REVENGE))
        return REVENGE;

    if (ctx.enemies >= 2)
    {
        if (CanTryCast(bot, SHOCKWAVE))
            return SHOCKWAVE;
        // Keep Weakened Blows up; do not hard-cast TC every GCD before Devastate.
        if (target
            && (!HasAuraUp(target, WEAKENED_BLOWS) || AuraRemains(target, WEAKENED_BLOWS) <= 3.0f)
            && CanTryCast(bot, THUNDER_CLAP))
            return THUNDER_CLAP;
        if (CanTryCast(bot, DEMORALIZING_SHOUT))
            return DEMORALIZING_SHOUT;
    }

    // Devastate replaces Sunder at higher levels; use Sunder while learning.
    if (CanTryCast(bot, DEVASTATE))
        return DEVASTATE;
    if (target && CanTryCast(bot, SUNDER_ARMOR))
    {
        uint32 const armorStacks = AuraStacks(target, WEAKENED_ARMOR);
        if (armorStacks < 3 || NeedsMyAuraRefresh(bot, target, WEAKENED_ARMOR, 3.0f))
            return SUNDER_ARMOR;
    }

    // ST Weakened Blows via Thunder Clap when nothing else is ready.
    if (target
        && (!HasAuraUp(target, WEAKENED_BLOWS) || AuraRemains(target, WEAKENED_BLOWS) <= 3.0f)
        && CanTryCast(bot, THUNDER_CLAP))
        return THUNDER_CLAP;

    // Rage dumps (cost 30). Dump from 40 so level-20 tanks actually spend rage.
    if (ctx.enemies >= 2 && rage >= 40 && CanTryCast(bot, CLEAVE))
        return CLEAVE;
    if (rage >= 40 && CanTryCast(bot, HEROIC_STRIKE))
        return HEROIC_STRIKE;

    // Out of melee fillers while chasing a peel.
    if (target && !bot->IsWithinMeleeRange(target) && CanTryCast(bot, HEROIC_THROW))
        return HEROIC_THROW;

    return 0;
}

} // namespace BotRotation
