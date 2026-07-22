/*
 * Unholy Death Knight - simplified MoP priority
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum UnholySpells : uint32
    {
        DEATH_COIL          = 47541,
        FESTERING_STRIKE    = 85948,
        SCOURGE_STRIKE      = 55090,
        DEATH_AND_DECAY     = 43265,
        DARK_TRANSFORMATION = 63560,
        UNHOLY_FRENZY       = 49016,
        SUMMON_GARGOYLE     = 49206,
        OUTBREAK            = 77575,
        BLOOD_PLAGUE        = 55078,
        FROST_FEVER         = 55095,
        SOUL_REAPER         = 130736,
        BLOOD_BOIL          = 48721,
        ICY_TOUCH           = 45477,
        PLAGUE_STRIKE       = 45462,
        RAISE_DEAD          = 46584,
        HORN_OF_WINTER      = 57330,
    };
}

uint32 SelectUnholy(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const runic = bot->GetPower(POWER_RUNIC_POWER);

    if (!HasAuraUp(bot, HORN_OF_WINTER) && CanTryCast(bot, HORN_OF_WINTER))
        return HORN_OF_WINTER;

    if (CanTryCast(bot, RAISE_DEAD))
        return RAISE_DEAD;

    if (CanTryCast(bot, UNHOLY_FRENZY))
        return UNHOLY_FRENZY;
    if (CanTryCast(bot, SUMMON_GARGOYLE))
        return SUMMON_GARGOYLE;
    if (CanTryCast(bot, DARK_TRANSFORMATION))
        return DARK_TRANSFORMATION;

    bool const diseases = HasAuraUp(target, BLOOD_PLAGUE) && HasAuraUp(target, FROST_FEVER);
    if (!diseases)
    {
        if (CanTryCast(bot, OUTBREAK))
            return OUTBREAK;
        if (CanTryCast(bot, ICY_TOUCH))
            return ICY_TOUCH;
        if (CanTryCast(bot, PLAGUE_STRIKE))
            return PLAGUE_STRIKE;
    }

    if (ctx.targetHealthPct <= 35.0f && CanTryCast(bot, SOUL_REAPER))
        return SOUL_REAPER;

    if (ctx.enemies >= 2 && CanTryCast(bot, DEATH_AND_DECAY))
        return DEATH_AND_DECAY;
    if (ctx.enemies >= 2 && diseases && CanTryCast(bot, BLOOD_BOIL))
        return BLOOD_BOIL;

    if (CanTryCast(bot, FESTERING_STRIKE))
        return FESTERING_STRIKE;
    if (CanTryCast(bot, SCOURGE_STRIKE))
        return SCOURGE_STRIKE;

    if (runic >= 30 && CanTryCast(bot, DEATH_COIL))
        return DEATH_COIL;

    return 0;
}

uint32 SelectFrostDK(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const runic = bot->GetPower(POWER_RUNIC_POWER);

    enum FrostDKSpells : uint32
    {
        HOWLING_BLAST       = 49184,
        OBLITERATE          = 49020,
        FROST_STRIKE        = 49143,
        PILLAR_OF_FROST     = 51271,
        OUTBREAK            = 77575,
        ICY_TOUCH           = 45477,
        PLAGUE_STRIKE       = 45462,
        BLOOD_PLAGUE        = 55078,
        FROST_FEVER         = 55095,
        RIME                = 59052,
        KILLING_MACHINE     = 51124,
        SOUL_REAPER         = 130735,
        HORN_OF_WINTER      = 57330,
        BLOOD_BOIL          = 48721,
    };

    if (!HasAuraUp(bot, HORN_OF_WINTER) && CanTryCast(bot, HORN_OF_WINTER))
        return HORN_OF_WINTER;

    if (CanTryCast(bot, PILLAR_OF_FROST))
        return PILLAR_OF_FROST;

    bool const diseases = HasAuraUp(target, BLOOD_PLAGUE) && HasAuraUp(target, FROST_FEVER);
    if (!diseases)
    {
        if (CanTryCast(bot, OUTBREAK))
            return OUTBREAK;
        if (CanTryCast(bot, HOWLING_BLAST))
            return HOWLING_BLAST;
        if (CanTryCast(bot, PLAGUE_STRIKE))
            return PLAGUE_STRIKE;
        if (CanTryCast(bot, ICY_TOUCH))
            return ICY_TOUCH;
    }

    if (ctx.targetHealthPct <= 35.0f && CanTryCast(bot, SOUL_REAPER))
        return SOUL_REAPER;

    if (HasAuraUp(bot, KILLING_MACHINE) && runic >= 25 && CanTryCast(bot, FROST_STRIKE))
        return FROST_STRIKE;

    if (HasAuraUp(bot, RIME) && CanTryCast(bot, HOWLING_BLAST))
        return HOWLING_BLAST;

    if (ctx.enemies >= 3 && CanTryCast(bot, HOWLING_BLAST))
        return HOWLING_BLAST;
    if (ctx.enemies >= 3 && diseases && CanTryCast(bot, BLOOD_BOIL))
        return BLOOD_BOIL;

    if (CanTryCast(bot, OBLITERATE))
        return OBLITERATE;
    if (CanTryCast(bot, HOWLING_BLAST))
        return HOWLING_BLAST;

    if (runic >= 40 && CanTryCast(bot, FROST_STRIKE))
        return FROST_STRIKE;

    return 0;
}

uint32 SelectBlood(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    uint32 const runic = bot->GetPower(POWER_RUNIC_POWER);

    enum BloodSpells : uint32
    {
        BLOOD_PRESENCE      = 48263,
        BONE_SHIELD         = 49222,
        DEATH_STRIKE        = 49998,
        HEART_STRIKE        = 55050,
        BLOOD_BOIL          = 48721,
        DEATH_AND_DECAY     = 43265,
        OUTBREAK            = 77575,
        ICY_TOUCH           = 45477,
        PLAGUE_STRIKE       = 45462,
        BLOOD_PLAGUE        = 55078,
        FROST_FEVER         = 55095,
        RUNE_TAP            = 48982,
        HORN_OF_WINTER      = 57330,
        DEATH_COIL          = 47541,
    };

    if (!HasAuraUp(bot, BLOOD_PRESENCE) && CanTryCast(bot, BLOOD_PRESENCE))
        return BLOOD_PRESENCE;
    if (!HasAuraUp(bot, HORN_OF_WINTER) && CanTryCast(bot, HORN_OF_WINTER))
        return HORN_OF_WINTER;
    if (!HasAuraUp(bot, BONE_SHIELD) && CanTryCast(bot, BONE_SHIELD))
        return BONE_SHIELD;

    float const hpPct = bot->GetMaxHealth()
        ? (100.0f * float(bot->GetHealth()) / float(bot->GetMaxHealth())) : 100.0f;
    if (hpPct < 50.0f && CanTryCast(bot, RUNE_TAP))
        return RUNE_TAP;

    bool const diseases = HasAuraUp(target, BLOOD_PLAGUE) && HasAuraUp(target, FROST_FEVER);
    if (!diseases)
    {
        if (CanTryCast(bot, OUTBREAK))
            return OUTBREAK;
        if (CanTryCast(bot, ICY_TOUCH))
            return ICY_TOUCH;
        if (CanTryCast(bot, PLAGUE_STRIKE))
            return PLAGUE_STRIKE;
    }

    if (hpPct < 80.0f && CanTryCast(bot, DEATH_STRIKE))
        return DEATH_STRIKE;

    if (ctx.enemies >= 2 && CanTryCast(bot, DEATH_AND_DECAY))
        return DEATH_AND_DECAY;
    if (ctx.enemies >= 2 && diseases && CanTryCast(bot, BLOOD_BOIL))
        return BLOOD_BOIL;

    if (CanTryCast(bot, HEART_STRIKE))
        return HEART_STRIKE;
    if (CanTryCast(bot, DEATH_STRIKE))
        return DEATH_STRIKE;

    if (runic >= 40 && CanTryCast(bot, DEATH_COIL))
        return DEATH_COIL;

    return 0;
}

} // namespace BotRotation
