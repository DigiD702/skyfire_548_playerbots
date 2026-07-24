/*
 * Formation follow angle/distance by combat role (unique party slots).
 */

#ifndef SF_BOT_FORMATION_H
#define SF_BOT_FORMATION_H

#include "Define.h"

class PlayerbotAI;

namespace BotFormation
{
    // Angle relative to leader facing for MoveFollow (0 = in front, PI = behind).
    float FollowAngle(PlayerbotAI* ai);
    // Follow distance yards (staggered by same-role ordinal).
    float FollowDistance(PlayerbotAI* ai);
}

#endif
