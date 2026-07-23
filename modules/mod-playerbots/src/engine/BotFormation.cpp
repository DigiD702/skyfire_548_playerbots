/*
 * Formation follow angle/distance by combat role.
 */

#include "BotFormation.h"
#include "PlayerbotAI.h"
#include "Player.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BotFormation
{
    float FollowAngle(PlayerbotAI* ai)
    {
        if (!ai || !ai->GetBot())
            return static_cast<float>(M_PI);

        Player* bot = ai->GetBot();
        // Stable per-bot spread so two same-role bots don't stack.
        float const spread = float(int(bot->GetGUIDLow() % 5) - 2) * 0.22f;

        switch (ai->GetCombatRolePublic())
        {
            case 0: // Tank — slightly ahead / beside leader (hold front)
                return 0.35f + spread;
            case 1: // Healer — behind leader
                return static_cast<float>(M_PI) + spread * 0.5f;
            default: // DPS — behind flanks
                if (ai->IsRangedClassPublic())
                    return static_cast<float>(M_PI) + (spread >= 0 ? 0.9f : -0.9f) + spread * 0.15f;
                return static_cast<float>(M_PI) + (spread >= 0 ? 0.55f : -0.55f) + spread * 0.1f;
        }
    }

    float FollowDistance(PlayerbotAI* ai)
    {
        if (!ai)
            return 2.5f;
        switch (ai->GetCombatRolePublic())
        {
            case 0: return 2.0f;  // tank
            case 1: return 3.5f;  // healer
            default:
                return ai->IsRangedClassPublic() ? 5.0f : 2.5f;
        }
    }
}
