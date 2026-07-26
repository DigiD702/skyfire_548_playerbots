// Beast Mastery / Marksmanship / Survival — modules/mod-playerbots/rotation.md
// Unknown (low-level) spells are skipped via CanTryCast.

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum HunterSpells : uint32
    {
        HUNTERS_MARK    = 1130,
        SERPENT_STING   = 1978,
        KILL_COMMAND    = 34026,
        ARCANE_SHOT     = 3044,
        COBRA_SHOT      = 77767,
        STEADY_SHOT     = 56641,
        CHIMERA_SHOT    = 53209,
        AIMED_SHOT      = 19434,
        BLACK_ARROW     = 3674,
        EXPLOSIVE_SHOT  = 53301,
    };

    bool NeedsMark(Player* bot, Unit* target)
    {
        return NeedsMyAuraRefresh(bot, target, HUNTERS_MARK, 3.0f);
    }

    bool NeedsSerpent(Player* bot, Unit* target)
    {
        return NeedsMyAuraRefresh(bot, target, SERPENT_STING, 3.0f);
    }
}

uint32 SelectBeastMastery(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    if (NeedsMark(bot, target) && CanTryCast(bot, HUNTERS_MARK))
        return HUNTERS_MARK;
    if (NeedsSerpent(bot, target) && CanTryCast(bot, SERPENT_STING))
        return SERPENT_STING;
    if (CanTryCast(bot, KILL_COMMAND))
        return KILL_COMMAND;
    if (bot->GetPower(POWER_FOCUS) >= 30 && CanTryCast(bot, ARCANE_SHOT))
        return ARCANE_SHOT;
    if (CanTryCast(bot, COBRA_SHOT))
        return COBRA_SHOT;

    return 0;
}

uint32 SelectMarksmanship(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    if (NeedsMark(bot, target) && CanTryCast(bot, HUNTERS_MARK))
        return HUNTERS_MARK;
    if (NeedsSerpent(bot, target) && CanTryCast(bot, SERPENT_STING))
        return SERPENT_STING;
    if (CanTryCast(bot, CHIMERA_SHOT))
        return CHIMERA_SHOT;
    if (CanTryCast(bot, AIMED_SHOT))
        return AIMED_SHOT;
    if (CanTryCast(bot, STEADY_SHOT))
        return STEADY_SHOT;

    return 0;
}

uint32 SelectSurvival(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    if (NeedsMark(bot, target) && CanTryCast(bot, HUNTERS_MARK))
        return HUNTERS_MARK;
    if (NeedsSerpent(bot, target) && CanTryCast(bot, SERPENT_STING))
        return SERPENT_STING;
    if (CanTryCast(bot, BLACK_ARROW))
        return BLACK_ARROW;
    if (CanTryCast(bot, EXPLOSIVE_SHOT))
        return EXPLOSIVE_SHOT;
    if (bot->GetPower(POWER_FOCUS) >= 30 && CanTryCast(bot, ARCANE_SHOT))
        return ARCANE_SHOT;
    if (CanTryCast(bot, COBRA_SHOT))
        return COBRA_SHOT;

    return 0;
}

} // namespace BotRotation
