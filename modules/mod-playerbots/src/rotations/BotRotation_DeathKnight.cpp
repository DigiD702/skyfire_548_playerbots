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

} // namespace BotRotation
