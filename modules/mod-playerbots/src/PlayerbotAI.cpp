/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "PlayerbotAI.h"
#include "Group.h"
#include "GroupMgr.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PetDefines.h"
#include "Player.h"
#include "SharedDefines.h"
#include "WorldSession.h"

namespace
{
    // How often (ms) the bot re-evaluates its behaviour. Kept coarse to keep the
    // per-tick cost of large bot populations low.
    constexpr uint32 BOT_AI_UPDATE_INTERVAL = 500;

    // Follow a little further out than pets so a party of bots doesn't stack on
    // the leader.
    constexpr float BOT_FOLLOW_DIST = 2.0f;
    constexpr float TWO_PI = 6.2831853f;

    // Beyond this distance (same map) the follow generator can't realistically
    // catch up, so the bot teleports to the leader instead.
    constexpr float BOT_TELEPORT_DIST = 100.0f;

    // Distance ranged/caster bots try to hold from their target.
    constexpr float BOT_CAST_DIST = 25.0f;
}

PlayerbotAI::PlayerbotAI(Player* bot) : _bot(bot), _updateTimer(0), _chaseGuid(0), _followGuid(0)
{
}

void PlayerbotAI::UpdateAI(uint32 diff)
{
    if (!_bot || !_bot->IsInWorld())
        return;

    _updateTimer += diff;
    if (_updateTimer < BOT_AI_UPDATE_INTERVAL)
        return;
    _updateTimer = 0;

    HandlePendingInvites();

    if (!_bot->IsAlive())
        return; // TODO: corpse release / resurrection handling

    if (HandleCombat())
        return; // engaged: combat drives movement

    HandleFollow();
}

// Auto-accept party/raid invitations so bots can be pulled into groups (and thus
// LFG/LFR/RBG queues). Mirrors the accept branch of HandleGroupAcceptOpcode.
void PlayerbotAI::HandlePendingInvites()
{
    Group* group = _bot->GetGroupInvite();
    if (!group)
        return;

    group->RemoveInvite(_bot);

    if (group->GetLeaderGUID() == _bot->GetGUID())
        return;

    if (group->IsFull())
        return;

    Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());

    if (!group->IsCreated())
    {
        if (!leader)
        {
            group->RemoveAllInvites();
            return;
        }

        group->RemoveInvite(leader);
        group->Create(leader);
        sGroupMgr->AddGroup(group);
    }

    if (!group->AddMember(_bot))
        return;

    group->BroadcastGroupUpdate();
}

// Combat: acquire a target (own attacker, or assist the group leader), position
// for the class (melee closes in, ranged holds at distance), and run a basic
// class rotation on top of auto-attack.
bool PlayerbotAI::HandleCombat()
{
    Unit* target = SelectTarget();
    if (!target)
    {
        // Nothing to fight: drop any lingering attack/chase so we can follow.
        if (_bot->GetVictim())
            _bot->AttackStop();
        _chaseGuid = 0;
        return false;
    }

    uint32 spellId = GetFillerSpell();
    bool hasFiller = spellId && _bot->HasSpell(spellId);

    // Ranged/caster classes hold at distance to cast; everyone else (and any bot
    // without a usable filler) closes to melee so it always does *something*.
    bool holdAtRange = IsRangedClass() && hasFiller;

    _bot->Attack(target, true);

    float chaseDist = holdAtRange ? BOT_CAST_DIST : 0.0f;
    if (_chaseGuid != target->GetGUID() ||
        _bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE)
    {
        _bot->GetMotionMaster()->MoveChase(target, chaseDist);
        _chaseGuid = target->GetGUID();
        _followGuid = 0;
    }

    // Cast the filler when we have one; the core validates range/LoS, so melee
    // strikes (e.g. Crusader/Sinister Strike) only fire once we're in range.
    if (hasFiller)
        DoRotation(target, spellId);

    return true;
}

// Target priority: a unit already attacking us, otherwise assist the group
// leader's target so the whole party focus-fires the same mob.
Unit* PlayerbotAI::SelectTarget()
{
    Unit* victim = _bot->GetVictim();
    if (victim && victim->IsAlive() && _bot->IsValidAttackTarget(victim))
        return victim;

    for (Unit* attacker : _bot->getAttackers())
        if (attacker && attacker->IsAlive() && _bot->IsValidAttackTarget(attacker))
            return attacker;

    // Assist the group leader: prefer whoever the leader is fighting, otherwise
    // whatever they currently have selected (so "target a mob" pulls the bots in
    // even before the leader lands a hit).
    Group* group = _bot->GetGroup();
    uint64 leaderGuid = group ? group->GetLeaderGUID() : 0;
    if (leaderGuid && leaderGuid != _bot->GetGUID())
    {
        if (Player* leader = ObjectAccessor::FindPlayer(leaderGuid))
        {
            Unit* assist = leader->GetVictim();
            if (!assist)
                assist = leader->GetSelectedUnit();

            if (assist && assist->IsAlive() &&
                _bot->IsValidAttackTarget(assist) &&
                leader->IsInCombat() &&
                _bot->IsWithinDistInMap(assist, 60.0f))
                return assist;
        }
    }

    return nullptr;
}

bool PlayerbotAI::IsRangedClass() const
{
    switch (_bot->getClass())
    {
        case CLASS_HUNTER:
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return true;
        default:
            return false;
    }
}

// One iconic, low-level "filler" attack per class. Conservative on purpose: bots
// only cast what they actually know, so an unknown/wrong id simply no-ops.
// Per-spec rotations are layered on later.
uint32 PlayerbotAI::GetFillerSpell() const
{
    switch (_bot->getClass())
    {
        case CLASS_PALADIN: return 35395; // Crusader Strike
        case CLASS_HUNTER:  return 3044;  // Arcane Shot
        case CLASS_ROGUE:   return 1752;  // Sinister Strike
        case CLASS_PRIEST:  return 585;   // Smite
        case CLASS_MAGE:    return 116;   // Frostbolt
        case CLASS_WARLOCK: return 686;   // Shadow Bolt
        default:            return 0;     // melee-only for now
    }
}

void PlayerbotAI::DoRotation(Unit* target, uint32 spellId)
{
    if (!spellId)
        return;

    if (_bot->HasUnitState(UNIT_STATE_CASTING))
        return; // don't interrupt our own cast

    if (_bot->HasSpellCooldown(spellId))
        return;

    // triggered=false: normal cast, so GCD/power/range/LoS are all validated by
    // the core and a bad cast just fails cleanly.
    _bot->CastSpell(target, spellId, false);
}

// Out of combat, keep formation on the group leader. If the leader is on
// another map, or too far to catch up on foot, the bot teleports to them.
void PlayerbotAI::HandleFollow()
{
    Group* group = _bot->GetGroup();
    uint64 leaderGuid = group ? group->GetLeaderGUID() : 0;

    Player* leader = nullptr;
    if (leaderGuid && leaderGuid != _bot->GetGUID())
        leader = ObjectAccessor::FindPlayer(leaderGuid);

    if (!leader || !leader->IsInWorld() || !leader->IsAlive())
    {
        if (_followGuid)
        {
            _bot->GetMotionMaster()->Clear();
            _bot->GetMotionMaster()->MoveIdle();
            _followGuid = 0;
        }
        return;
    }

    // Warp to the leader when we can't reasonably run there (different map or
    // very far). Following resumes automatically once we arrive on the map.
    if (leader->GetMap() != _bot->GetMap() || _bot->GetDistance(leader) > BOT_TELEPORT_DIST)
    {
        TeleportToLeader(leader);
        return;
    }

    if (_followGuid != leaderGuid ||
        _bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
    {
        // Spread bots around the leader by deriving a stable angle from the GUID.
        float angle = float(_bot->GetGUIDLow() % 16) / 16.0f * TWO_PI;
        _bot->GetMotionMaster()->MoveFollow(leader, BOT_FOLLOW_DIST, angle);
        _followGuid = leaderGuid;
        _chaseGuid = 0;
    }
}

void PlayerbotAI::TeleportToLeader(Player* leader)
{
    _bot->GetMotionMaster()->Clear();
    _bot->GetMotionMaster()->MoveIdle();

    if (_bot->TeleportTo(leader->GetMapId(), leader->GetPositionX(), leader->GetPositionY(),
        leader->GetPositionZ(), leader->GetOrientation()))
    {
        // Bots have no client to ack the teleport; finalize it immediately.
        _bot->GetSession()->FinalizeBotTeleport();
    }

    // Force MoveFollow to be re-issued once the teleport completes.
    _followGuid = 0;
    _chaseGuid = 0;
}
