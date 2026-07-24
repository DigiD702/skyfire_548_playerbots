/*
 * Cast-safe movement helpers.
 */

#include "BotMovement.h"

#include "MotionMaster.h"
#include "Object.h"
#include "Player.h"
#include "Unit.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BotMovement
{
    bool IsCasting(Player* bot)
    {
        return bot && (bot->IsNonMeleeSpellCasted(false) || bot->HasUnitState(UNIT_STATE_CASTING));
    }

    bool CanMove(Player* bot)
    {
        return bot && bot->IsAlive() && !IsCasting(bot);
    }

    bool StopAndIdle(Player* bot)
    {
        if (!CanMove(bot))
            return false;
        if (!bot->IsStopped())
            bot->StopMoving();
        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != IDLE_MOTION_TYPE)
        {
            bot->GetMotionMaster()->Clear();
            bot->GetMotionMaster()->MoveIdle();
        }
        return true;
    }

    bool ClearMotion(Player* bot)
    {
        if (!CanMove(bot))
            return false;
        bot->GetMotionMaster()->Clear();
        return true;
    }

    bool ComputeFollowPoint(Unit* leader, Player* bot, float distance, float angle,
        float& x, float& y, float& z)
    {
        if (!leader || !bot)
            return false;

        // Same convention as FollowMovementGenerator / GetClosePoint:
        // absolute angle = leader orientation + relative angle.
        float const size = bot->GetObjectSize();
        leader->GetClosePoint(x, y, z, size, distance, angle);

        // Re-clamp Z in case GetClosePoint's first pass landed in bad geometry.
        leader->UpdateAllowedPositionZ(x, y, z);
        bot->UpdateAllowedPositionZ(x, y, z);
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    bool IsBadlyOffGround(Player* bot, float maxDelta)
    {
        if (!bot || !bot->IsInWorld())
            return false;

        float x = bot->GetPositionX();
        float y = bot->GetPositionY();
        float z = bot->GetPositionZ();
        float groundZ = z;
        bot->UpdateAllowedPositionZ(x, y, groundZ);
        return std::fabs(z - groundZ) > maxDelta;
    }

    bool MoveToFollowSlot(Player* bot, Unit* leader, float distance, float angle)
    {
        if (!CanMove(bot) || !leader)
            return false;

        float x, y, z;
        if (!ComputeFollowPoint(leader, bot, distance, angle, x, y, z))
            return false;

        return MovePoint(bot, x, y, z);
    }

    bool MoveFollowLeader(Player* bot, Unit* leader, float distance, float angle)
    {
        if (!CanMove(bot) || !leader)
            return false;
        bot->GetMotionMaster()->Clear();
        bot->GetMotionMaster()->MoveFollow(leader, distance, angle);
        return true;
    }

    bool MoveChase(Player* bot, Unit* target, float distance)
    {
        if (!CanMove(bot) || !target || !target->IsAlive())
            return false;
        bot->GetMotionMaster()->Clear();
        bot->GetMotionMaster()->MoveChase(target, distance);
        return true;
    }

    bool MovePoint(Player* bot, float x, float y, float z)
    {
        if (!CanMove(bot))
            return false;
        bot->GetMotionMaster()->Clear();
        bot->GetMotionMaster()->MovePoint(0, x, y, z);
        return true;
    }

    void FaceOrientation(Player* bot, float orientation)
    {
        if (!bot || IsCasting(bot))
            return;
        // Server-side only — avoids MoveSpline that fights Follow motion.
        bot->SetFacingTo(orientation);
    }

    void FaceUnit(Player* bot, Unit* target)
    {
        if (!bot || !target || IsCasting(bot))
            return;
        bot->SetFacingToObject(target);
    }

    void ClearDeadSelection(Player* bot)
    {
        if (!bot)
            return;
        if (Unit* victim = bot->GetVictim())
        {
            if (!victim->IsAlive())
                bot->AttackStop();
        }
    }
}
