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

    // Stop + idle only if not casting. Returns false if blocked by cast.
    bool StopAndIdle(Player* bot);

    // Clear MotionMaster only if not casting.
    bool ClearMotion(Player* bot);

    // MoveFollow behind leader with GUID spread. No-ops while casting.
    bool MoveFollowLeader(Player* bot, Unit* leader, float distance, float angle);

    // Chase target for melee. No-ops while casting.
    bool MoveChase(Player* bot, Unit* target, float distance = 0.0f);

    // Walk to a point (ranged plant / loot). No-ops while casting.
    bool MovePoint(Player* bot, float x, float y, float z);

    // Face without launching a spline if possible; never while casting.
    void FaceOrientation(Player* bot, float orientation);
    void FaceUnit(Player* bot, Unit* target);

    // Clear dead selection so bots don't stare at corpses.
    void ClearDeadSelection(Player* bot);
}

#endif
