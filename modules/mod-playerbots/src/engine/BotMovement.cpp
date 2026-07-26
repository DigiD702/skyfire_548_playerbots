/*
 * Cast-safe movement helpers.
 */

#include "BotMovement.h"

#include "MotionMaster.h"
#include "MoveSpline.h"
#include "Object.h"
#include "PathGenerator.h"
#include "Player.h"
#include "SpellAuraDefines.h"
#include "Unit.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BotMovement
{
bool IsCasting(Player* bot)
{
    // Skip Auto Shot / wand autorepeat — those are background fire, not a cast.
    // Include pending instants (skipInstant=false). Do not treat bare CASTING
    // state alone — that stuck true and blocked facing / move-flag cleanup.
    return bot && bot->IsNonMeleeSpellCasted(false, false, true, false, false);
}

    bool CanMove(Player* bot)
    {
        if (!bot || !bot->IsAlive() || IsCasting(bot))
            return false;
        // Moving while seated cancels drink/food auras — rest must finish first.
        if (bot->IsSitState())
            return false;
        return true;
    }

    // Point/follow Launch() copies walkmode from MOVEMENTFLAG_WALKING. A stuck
    // walk flag makes every MovePoint crawl; strip it before issuing motion.
    void EnsureRunning(Player* bot)
    {
        if (!bot)
            return;
        if (bot->IsWalking())
            bot->SetWalk(false);
    }

    bool IsAirborne(Unit const* unit)
    {
        if (!unit)
            return false;
        if (unit->IsFlying())
            return true;
        if (unit->HasUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING))
            return true;
        if (unit->HasAuraType(SPELL_AURA_FLY)
            || unit->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED))
            return true;
        return false;
    }

    bool StopAndIdle(Player* bot)
    {
        if (!CanMove(bot))
            return false;

        // Stop spline / clear UNIT_STATE_MOVING while still animating.
        if (!bot->IsStopped() || (bot->movespline && !bot->movespline->Finalized()))
            bot->StopMoving();

        // Spell::prepare rejects cast-time spells when isMoving() — that checks
        // MOVEMENTFLAG_MASK_MOVING, not UNIT_STATE_MOVING. Point/facing Launch()
        // sets MOVEMENTFLAG_FORWARD; once the spline finalizes, StopMoving
        // early-outs and leaves the flag stuck. Casters then only land instants
        // (DoTs) and look like a broken rotation.
        bot->RemoveUnitMovementFlag(MOVEMENTFLAG_MASK_MOVING);
        EnsureRunning(bot);

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
        float& x, float& y, float& z, bool airborne)
    {
        if (!leader || !bot)
            return false;

        // Same convention as FollowMovementGenerator / GetClosePoint:
        // absolute angle = leader orientation + relative angle.
        float const size = bot->GetObjectSize();
        leader->GetClosePoint(x, y, z, size, distance, angle);

        if (airborne)
        {
            // Stay at the leader's altitude — never snap the slot to ground mesh.
            z = leader->GetPositionZ();
        }
        else
        {
            leader->UpdateAllowedPositionZ(x, y, z);
            bot->UpdateAllowedPositionZ(x, y, z);
        }
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    bool IsBadlyOffGround(Player* bot, float maxDelta)
    {
        if (!bot || !bot->IsInWorld())
            return false;
        if (IsAirborne(bot))
            return false;

        float x = bot->GetPositionX();
        float y = bot->GetPositionY();
        float z = bot->GetPositionZ();
        float groundZ = z;
        bot->UpdateAllowedPositionZ(x, y, groundZ);
        return std::fabs(z - groundZ) > maxDelta;
    }

    bool MoveToFollowSlot(Player* bot, Unit* leader, float distance, float angle, bool airborne)
    {
        if (!CanMove(bot) || !leader)
            return false;

        float x, y, z;
        if (!ComputeFollowPoint(leader, bot, distance, angle, x, y, z, airborne))
            return false;

        // Air: no navmesh path (that clamps Z to ground and "sucks" flyers down).
        return MovePoint(bot, x, y, z, !airborne);
    }

    bool MoveFollowLeader(Player* bot, Unit* leader, float distance, float angle)
    {
        if (!CanMove(bot) || !leader)
            return false;
        EnsureRunning(bot);
        bot->GetMotionMaster()->Clear();
        bot->GetMotionMaster()->MoveFollow(leader, distance, angle);
        return true;
    }

    bool MoveChase(Player* bot, Unit* target, float distance)
    {
        if (!CanMove(bot) || !target || !target->IsAlive())
            return false;
        EnsureRunning(bot);
        bot->GetMotionMaster()->Clear();
        bot->GetMotionMaster()->MoveChase(target, distance);
        return true;
    }

    bool MovePoint(Player* bot, float x, float y, float z, bool generatePath)
    {
        if (!CanMove(bot))
            return false;
        EnsureRunning(bot);
        bot->GetMotionMaster()->Clear();
        bot->GetMotionMaster()->MovePoint(0, x, y, z, generatePath);
        return true;
    }

    bool MoveTo(Player* bot, float x, float y, float z)
    {
        if (!CanMove(bot))
            return false;
        EnsureRunning(bot);

        bot->UpdateAllowedPositionZ(x, y, z);
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return false;

        PathGenerator path(bot);
        bool const ok = path.CalculatePath(x, y, z);
        PathType const type = path.GetPathType();
        if (!ok || (type & PATHFIND_NOPATH) || (type & PATHFIND_BLANK))
            return false;

        // Prefer the navmesh's actual end when the path is incomplete/short.
        G3D::Vector3 const& end = path.GetActualEndPosition();
        float destX = end.x;
        float destY = end.y;
        float destZ = end.z;
        bot->UpdateAllowedPositionZ(destX, destY, destZ);

        bot->GetMotionMaster()->Clear();
        bot->GetMotionMaster()->MovePoint(1, destX, destY, destZ, true);
        return true;
    }

    void FaceOrientation(Player* bot, float orientation)
    {
        if (!bot || IsCasting(bot))
            return;
        // Orientation only — SetFacingTo launches a MoveSpline that sets
        // MOVEMENTFLAG_FORWARD and breaks cast-time spells.
        bot->SetOrientation(orientation);
    }

    void FaceUnit(Player* bot, Unit* target)
    {
        if (!bot || !target || IsCasting(bot))
            return;
        // SetInFront = orientation only. SetFacingToObject Launch() marks the
        // bot as moving and Spell::prepare then fails every cast-time spell.
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
