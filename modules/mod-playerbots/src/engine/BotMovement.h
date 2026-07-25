/*
 * Cast-safe movement helpers (harden vs AC MovementActions pitfalls).
 * Never Clear / StopMoving / SetFacing while a cast is in progress.
 */

#ifndef SF_BOT_MOVEMENT_H
#define SF_BOT_MOVEMENT_H

#include "Define.h"

class Player;
class Unit;

namespace BotMovement
{
    bool IsCasting(Player* bot);
    bool CanMove(Player* bot);

    // True when unit can/should move in 3D (flying mount, levitate, etc.).
    bool IsAirborne(Unit const* unit);

    // Stop + idle only if not casting. Returns false if blocked by cast.
    bool StopAndIdle(Player* bot);

    // Clear MotionMaster only if not casting.
    bool ClearMotion(Player* bot);

    // Compute leader-relative follow slot. When airborne, keep leader Z (no ground clamp).
    bool ComputeFollowPoint(Unit* leader, Player* bot, float distance, float angle,
        float& x, float& y, float& z, bool airborne = false);

    // True when bot Z is badly off the ground under its feet (clipped mesh).
    // Always false while airborne — flying is not "clipped".
    bool IsBadlyOffGround(Player* bot, float maxDelta = 3.0f);

    // Move to validated follow slot (MovePoint). Prefer over raw MoveFollow near
    // geometry so bots do not idle inside walls/stairs.
    bool MoveToFollowSlot(Player* bot, Unit* leader, float distance, float angle,
        bool airborne = false);

    // MoveFollow behind leader. No-ops while casting.
    bool MoveFollowLeader(Player* bot, Unit* leader, float distance, float angle);

    // Chase target for melee. No-ops while casting.
    bool MoveChase(Player* bot, Unit* target, float distance = 0.0f);

    // Walk to a point (ranged plant / loot). No-ops while casting.
    // generatePath=false for air moves so Z is not navmesh-clamped to ground.
    bool MovePoint(Player* bot, float x, float y, float z, bool generatePath = true);

    // Path-aware travel: clamp Z, reject NOPATH, MovePoint with generatePath.
    bool MoveTo(Player* bot, float x, float y, float z);

    // Face via orientation only (no MoveSpline / MOVEMENTFLAG_FORWARD).
    // Never while casting.
    void FaceOrientation(Player* bot, float orientation);
    void FaceUnit(Player* bot, Unit* target);

    // Clear dead selection so bots don't stare at corpses.
    void ClearDeadSelection(Player* bot);
}

#endif
