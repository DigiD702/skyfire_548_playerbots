/*
 * Brewmaster / Windwalker — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum MonkSpells : uint32
    {
        STANCE_STURDY_OX    = 115069,
        STANCE_FIERCE_TIGER = 103985,
        KEG_SMASH           = 121253,
        BLACKOUT_KICK       = 100784,
        TIGER_PALM          = 100787,
        TIGER_POWER         = 125359,
        JAB                 = 100780,
        PURIFYING_BREW      = 119582,
        SHUFFLE             = 115307,
        RISING_SUN_KICK     = 107428,
        FISTS_OF_FURY       = 113656,
    };
}

uint32 SelectBrewmaster(Context const& ctx)
{
    Player* bot = ctx.bot;
    if (!bot)
        return 0;

    if (!HasAuraUp(bot, STANCE_STURDY_OX) && CanTryCast(bot, STANCE_STURDY_OX))
        return STANCE_STURDY_OX;

    if (CanTryCast(bot, KEG_SMASH))
        return KEG_SMASH;

    if (NeedsMyAuraRefresh(bot, bot, SHUFFLE, 3.0f) && CanTryCast(bot, BLACKOUT_KICK))
        return BLACKOUT_KICK;

    if (NeedsMyAuraRefresh(bot, bot, TIGER_POWER, 3.0f) && CanTryCast(bot, TIGER_PALM))
        return TIGER_PALM;

    if (CanTryCast(bot, JAB))
        return JAB;

    if (CanTryCast(bot, PURIFYING_BREW))
        return PURIFYING_BREW;

    return 0;
}

uint32 SelectWindwalker(Context const& ctx)
{
    Player* bot = ctx.bot;
    if (!bot)
        return 0;

    if (!HasAuraUp(bot, STANCE_FIERCE_TIGER) && CanTryCast(bot, STANCE_FIERCE_TIGER))
        return STANCE_FIERCE_TIGER;

    if (CanTryCast(bot, RISING_SUN_KICK))
        return RISING_SUN_KICK;

    if (NeedsMyAuraRefresh(bot, bot, TIGER_POWER, 3.0f) && CanTryCast(bot, TIGER_PALM))
        return TIGER_PALM;

    if (CanTryCast(bot, FISTS_OF_FURY))
        return FISTS_OF_FURY;

    if (CanTryCast(bot, BLACKOUT_KICK))
        return BLACKOUT_KICK;

    if (CanTryCast(bot, JAB))
        return JAB;

    return 0;
}

} // namespace BotRotation
