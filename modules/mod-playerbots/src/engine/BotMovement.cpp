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
        bot->SetOrientation(orientation);
    }

    void FaceUnit(Player* bot, Unit* target)
    {
        if (!bot || !target || IsCasting(bot))
            return;
        if (!bot->HasInArc(static_cast<float>(M_PI), target))
            bot->SetInFront(target);
    }

    void ClearDeadSelection(Player* bot)
    {
        if (!bot)
            return;
        if (Unit* selected = bot->GetSelectedUnit())
            if (!selected->IsAlive())
                bot->SetSelection(0);
    }
}
