/*
 * Declarations for per-spec rotation selectors (Wave 1 + Wave 2).
 */

#ifndef _SF_BOT_ROTATION_LISTS_H
#define _SF_BOT_ROTATION_LISTS_H

#include "BotRotation.h"

namespace BotRotation
{
    // Wave 1
    uint32 SelectRetribution(Context const& ctx);
    uint32 SelectWindwalker(Context const& ctx);
    uint32 SelectBeastMastery(Context const& ctx);
    uint32 SelectShadow(Context const& ctx);
    uint32 SelectAffliction(Context const& ctx);
    uint32 SelectElemental(Context const& ctx);

    // Wave 2
    uint32 SelectEnhancement(Context const& ctx);
    uint32 SelectFeral(Context const& ctx);
    uint32 SelectMarksmanship(Context const& ctx);
    uint32 SelectSurvival(Context const& ctx);
    uint32 SelectArms(Context const& ctx);
    uint32 SelectFury(Context const& ctx);
    uint32 SelectCombat(Context const& ctx);
    uint32 SelectFrostMage(Context const& ctx);
    uint32 SelectDestruction(Context const& ctx);
    uint32 SelectDemonology(Context const& ctx);
    uint32 SelectUnholy(Context const& ctx);
}

#endif // _SF_BOT_ROTATION_LISTS_H
