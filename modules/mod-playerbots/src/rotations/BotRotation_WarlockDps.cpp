/*
 * Destruction / Demonology — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum DestroSpells : uint32
    {
        DARK_INTENT = 109773,
        IMMOLATE    = 348,
        CONFLAGRATE = 17962,
        CHAOS_BOLT  = 116858,
        INCINERATE  = 29722,
        SHADOW_BOLT = 686, // low-level until Incinerate
    };

    enum DemoSpells : uint32
    {
        DARK_INTENT_DEMO = 109773,
        CORRUPTION       = 172,    // aura 146739
        HAND_OF_GULDAN   = 105174,
        SOUL_FIRE        = 6353,
        SHADOW_BOLT_DEMO = 686,
        METAMORPHOSIS    = 103958,
        TOUCH_OF_CHAOS   = 103964,
        MOLTEN_CORE      = 122351,
    };
}

uint32 SelectDestruction(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const embers = bot->GetPower(POWER_BURNING_EMBERS);

    if (!HasAuraUp(bot, DARK_INTENT) && CanTryCast(bot, DARK_INTENT))
        return DARK_INTENT;

    // Immolate (keep active)
    if (NeedsMyAuraRefresh(bot, target, IMMOLATE, 3.0f) && CanTryCast(bot, IMMOLATE))
        return IMMOLATE;

    // Conflagrate (build Burning Embers)
    if (CanTryCast(bot, CONFLAGRATE))
        return CONFLAGRATE;

    // Chaos Bolt (dump Embers)
    if (embers >= 1 && CanTryCast(bot, CHAOS_BOLT))
        return CHAOS_BOLT;

    // Incinerate filler (Shadow Bolt if Incinerate unknown)
    if (CanTryCast(bot, INCINERATE))
        return INCINERATE;
    if (CanTryCast(bot, SHADOW_BOLT))
        return SHADOW_BOLT;

    return 0;
}

uint32 SelectDemonology(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const fury = bot->GetPower(POWER_DEMONIC_FURY);
    bool const meta = HasAuraUp(bot, METAMORPHOSIS);

    if (!HasAuraUp(bot, DARK_INTENT_DEMO) && CanTryCast(bot, DARK_INTENT_DEMO))
        return DARK_INTENT_DEMO;

    // Corruption (keep active)
    if (NeedsMyAuraRefresh(bot, target, CORRUPTION, 3.0f) && CanTryCast(bot, CORRUPTION))
        return CORRUPTION;

    // Metamorphosis to dump Demonic Fury → Touch of Chaos
    if (!meta && fury >= 800 && CanTryCast(bot, METAMORPHOSIS))
        return METAMORPHOSIS;
    if (meta && CanTryCast(bot, TOUCH_OF_CHAOS))
        return TOUCH_OF_CHAOS;

    // Hand of Gul'dan (build Demonic Fury)
    if (CanTryCast(bot, HAND_OF_GULDAN))
        return HAND_OF_GULDAN;

    // Soul Fire on Molten Core proc
    if (HasAuraUp(bot, MOLTEN_CORE) && CanTryCast(bot, SOUL_FIRE))
        return SOUL_FIRE;

    // Shadow Bolt filler
    if (CanTryCast(bot, SHADOW_BOLT_DEMO))
        return SHADOW_BOLT_DEMO;

    return 0;
}

} // namespace BotRotation
