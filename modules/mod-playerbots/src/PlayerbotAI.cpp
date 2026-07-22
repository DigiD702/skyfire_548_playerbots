/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "PlayerbotAI.h"
#include "PlayerbotMgr.h"
#include "CellImpl.h"
#include "Creature.h"
#include "DBCStores.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "GroupMgr.h"
#include "GroupReference.h"
#include "Item.h"
#include "LFGMgr.h"
#include "LootMgr.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "PetDefines.h"
#include "Player.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <list>
#include <sstream>
#include <string>
#include <vector>

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

    // How far a bot will walk to loot a corpse or reach a repairer.
    constexpr float BOT_LOOT_SEEK_DIST = 30.0f;
    constexpr float BOT_REPAIR_SEEK_DIST = 20.0f;

    // Solo idle wander: radius around the bot and pause between picks.
    constexpr float BOT_WANDER_RADIUS = 18.0f;
    constexpr uint32 BOT_WANDER_PAUSE_MIN = 4000;
    constexpr uint32 BOT_WANDER_PAUSE_MAX = 10000;

    // Repair when any equipped item is below this fraction of max durability.
    constexpr float BOT_REPAIR_THRESHOLD = 0.50f;

    // Matches AELootCreatureCheck / isAllowedToLoot for bot corpse scavenging.
    struct BotLootCreatureCheck
    {
        BotLootCreatureCheck(Player* looter, float range) : _looter(looter), _range(range) { }

        bool operator()(Creature* creature) const
        {
            if (!creature || creature->IsAlive())
                return false;
            if (!_looter->IsWithinDist(creature, _range))
                return false;
            return _looter->isAllowedToLoot(creature);
        }

        Player* _looter;
        float _range;
    };

    struct BotRepairerCheck
    {
        BotRepairerCheck(Player* bot, float range) : _bot(bot), _range(range) { }

        bool operator()(Creature* creature) const
        {
            if (!creature || !creature->IsAlive() || creature->IsInCombat())
                return false;
            if (!_bot->IsWithinDist(creature, _range))
                return false;
            return creature->HasFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_REPAIR);
        }

        Player* _bot;
        float _range;
    };
}

PlayerbotAI::PlayerbotAI(Player* bot, bool clientControlled)
    : _bot(bot), _clientControlled(clientControlled), _updateTimer(0), _chaseGuid(0),
      _followGuid(0), _lootGuid(0), _wanderTimer(0), _stay(false), _passive(false),
      _grind(false), _forcedTargetGuid(0), _lfgRoleResponded(false), _lfgProposalResponded(false)
{
}

namespace
{
    bool CanTankClass(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT:
            case CLASS_DRUID:
            case CLASS_MONK:
                return true;
            default:
                return false;
        }
    }

    bool CanHealClass(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_PALADIN:
            case CLASS_PRIEST:
            case CLASS_SHAMAN:
            case CLASS_DRUID:
            case CLASS_MONK:
                return true;
            default:
                return false;
        }
    }
}

void PlayerbotAI::UpdateAI(uint32 diff)
{
    if (!_bot || !_bot->IsInWorld())
        return;

    _updateTimer += diff;
    if (_updateTimer < BOT_AI_UPDATE_INTERVAL)
        return;

    // Wander pauses are tracked in real time so they don't depend on the AI
    // throttle; clamp so a long hitch doesn't skip forever.
    if (_wanderTimer > BOT_AI_UPDATE_INTERVAL)
        _wanderTimer -= BOT_AI_UPDATE_INTERVAL;
    else
        _wanderTimer = 0;

    _updateTimer = 0;

    // Self-bot: client owns movement. Only cast in combat (and light invites/LFG).
    if (_clientControlled)
    {
        HandlePendingInvites();
        HandleLfg();
        if (_bot->IsAlive())
            HandleCombatCastOnly();
        return;
    }

    HandlePendingInvites();
    HandleLfg();
    HandleInteractions();

    if (!_bot->IsAlive())
        return; // TODO: corpse release / resurrection handling

    if (HandleCombat())
        return; // engaged: combat drives movement

    if (_stay)
    {
        HandleStay();
        HandleVendor();
        return;
    }

    if (HandleLoot())
        return; // walking to / looting a corpse

    // Grouped bots stick with the leader; solo bots wander the local area so
    // the world feels populated instead of frozen statues.
    Group* group = _bot->GetGroup();
    uint64 leaderGuid = group ? group->GetLeaderGUID() : 0;
    if (leaderGuid && leaderGuid != _bot->GetGUID())
        HandleFollow();
    else
        HandleWander();

    HandleVendor();
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

// Auto-respond to the dungeon finder so a master can queue a party of bots: the
// bot answers the group role check and accepts the join proposal. The rest of
// the LFG flow (teleport in/out, boot votes, etc.) is handled by the core.
void PlayerbotAI::HandleLfg()
{
    Group* grp = _bot->GetGroup();
    if (!grp)
    {
        _lfgRoleResponded = false;
        _lfgProposalResponded = false;
        return;
    }

    uint64 guid = _bot->GetGUID();
    lfg::LfgState state = sLFGMgr->GetState(guid);

    if (state == lfg::LFG_STATE_ROLECHECK)
    {
        if (!_lfgRoleResponded)
        {
            sLFGMgr->UpdateRoleCheck(grp->GetGUID(), guid, ComputeLfgRole());
            _lfgRoleResponded = true;
        }
    }
    else
        _lfgRoleResponded = false;

    if (state == lfg::LFG_STATE_PROPOSAL)
    {
        if (!_lfgProposalResponded)
        {
            if (uint32 proposalId = sLFGMgr->GetActiveProposalIdForPlayer(guid))
            {
                sLFGMgr->UpdateProposal(proposalId, guid, true);
                _lfgProposalResponded = true;
            }
        }
    }
    else
        _lfgProposalResponded = false;
}

// Pick a role for the role check. All bots in the group run the same
// deterministic assignment (by GUID order), so without communicating they still
// produce a valid tank/healer/dps spread: first tank-capable bot tanks, first
// healer-capable bot heals, the rest are damage.
uint8 PlayerbotAI::ComputeLfgRole()
{
    Group* grp = _bot->GetGroup();
    if (!grp)
        return lfg::PLAYER_ROLE_DAMAGE;

    std::vector<uint64> botGuids;
    for (GroupReference* itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
        if (Player* m = itr->GetSource())
            if (m->GetSession() && m->GetSession()->IsBot())
                botGuids.push_back(m->GetGUID());

    std::sort(botGuids.begin(), botGuids.end());

    bool tankTaken = false;
    bool healTaken = false;
    for (uint64 g : botGuids)
    {
        Player* m = ObjectAccessor::FindPlayer(g);
        uint8 cls = m ? m->getClass() : 0;

        uint8 role = lfg::PLAYER_ROLE_DAMAGE;
        if (!tankTaken && CanTankClass(cls))
        {
            role = lfg::PLAYER_ROLE_TANK;
            tankTaken = true;
        }
        else if (!healTaken && CanHealClass(cls))
        {
            role = lfg::PLAYER_ROLE_HEALER;
            healTaken = true;
        }

        if (g == _bot->GetGUID())
            return role;
    }

    return lfg::PLAYER_ROLE_DAMAGE;
}

// Combat: acquire a target (own attacker, or assist the group leader), position
// for the class (melee closes in, ranged holds at distance), and run a basic
// class rotation on top of auto-attack.
bool PlayerbotAI::HandleCombat()
{
    Unit* target = SelectTarget();
    if (!target)
    {
        // Forced target died or became invalid - drop the order.
        if (_forcedTargetGuid)
            ClearForcedTarget();

        // Nothing to fight: drop any lingering attack/chase so we can follow.
        if (_bot->GetVictim())
            _bot->AttackStop();
        _chaseGuid = 0;
        return false;
    }

    uint32 spellId = GetFillerSpell();
    bool hasFiller = spellId && _bot->HasSpell(spellId);

    // Class decides stance, not whether the filler is known. A mage without its
    // filler must still stay at range - otherwise MoveChase(0) + _reachTarget
    // forces fist-fighting in melee.
    // Cloth casters/hunters stay ranged; tanks always melee. Hybrid healers
    // (resto shaman/monk/druid/pala) melee for now; priests are ranged.
    bool const ranged = IsRangedClass() && GetCombatRole() != CombatRole::Tank;
    bool const inMelee = _bot->IsWithinMeleeRange(target);

    _followGuid = 0;
    _lootGuid = 0;

    if (!ranged)
    {
        _bot->Attack(target, true);

        if (!inMelee)
        {
            MovementGeneratorType moveType = _bot->GetMotionMaster()->GetCurrentMovementGeneratorType();
            bool reissue = _chaseGuid != target->GetGUID()
                || moveType != CHASE_MOTION_TYPE
                || _bot->IsStopped();
            if (reissue)
            {
                _bot->GetMotionMaster()->Clear();
                _bot->GetMotionMaster()->MoveChase(target, 0.0f);
                _chaseGuid = target->GetGUID();
            }
        }
        else
            _chaseGuid = target->GetGUID();

        if (hasFiller && inMelee)
            DoRotation(target, spellId);

        return true;
    }

    // Ranged / caster: never chase to contact. Walk to a cast-range point on the
    // bot's side of the target, then plant and cast when in range with LoS.
    float const dist = _bot->GetDistance(target);
    bool const inRange = dist <= BOT_CAST_DIST;
    bool const hasLos = _bot->IsWithinLOSInMap(target);

    // Ensure we are not in a melee-attack state (chase _reachTarget can force it).
    if (_bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING))
    {
        _bot->ClearUnitState(UNIT_STATE_MELEE_ATTACKING);
        _bot->SendMeleeAttackStop(target);
    }
    _bot->Attack(target, false);

    if (inRange && hasLos)
    {
        if (_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != IDLE_MOTION_TYPE)
        {
            _bot->GetMotionMaster()->Clear();
            _bot->GetMotionMaster()->MoveIdle();
        }
        if (!_bot->HasInArc(static_cast<float>(M_PI) / 3.0f, target))
            _bot->SetInFront(target);

        _chaseGuid = target->GetGUID();
        if (hasFiller)
            DoRotation(target, spellId);
        return true;
    }

    // Too far, or in range but LoS blocked: move toward a point at cast range on
    // our current side of the target (not behind it, not into melee).
    {
        float destX, destY, destZ;
        float const standDist = BOT_CAST_DIST * 0.85f;
        // Absolute angle from target toward the bot - keeps them on their side.
        float const absAngle = target->GetAngle(_bot);
        target->GetNearPoint(_bot, destX, destY, destZ, _bot->GetObjectSize(), standDist, absAngle);
        _bot->UpdateAllowedPositionZ(destX, destY, destZ);

        MovementGeneratorType moveType = _bot->GetMotionMaster()->GetCurrentMovementGeneratorType();
        bool reissue = _chaseGuid != target->GetGUID()
            || moveType != POINT_MOTION_TYPE
            || _bot->IsStopped();
        if (reissue)
        {
            MoveToPosition(destX, destY, destZ);
            _chaseGuid = target->GetGUID();
        }
    }

    return true;
}

// Self-bot combat: never touch MotionMaster. Cast when the player is already in
// range with LoS; they steer into position themselves.
bool PlayerbotAI::HandleCombatCastOnly()
{
    Unit* target = SelectTarget();
    if (!target)
    {
        if (_forcedTargetGuid)
            ClearForcedTarget();
        return false;
    }

    uint32 spellId = GetFillerSpell();
    if (!spellId || !_bot->HasSpell(spellId))
        return true;

    bool const ranged = IsRangedClass() && GetCombatRole() != CombatRole::Tank;
    if (ranged)
    {
        if (_bot->GetDistance(target) > BOT_CAST_DIST || !_bot->IsWithinLOSInMap(target))
            return true;
        if (_bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING))
        {
            _bot->ClearUnitState(UNIT_STATE_MELEE_ATTACKING);
            _bot->SendMeleeAttackStop(target);
        }
        _bot->Attack(target, false);
        DoRotation(target, spellId);
    }
    else
    {
        if (!_bot->IsWithinMeleeRange(target))
            return true;
        _bot->Attack(target, true);
        DoRotation(target, spellId);
    }
    return true;
}

// Target priority: a forced "attack" target, then a unit already attacking us,
// otherwise (when not passive) assist the group leader's target.
Unit* PlayerbotAI::SelectTarget()
{
    if (Unit* forced = GetForcedTarget())
        return forced;

    Unit* victim = _bot->GetVictim();
    if (victim && victim->IsAlive() && _bot->IsValidAttackTarget(victim))
        return victim;

    for (Unit* attacker : _bot->getAttackers())
        if (attacker && attacker->IsAlive() && _bot->IsValidAttackTarget(attacker))
            return attacker;

    // Passiveive bots only fight back; they do not assist or pull.
    if (_passive)
        return nullptr;

    if (_grind)
        if (Unit* grind = SelectGrindTarget())
            return grind;

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

    // Self-bot with no forced target: attack whatever the player has selected
    // while in combat, or any attacker already handled above.
    if (_clientControlled)
    {
        if (Unit* selected = _bot->GetSelectedUnit())
            if (selected->IsAlive() && _bot->IsValidAttackTarget(selected))
                return selected;
    }

    return nullptr;
}

Unit* PlayerbotAI::SelectGrindTarget()
{
    Unit* target = nullptr;
    Skyfire::NearestAttackableUnitInObjectRangeCheck check(_bot, _bot, BOT_LOOT_SEEK_DIST);
    Skyfire::UnitLastSearcher<Skyfire::NearestAttackableUnitInObjectRangeCheck> searcher(_bot, target, check);
    _bot->VisitNearbyObject(BOT_LOOT_SEEK_DIST, searcher);
    if (target && _bot->IsValidAttackTarget(target))
        return target;
    return nullptr;
}

Unit* PlayerbotAI::GetForcedTarget() const
{
    if (!_forcedTargetGuid)
        return nullptr;

    Unit* target = ObjectAccessor::GetUnit(*_bot, _forcedTargetGuid);
    if (!target || !target->IsAlive() || !_bot->IsValidAttackTarget(target))
        return nullptr;
    return target;
}

void PlayerbotAI::SetForcedTarget(Unit* target)
{
    _forcedTargetGuid = target ? target->GetGUID() : 0;
    _chaseGuid = 0;
}

void PlayerbotAI::ClearForcedTarget()
{
    _forcedTargetGuid = 0;
}

PlayerbotAI::CombatRole PlayerbotAI::GetCombatRole() const
{
    uint32 specId = _bot->GetTalentSpecialization(_bot->GetActiveSpec());
    uint8 cls = _bot->getClass();

    // Match the same role-to-spec mapping used at character create / init.
    auto isSpec = [&](uint8 tab) -> bool
    {
        uint32 const* specs = GetClassSpecializations(cls);
        return specs && specs[tab] == specId;
    };

    switch (cls)
    {
        case CLASS_WARRIOR:      return isSpec(2) ? CombatRole::Tank : CombatRole::Damage;
        case CLASS_PALADIN:      return isSpec(1) ? CombatRole::Tank : (isSpec(0) ? CombatRole::Healer : CombatRole::Damage);
        case CLASS_DEATH_KNIGHT: return isSpec(0) ? CombatRole::Tank : CombatRole::Damage;
        case CLASS_PRIEST:       return isSpec(2) ? CombatRole::Damage : CombatRole::Healer;
        case CLASS_SHAMAN:       return isSpec(2) ? CombatRole::Healer : CombatRole::Damage;
        case CLASS_MONK:         return isSpec(0) ? CombatRole::Tank : (isSpec(1) ? CombatRole::Healer : CombatRole::Damage);
        case CLASS_DRUID:        return isSpec(2) ? CombatRole::Tank : (isSpec(3) ? CombatRole::Healer : CombatRole::Damage);
        default:                 return CombatRole::Damage;
    }
}

bool PlayerbotAI::MatchesRoleFilter(std::string const& filter) const
{
    if (filter == "tank")
        return GetCombatRole() == CombatRole::Tank;
    if (filter == "heal" || filter == "healer")
        return GetCombatRole() == CombatRole::Healer;
    if (filter == "dps" || filter == "damage")
        return GetCombatRole() == CombatRole::Damage;
    if (filter == "ranged")
        return IsRangedClass() && GetCombatRole() != CombatRole::Tank;
    return false;
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

// Auto-accept trades and duels. Trades use the real accept opcode path so a
// pending mutual accept completes properly; duels mirror HandleDuelResponseOpcode.
void PlayerbotAI::HandleInteractions()
{
    // Trade: accept once the window is open. If the other side already accepted,
    // HandleAcceptTradeOpcode finishes the exchange; otherwise it just flags us
    // accepted so their next Accept completes it. Re-runs after item/money changes
    // because those clear the accepted flag on both sides.
    if (TradeData* trade = _bot->GetTradeData())
    {
        if (!trade->IsAccepted())
        {
            WorldPacket data(CMSG_ACCEPT_TRADE);
            _bot->GetSession()->HandleAcceptTradeOpcode(data);
        }
    }

    // Duel: accept a pending challenge (startTimer still 0) from someone else.
    if (_bot->duel && _bot->duel->startTime == 0 && _bot->duel->startTimer == 0 &&
        _bot->duel->initiator && _bot->duel->initiator != _bot &&
        _bot->duel->opponent)
    {
        time_t now = time(nullptr);
        _bot->duel->startTimer = now;
        _bot->duel->opponent->duel->startTimer = now;
        _bot->SendDuelCountdown(3000);
        _bot->duel->opponent->SendDuelCountdown(3000);
    }
}

// Walk to and loot nearby corpses the bot is allowed to take from. Returns true
// while the bot is busy with loot so follow/wander don't yank it away mid-run.
bool PlayerbotAI::HandleLoot()
{
    // Still pathing toward a corpse we already picked.
    if (_lootGuid)
    {
        Creature* corpse = _bot->GetMap()->GetCreature(_lootGuid);
        if (!corpse || corpse->IsAlive() || !_bot->isAllowedToLoot(corpse))
        {
            _lootGuid = 0;
            return false;
        }

        if (!_bot->IsWithinDistInMap(corpse, INTERACTION_DISTANCE))
        {
            if (_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                MoveToPosition(corpse->GetPositionX(), corpse->GetPositionY(), corpse->GetPositionZ());
            return true;
        }

        // In range: open loot, take everything (incl. money), then release.
        _bot->SendLoot(corpse->GetGUID(), LootType::LOOT_CORPSE);

        Loot* loot = &corpse->loot;
        uint32 maxSlot = loot->GetMaxSlotInLootFor(_bot);
        for (uint32 slot = 0; slot < maxSlot; ++slot)
            _bot->StoreLootItem(uint8(slot), loot, corpse->GetGUID());

        if (loot->gold)
        {
            WorldPacket money(CMSG_LOOT_MONEY);
            _bot->GetSession()->HandleLootMoneyOpcode(money);
        }

        _bot->GetSession()->DoLootRelease(corpse->GetGUID());
        _lootGuid = 0;
        _followGuid = 0;
        return false;
    }

    Creature* corpse = FindNearbyLoot();
    if (!corpse)
        return false;

    if (_bot->IsWithinDistInMap(corpse, INTERACTION_DISTANCE))
    {
        _lootGuid = corpse->GetGUID();
        return HandleLoot(); // re-enter the in-range branch above
    }

    _lootGuid = corpse->GetGUID();
    _followGuid = 0;
    _chaseGuid = 0;
    MoveToPosition(corpse->GetPositionX(), corpse->GetPositionY(), corpse->GetPositionZ());
    return true;
}

Creature* PlayerbotAI::FindNearbyLoot()
{
    std::list<Creature*> corpses;
    BotLootCreatureCheck check(_bot, BOT_LOOT_SEEK_DIST);
    Skyfire::CreatureListSearcher<BotLootCreatureCheck> searcher(_bot, corpses, check);
    _bot->VisitNearbyGridObject(BOT_LOOT_SEEK_DIST, searcher);

    Creature* best = nullptr;
    float bestDist = BOT_LOOT_SEEK_DIST + 1.0f;
    for (Creature* c : corpses)
    {
        float d = _bot->GetDistance(c);
        if (d < bestDist)
        {
            bestDist = d;
            best = c;
        }
    }
    return best;
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
        _lootGuid = 0;
    }
}

// Solo bots pick a nearby ground point and walk there, then pause before the
// next pick. Skip while a trade/duel window is open so we don't walk away mid-UI.
void PlayerbotAI::HandleWander()
{
    if (_bot->GetTradeData() || _bot->duel)
        return;

    MovementGeneratorType moveType = _bot->GetMotionMaster()->GetCurrentMovementGeneratorType();
    if (moveType == POINT_MOTION_TYPE || moveType == CHASE_MOTION_TYPE)
        return; // still walking somewhere

    if (_wanderTimer)
        return; // resting between walks

    float angle = float(std::rand() % 360) * (TWO_PI / 360.0f);
    float dist = 4.0f + float(std::rand() % int(BOT_WANDER_RADIUS - 3.0f));
    float x = _bot->GetPositionX() + dist * std::cos(angle);
    float y = _bot->GetPositionY() + dist * std::sin(angle);
    float z = _bot->GetPositionZ();
    _bot->UpdateAllowedPositionZ(x, y, z);

    // Reject obviously bad Z (into the void / ceiling) and try again next tick.
    if (std::fabs(z - _bot->GetPositionZ()) > 12.0f)
    {
        _wanderTimer = 1000;
        return;
    }

    _followGuid = 0;
    MoveToPosition(x, y, z);
    _wanderTimer = BOT_WANDER_PAUSE_MIN + uint32(std::rand() % (BOT_WANDER_PAUSE_MAX - BOT_WANDER_PAUSE_MIN + 1));
}

void PlayerbotAI::HandleStay()
{
    // Hold still unless combat already issued a chase this tick (combat returns
    // before HandleStay). Clear follow so a later "follow" re-issues MoveFollow.
    if (_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != IDLE_MOTION_TYPE)
    {
        _bot->GetMotionMaster()->Clear();
        _bot->GetMotionMaster()->MoveIdle();
    }
    _followGuid = 0;
    _lootGuid = 0;
}

// Opportunistic repair: if gear is worn and a repairer is nearby, walk over and
// fix it (free for bots so a test population doesn't stall on gold).
void PlayerbotAI::HandleVendor()
{
    if (!NeedsRepair())
        return;

    Creature* repairer = FindNearbyRepairer();
    if (!repairer)
        return;

    if (!_bot->IsWithinDistInMap(repairer, INTERACTION_DISTANCE))
    {
        if (_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
        {
            _followGuid = 0;
            MoveToPosition(repairer->GetPositionX(), repairer->GetPositionY(), repairer->GetPositionZ());
        }
        return;
    }

    // Free repair keeps the test population combat-ready without wallet setup.
    _bot->DurabilityRepairAll(false, 1.0f, false);
}

Creature* PlayerbotAI::FindNearbyRepairer()
{
    std::list<Creature*> list;
    BotRepairerCheck check(_bot, BOT_REPAIR_SEEK_DIST);
    Skyfire::CreatureListSearcher<BotRepairerCheck> searcher(_bot, list, check);
    _bot->VisitNearbyGridObject(BOT_REPAIR_SEEK_DIST, searcher);

    Creature* best = nullptr;
    float bestDist = BOT_REPAIR_SEEK_DIST + 1.0f;
    for (Creature* c : list)
    {
        float d = _bot->GetDistance(c);
        if (d < bestDist)
        {
            bestDist = d;
            best = c;
        }
    }
    return best;
}

bool PlayerbotAI::NeedsRepair() const
{
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        uint32 maxDur = item->GetUInt32Value(ITEM_FIELD_MAX_DURABILITY);
        if (!maxDur)
            continue;

        uint32 curDur = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
        if (float(curDur) / float(maxDur) < BOT_REPAIR_THRESHOLD)
            return true;
    }
    return false;
}

void PlayerbotAI::MoveToPosition(float x, float y, float z)
{
    _bot->GetMotionMaster()->Clear();
    _bot->GetMotionMaster()->MovePoint(1, x, y, z, true);
}

void PlayerbotAI::ReplyTo(Player* from, std::string const& text)
{
    if (!from || !_bot)
        return;
    _bot->Whisper(text, Language::LANG_UNIVERSAL, from->GetGUID());
}

bool PlayerbotAI::HandleChatCommand(Player* from, std::string const& text, bool acknowledge)
{
    if (!from || text.empty())
        return false;

    // Normalize: lowercase, collapse whitespace.
    std::string cmd;
    cmd.reserve(text.size());
    bool prevSpace = true;
    for (unsigned char ch : text)
    {
        if (std::isspace(ch))
        {
            if (!prevSpace)
            {
                cmd.push_back(' ');
                prevSpace = true;
            }
            continue;
        }
        cmd.push_back(char(std::tolower(ch)));
        prevSpace = false;
    }
    while (!cmd.empty() && cmd.back() == ' ')
        cmd.pop_back();

    if (cmd.empty())
        return false;

    auto ack = [&](char const* msg)
    {
        if (acknowledge)
            ReplyTo(from, msg);
    };

    if (cmd == "help")
    {
        ack("Orders: stay, follow, flee, leave, summon, grind, reset, passive, aggressive, attack, tank attack, dps attack, maintenance, autogear. Party filters: @tank/@dps/@heal/@ranged <order>.");
        return true;
    }

    if (cmd == "leave")
    {
        if (_clientControlled)
        {
            ack("Self-bot ignores leave.");
            return true;
        }
        if (!_bot->GetGroup())
        {
            ack("Not in a group.");
            return true;
        }
        _bot->RemoveFromGroup(GROUP_REMOVEMETHOD_LEAVE);
        _followGuid = 0;
        ack("Leaving group.");
        return true;
    }

    if (cmd == "stay")
    {
        if (_clientControlled)
        {
            ack("Self-bot: you control movement.");
            return true;
        }
        _stay = true;
        _grind = false;
        ClearForcedTarget();
        if (_bot->GetVictim())
            _bot->AttackStop();
        _bot->GetMotionMaster()->Clear();
        _bot->GetMotionMaster()->MoveIdle();
        _followGuid = 0;
        _lootGuid = 0;
        _chaseGuid = 0;
        ack("Staying.");
        return true;
    }

    if (cmd == "follow" || cmd == "come")
    {
        if (_clientControlled)
        {
            ack("Self-bot: you control movement.");
            return true;
        }
        _stay = false;
        _grind = false;
        ClearForcedTarget();
        _followGuid = 0;
        ack("Following.");
        return true;
    }

    if (cmd == "flee")
    {
        if (_clientControlled)
        {
            ack("Self-bot: you control movement.");
            return true;
        }
        _stay = false;
        _grind = false;
        _passive = false;
        ClearForcedTarget();
        if (_bot->GetVictim())
            _bot->AttackStop();
        _chaseGuid = 0;
        _followGuid = 0;
        // Immediate run to the issuer (usually the group leader / master).
        TeleportToPlayer(from);
        ack("Fleeing to you.");
        return true;
    }

    if (cmd == "summon")
    {
        if (_clientControlled)
        {
            ack("Self-bot ignores summon.");
            return true;
        }
        TeleportToPlayer(from);
        _stay = false;
        _followGuid = 0;
        ack("Summoned.");
        return true;
    }

    if (cmd == "grind")
    {
        _passive = false;
        _grind = true;
        _stay = false;
        ClearForcedTarget();
        ack("Grinding.");
        return true;
    }

    if (cmd == "reset")
    {
        _stay = false;
        _passive = false;
        _grind = false;
        ClearForcedTarget();
        if (_bot->GetVictim())
            _bot->AttackStop();
        _chaseGuid = 0;
        _followGuid = 0;
        if (!_clientControlled)
        {
            _bot->GetMotionMaster()->Clear();
            _bot->GetMotionMaster()->MoveIdle();
        }
        if (_bot->IsNonMeleeSpellCasted(false))
            _bot->InterruptNonMeleeSpells(false);
        ack("Reset.");
        return true;
    }

    if (cmd == "maintenance" || cmd == "autogear")
    {
        sPlayerbotMgr->InitializeBot(_bot);
        ack(cmd == "autogear" ? "Autogear applied." : "Maintenance applied.");
        return true;
    }

    if (cmd == "passive")
    {
        _passive = true;
        _grind = false;
        ClearForcedTarget();
        if (_bot->GetVictim())
            _bot->AttackStop();
        _chaseGuid = 0;
        if (!_clientControlled)
        {
            _bot->GetMotionMaster()->Clear();
            if (_stay)
                _bot->GetMotionMaster()->MoveIdle();
        }
        ack("Passive.");
        return true;
    }

    if (cmd == "aggressive" || cmd == "aggro")
    {
        _passive = false;
        ack("Aggressive.");
        return true;
    }

    if (cmd == "attack")
    {
        Unit* target = from->GetSelectedUnit();
        if (!target || !target->IsAlive() || !_bot->IsValidAttackTarget(target))
        {
            ack("No valid target.");
            return true;
        }
        _passive = false;
        _grind = false;
        SetForcedTarget(target);
        ack("Attacking.");
        return true;
    }

    if (cmd == "tank attack")
    {
        if (GetCombatRole() != CombatRole::Tank)
            return true;
        Unit* target = from->GetSelectedUnit();
        if (!target || !target->IsAlive() || !_bot->IsValidAttackTarget(target))
        {
            ack("No valid target.");
            return true;
        }
        _passive = false;
        _grind = false;
        SetForcedTarget(target);
        ack("Tank attacking.");
        return true;
    }

    if (cmd == "dps attack")
    {
        if (GetCombatRole() != CombatRole::Damage)
            return true;
        Unit* target = from->GetSelectedUnit();
        if (!target || !target->IsAlive() || !_bot->IsValidAttackTarget(target))
        {
            ack("No valid target.");
            return true;
        }
        _passive = false;
        _grind = false;
        SetForcedTarget(target);
        ack("DPS attacking.");
        return true;
    }

    return false;
}

void PlayerbotAI::TeleportToPlayer(Player* master)
{
    if (!master || _clientControlled)
        return;

    _bot->GetMotionMaster()->Clear();
    _bot->GetMotionMaster()->MoveIdle();

    if (_bot->TeleportTo(master->GetMapId(), master->GetPositionX(), master->GetPositionY(),
        master->GetPositionZ(), master->GetOrientation()))
    {
        if (_bot->GetSession() && _bot->GetSession()->IsBot())
            _bot->GetSession()->FinalizeBotTeleport();
    }

    _followGuid = 0;
    _chaseGuid = 0;
    _lootGuid = 0;
}

void PlayerbotAI::TeleportToLeader(Player* leader)
{
    TeleportToPlayer(leader);
}
