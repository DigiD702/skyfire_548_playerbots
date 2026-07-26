/*
 * Assassination / Combat / Subtlety — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Item.h"
#include "ItemPrototype.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum RogueSpells : uint32
    {
        DEADLY_POISON       = 2823,
        MUTILATE            = 1329,
        RUPTURE             = 1943,
        ENVENOM             = 32645,
        DISPATCH            = 111240,
        BLINDSIDE           = 121153,
        REVEALING_STRIKE    = 84617,
        SINISTER_STRIKE     = 1752,
        SLICE_AND_DICE      = 5171,
        EVISCERATE          = 2098,
        HEMORRHAGE          = 16511,
        BACKSTAB            = 53,
    };

    bool HasMainhandWeaponImbue(Player* bot)
    {
        if (!bot)
            return false;
        if (Item* mh = bot->GetWeaponForAttack(WeaponAttackType::BASE_ATTACK))
            return mh->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT) != 0;
        return false;
    }
}

uint32 SelectAssassination(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    int8 const cp = ctx.comboPoints;

    // Buff: Deadly Poison
    if (!HasMainhandWeaponImbue(bot) && CanTryCast(bot, DEADLY_POISON))
        return DEADLY_POISON;

    if (CanTryCast(bot, MUTILATE))
        return MUTILATE;

    if (NeedsMyAuraRefresh(bot, target, RUPTURE, 3.0f) && cp >= 1 && CanTryCast(bot, RUPTURE))
        return RUPTURE;

    if (cp >= 4 && CanTryCast(bot, ENVENOM))
        return ENVENOM;

    if ((ctx.targetHealthPct <= 30.0f || HasAuraUp(bot, BLINDSIDE)) && CanTryCast(bot, DISPATCH))
        return DISPATCH;

    return 0;
}

uint32 SelectCombat(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    int8 const cp = ctx.comboPoints;

    // Buff: Deadly Poison
    if (!HasMainhandWeaponImbue(bot) && CanTryCast(bot, DEADLY_POISON))
        return DEADLY_POISON;

    if (NeedsMyAuraRefresh(bot, target, REVEALING_STRIKE, 3.0f) && CanTryCast(bot, REVEALING_STRIKE))
        return REVEALING_STRIKE;

    if (CanTryCast(bot, SINISTER_STRIKE))
        return SINISTER_STRIKE;

    if (NeedsMyAuraRefresh(bot, bot, SLICE_AND_DICE, 3.0f) && cp >= 1 && CanTryCast(bot, SLICE_AND_DICE))
        return SLICE_AND_DICE;

    if (cp >= 5 && CanTryCast(bot, EVISCERATE))
        return EVISCERATE;

    return 0;
}

uint32 SelectSubtlety(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    int8 const cp = ctx.comboPoints;

    // Buff: Deadly Poison
    if (!HasMainhandWeaponImbue(bot) && CanTryCast(bot, DEADLY_POISON))
        return DEADLY_POISON;

    if (NeedsMyAuraRefresh(bot, target, HEMORRHAGE, 3.0f) && CanTryCast(bot, HEMORRHAGE))
        return HEMORRHAGE;

    if (CanTryCast(bot, BACKSTAB))
        return BACKSTAB;

    if (NeedsMyAuraRefresh(bot, bot, SLICE_AND_DICE, 3.0f) && cp >= 1 && CanTryCast(bot, SLICE_AND_DICE))
        return SLICE_AND_DICE;

    if (cp >= 1 && CanTryCast(bot, RUPTURE))
        return RUPTURE;

    if (cp >= 5 && CanTryCast(bot, EVISCERATE))
        return EVISCERATE;

    return 0;
}

} // namespace BotRotation
