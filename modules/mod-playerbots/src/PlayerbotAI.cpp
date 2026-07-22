/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "PlayerbotAI.h"
#include "PlayerbotMgr.h"
#include "rotations/BotRotation.h"
#include "Bag.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Creature.h"
#include "DBCStores.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "GroupMgr.h"
#include "GroupReference.h"
#include "Item.h"
#include "ItemPrototype.h"
#include "LFGMgr.h"
#include "LootMgr.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "PetDefines.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
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

    // Rest / save-mana defaults (override via Playerbots.Rest.* / SaveMana.Threshold).
    // Strategy enable/disable is runtime co/nc only — not config.
    constexpr float BOT_REST_REGEN_PCT = 0.15f;

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
      _followGuid(0), _lootGuid(0), _wanderTimer(0),
      _stay(false), _food(true), _loot(true),
      _passive(false), _grind(false),
      _tankMode(false), _tankAssist(false), _dpsMode(false), _threat(false),
      _healerDps(false), _saveMana(false),
      _forceRest(false), _resting(false),
      _forcedTargetGuid(0), _lfgRoleResponded(false), _lfgProposalResponded(false)
{
    ResetStrategiesToRoleDefaults();
}

void PlayerbotAI::ResetStrategiesToRoleDefaults()
{
    _stay = false;
    _food = true;
    _loot = true;
    _passive = false;
    _grind = false;
    _forceRest = false;

    _tankMode = false;
    _tankAssist = false;
    _dpsMode = false;
    _threat = false;
    _healerDps = false;
    _saveMana = false;

    switch (GetCombatRole())
    {
        case CombatRole::Tank:
            _tankMode = true;
            _tankAssist = true;
            break;
        case CombatRole::Healer:
            _healerDps = false; // strict heal in dungeons
            _saveMana = true;
            break;
        case CombatRole::Damage:
        default:
            _dpsMode = true;
            _threat = true; // keep threat low vs tank
            break;
    }
}

void PlayerbotAI::UpdateAI(uint32 diff)
{
    if (!_bot || !_bot->IsInWorld())
        return;

    // Invites must react immediately - they time out if bots only answer on
    // the coarse AI interval. LFG role/proposal responses run from
    // PlayerbotMgr::Update on the world thread (not here on map workers).
    HandlePendingInvites();

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

    // Self-bot: client owns movement. Cast in combat; allow food regen OOC.
    if (_clientControlled)
    {
        if (!_bot->IsAlive())
            return;
        if (HandleCombatCastOnly())
            return;
        HandleRest();
        return;
    }

    HandleInteractions();

    if (!_bot->IsAlive())
        return; // TODO: corpse release / resurrection handling

    if (HandleCombat())
        return; // engaged: combat drives movement

    // Out of combat: eat/drink before follow/wander when ordered or low resources.
    if (HandleRest())
        return;

    if (_stay)
    {
        HandleStay();
        HandleVendor();
        return;
    }

    // Grouped non-leaders stick with the leader. Solo / group-leader bots wander.
    // Loot and repair walks are solo-only — otherwise they cancel MoveFollow and
    // look like leftover random movement after the bot was invited.
    Group* group = _bot->GetGroup();
    uint64 leaderGuid = group ? group->GetLeaderGUID() : 0;
    bool const followLeader = leaderGuid && leaderGuid != _bot->GetGUID();

    if (!followLeader && HandleLoot())
        return;

    if (followLeader)
        HandleFollow();
    else
    {
        HandleWander();
        HandleVendor();
    }
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
// bot answers the group role check and accepts the join proposal. Called from
// PlayerbotMgr::Update on the world thread (not from map AI ticks) because
// UpdateProposal mutates shared LFG/group state and teleports players.
void PlayerbotAI::HandleLfg()
{
    uint64 const guid = _bot->GetGUID();
    Group* grp = _bot->GetGroup();
    uint8 const roles = ComputeLfgRole();

    // Solo LFG fill: bots join without a party. They still must accept proposals.
    // Grouped bots also set party roles so MoP JoinLfg can pass RoleCheckAllResponded.
    if (grp)
    {
        uint64 const gguid = grp->GetGUID();

        // MoP: HandleLfgJoinOpcode refuses to call JoinLfg until every member has a
        // party role (Group::RoleCheckAllResponded). That happens *before* any LFG
        // ROLECHECK state exists, so bots must set GetMemberRole from their spec
        // while simply grouped — not only during an active role check.
        {
            uint32 const memberRole = grp->GetMemberRole(guid);
            uint32 const combatBits = memberRole & (lfg::PLAYER_ROLE_TANK | lfg::PLAYER_ROLE_HEALER | lfg::PLAYER_ROLE_DAMAGE);
            if (combatBits == 0 || (combatBits & roles) != roles)
            {
                uint32 newRole = roles;
                if (memberRole & lfg::PLAYER_ROLE_LEADER)
                    newRole |= lfg::PLAYER_ROLE_LEADER;
                grp->SetMemberRole(guid, newRole);
                grp->SendUpdate();
            }
        }

        // Prefer group state: player PlayersStore entries can default-construct to
        // NONE if touched before JoinLfg sets ROLECHECK on every member.
        lfg::LfgState const gstate = sLFGMgr->GetState(gguid);
        lfg::LfgState const pstate = sLFGMgr->GetState(guid);
        bool const inRoleCheck = (gstate == lfg::LFG_STATE_ROLECHECK) || (pstate == lfg::LFG_STATE_ROLECHECK);

        if (inRoleCheck)
        {
            uint8 const current = sLFGMgr->GetRoleCheckRoles(gguid, guid);
            // Only submit when unset or mismatched (avoid packet spam every tick).
            if ((current & (lfg::PLAYER_ROLE_TANK | lfg::PLAYER_ROLE_HEALER | lfg::PLAYER_ROLE_DAMAGE)) == 0
                || (current & roles) != roles)
            {
                SF_LOG_INFO("modules", "[mod-playerbots] Bot '%s' answering LFG role check as %u.",
                    _bot->GetName().c_str(), uint32(roles));
                sLFGMgr->UpdateRoleCheck(gguid, guid, roles);
            }
            _lfgRoleResponded = true;
        }
        else if (_lfgRoleResponded)
            _lfgRoleResponded = false;
    }
    else
        _lfgRoleResponded = false;

    // Proposals apply to solo queues and party queues alike — do not require a group.
    lfg::LfgState const pstate = sLFGMgr->GetState(guid);
    lfg::LfgState const gstate = grp ? sLFGMgr->GetState(grp->GetGUID()) : lfg::LFG_STATE_NONE;
    bool const inProposal = (pstate == lfg::LFG_STATE_PROPOSAL) || (gstate == lfg::LFG_STATE_PROPOSAL);
    if (inProposal || sLFGMgr->GetActiveProposalIdForPlayer(guid))
    {
        // Only accept once per proposal; GetActiveProposalIdForPlayer already
        // skips AGREE, but guard against re-entry while state is still PROPOSAL.
        if (!_lfgProposalResponded)
        {
            if (uint32 proposalId = sLFGMgr->GetActiveProposalIdForPlayer(guid))
            {
                SF_LOG_INFO("modules", "[mod-playerbots] Bot '%s' accepting LFG proposal %u (role %u).",
                    _bot->GetName().c_str(), proposalId, uint32(roles));
                sLFGMgr->UpdateProposal(proposalId, guid, true);
                _lfgProposalResponded = true;
            }
        }
    }
    else
        _lfgProposalResponded = false;
}

// Pick a role for the role check from the bot's current specialization.
// Hybrids must be init'd to tank/healer/dps (or an explicit spec) so the party
// can pass CheckGroupRoles.
uint8 PlayerbotAI::ComputeLfgRole()
{
    switch (GetCombatRole())
    {
        case CombatRole::Tank:   return lfg::PLAYER_ROLE_TANK;
        case CombatRole::Healer: return lfg::PLAYER_ROLE_HEALER;
        default:                 return lfg::PLAYER_ROLE_DAMAGE;
    }
}

// Combat: acquire a target from group threats (not the master's mouseover),
// position for the class, and run the rotation.
bool PlayerbotAI::HandleCombat()
{
    // Between pulls: sit/drink with the party instead of chasing the next pack.
    if (!_bot->IsInCombat() && !GroupInCombat() && _food && PartyNeedsRest())
        return false;

    if (GetCombatRole() == CombatRole::Healer)
    {
        if (SelectHealTarget() && HandleHealing())
        {
            StopResting();
            return true;
        }
        // Strict heal: hold with the group, never spend GCDs on damage.
        if (!_healerDps)
        {
            if (GroupInCombat() || !_bot->getAttackers().empty())
            {
                StopResting();
                if (!_clientControlled && !_stay)
                    HandleFollow();
                return true;
            }
            return false;
        }
    }

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

    // Only abort eat/drink once we actually have something to fight.
    StopResting();

    // Spec decides stance (e.g. Elemental ranged, Enhancement melee). Tanks always melee.
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

        if (inMelee)
        {
            if (!_bot->HasInArc(static_cast<float>(M_PI), target))
                _bot->SetInFront(target);
            if (GetCombatRole() == CombatRole::Tank && _tankMode)
                DoTankExtras(target);
            DoRotation(target);
        }
        else if (GetCombatRole() == CombatRole::Tank && _tankMode)
        {
            // Tanks can throw a taunt / ranged threat while closing.
            if (!_bot->HasInArc(static_cast<float>(M_PI), target))
                _bot->SetInFront(target);
            DoTankExtras(target);
        }

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
        if (!_bot->IsStopped())
            _bot->StopMoving();
        if (_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != IDLE_MOTION_TYPE)
        {
            _bot->GetMotionMaster()->Clear();
            _bot->GetMotionMaster()->MoveIdle();
        }
        _bot->SetSelection(target->GetGUID());
        if (!_bot->HasInArc(static_cast<float>(M_PI), target))
            _bot->SetInFront(target);

        _chaseGuid = target->GetGUID();
        DoRotation(target);
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
    // Seated / mid-cast OOC: never clear rest or start a rotation — that cancelled
    // clicked food/drink and wiped eat/drink state every AI tick.
    if (!_bot->IsInCombat() && _bot->getAttackers().empty())
    {
        if (_bot->IsSitState() || _bot->IsNonMeleeSpellCasted(false) || HasFoodOrDrinkAura())
            return false;
    }

    // Heal only when someone is actually injured; otherwise healer-dps may attack.
    if (GetCombatRole() == CombatRole::Healer)
    {
        if (SelectHealTarget() && HandleHealing())
        {
            StopResting();
            return true;
        }
        if (!_healerDps)
            return GroupInCombat() || !_bot->getAttackers().empty();
    }

    Unit* target = SelectTarget();
    if (!target)
    {
        if (_forcedTargetGuid)
            ClearForcedTarget();
        return false;
    }

    StopResting();

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
        DoRotation(target);
    }
    else
    {
        if (!_bot->IsWithinMeleeRange(target))
            return true;
        _bot->Attack(target, true);
        if (GetCombatRole() == CombatRole::Tank && _tankMode)
            DoTankExtras(target);
        DoRotation(target);
    }
    return true;
}

// Target priority (player mouseover is NOT used — only "attack"/"tank attack"
// forced targets come from chat commands):
//   1) Forced command target
//   2) Tanks: peel mobs off healers/party, then hold the pack
//   3) Own attackers
//   4) Group combat: prefer the tank's target, else lowest-HP mob on the party
//   5) Grind (explicit) / self-bot selected unit
Unit* PlayerbotAI::SelectTarget()
{
    if (Unit* forced = GetForcedTarget())
        return forced;

    // Tanks in tank-mode own peel / pack selection.
    if (GetCombatRole() == CombatRole::Tank && _tankMode)
        if (Unit* tankTarget = SelectTankTarget())
            return tankTarget;

    for (Unit* attacker : _bot->getAttackers())
        if (attacker && attacker->IsAlive() && _bot->IsValidAttackTarget(attacker))
            return attacker;

    // Passive bots only fight back; they do not assist or pull.
    if (_passive)
        return nullptr;

    // Party fight: stick with the tank's target when possible, otherwise burn
    // the lowest-HP mob already attacking someone in the group.
    if (Unit* tankAssist = SelectAssistTankTarget())
        return tankAssist;

    if (Unit* groupThreat = SelectGroupThreatTarget())
        return groupThreat;

    if (Unit* execute = SelectLowestHpGroupEnemy())
        return execute;

    if (_grind)
    {
        // Never open-world grind-pull inside dungeons/raids — that yanks the party
        // into trash while healers are drinking.
        Map* map = _bot->GetMap();
        bool const inInstance = map && map->IsInstance();
        if (!inInstance && !PartyNeedsRest())
            if (Unit* grind = SelectGrindTarget())
                return grind;
    }

    // Self-bot only: the real player's selected unit while they are in combat.
    if (_clientControlled && _bot->IsInCombat())
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
    uint32 specId = _bot->GetTalentSpecialization(_bot->GetActiveSpec());
    uint8 cls = _bot->getClass();

    auto isSpec = [&](uint8 tab) -> bool
    {
        uint32 const* specs = GetClassSpecializations(cls);
        return specs && specs[tab] == specId;
    };

    switch (cls)
    {
        case CLASS_HUNTER:
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return true;
        case CLASS_SHAMAN:
            // Elemental = tab 0; Enhancement/Resto melee for now.
            return isSpec(0);
        case CLASS_DRUID:
            // Balance = tab 0.
            return isSpec(0);
        default:
            return false;
    }
}

// One iconic, low-level "filler" attack per class. Used when no Wave-1
// per-spec priority list returns a spell.
uint32 PlayerbotAI::GetFillerSpell() const
{
    switch (_bot->getClass())
    {
        case CLASS_PALADIN: return 35395; // Crusader Strike
        case CLASS_HUNTER:  return 3044;  // Arcane Shot
        case CLASS_ROGUE:
        {
            // Only prefer Mutilate when both daggers are equipped.
            Item* mh = _bot->GetWeaponForAttack(WeaponAttackType::BASE_ATTACK, true);
            Item* oh = _bot->GetWeaponForAttack(WeaponAttackType::OFF_ATTACK, true);
            bool const daggers = mh && oh
                && mh->GetTemplate() && oh->GetTemplate()
                && mh->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER
                && oh->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER;
            if (daggers && _bot->HasSpell(1329))
                return 1329;   // Mutilate
            if (mh && mh->GetTemplate() && mh->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER
                && _bot->HasSpell(111240))
                return 111240; // Dispatch
            if (_bot->HasSpell(16511))
                return 16511;  // Hemorrhage
            return 1752;       // Sinister Strike
        }
        case CLASS_PRIEST:  return 585;   // Smite
        case CLASS_MAGE:    return 116;   // Frostbolt
        case CLASS_WARLOCK: return 686;   // Shadow Bolt
        case CLASS_SHAMAN:  return 403;   // Lightning Bolt (ranged) / melee still auto-attacks
        case CLASS_MONK:    return 100780; // Jab
        default:            return 0;
    }
}

void PlayerbotAI::DoRotation(Unit* target)
{
    if (!target)
        return;

    if (_bot->HasUnitState(UNIT_STATE_CASTING))
        return;

    // DPS threat strategy: pause damage if we are about to rip aggro from the tank.
    if (ShouldThrottleThreat(target))
        return;

    // Interrupts first. During burst windows pop trinkets before fillers so they
    // sync with Ascendance / Vendetta / Adrenaline Rush etc.
    if (BotRotation::TryInterrupt(_bot, target))
        return;
    if (BotRotation::IsBursting(_bot) && BotRotation::TryTrinkets(_bot))
        return;

    if (uint32 spellId = BotRotation::SelectNextSpell(_bot, target))
        if (BotRotation::CastSpell(_bot, target, spellId))
            return;

    if (BotRotation::TryRacial(_bot, target))
        return;
    // Fallback: still use trinkets if no burst window this fight.
    if (BotRotation::TryTrinkets(_bot))
        return;

    if (uint32 filler = GetFillerSpell())
        BotRotation::CastSpell(_bot, target, filler);
}

bool PlayerbotAI::ShouldThrottleThreat(Unit* target) const
{
    if (!_threat || !_bot || !target)
        return false;
    if (GetCombatRole() == CombatRole::Tank && _tankMode)
        return false;
    if (!target->CanHaveThreatList())
        return false;

    float const myThreat = target->getThreatManager().getThreat(_bot);
    HostileReference* cur = target->getThreatManager().getCurrentVictim();
    if (!cur)
        return false;
    float const topThreat = cur->getThreat();
    if (topThreat <= 0.0f)
        return false;

    // Stop damaging when we hold ~80%+ of the current top threat.
    return myThreat >= topThreat * 0.80f;
}

bool PlayerbotAI::HandleHealing()
{
    if (_bot->HasUnitState(UNIT_STATE_CASTING))
        return true;

    // Kick / racial / trinket before heals when a hostile is available.
    Unit* utilityTarget = _bot->GetVictim();
    if (!utilityTarget || !utilityTarget->IsAlive() || !_bot->IsValidAttackTarget(utilityTarget))
    {
        for (Unit* attacker : _bot->getAttackers())
        {
            if (attacker && attacker->IsAlive() && _bot->IsValidAttackTarget(attacker))
            {
                utilityTarget = attacker;
                break;
            }
        }
    }
    if (BotRotation::TryCombatUtilities(_bot, utilityTarget))
        return true;

    Player* ally = SelectHealTarget();
    if (!ally)
        return false;

    uint32 healId = BotRotation::SelectNextHeal(_bot, ally, _saveMana,
        sPlayerbotMgr->GetSaveManaThreshold());
    if (!healId)
        healId = GetHealSpell();
    if (!healId || !BotRotation::CanTryCast(_bot, healId))
        return false;

    constexpr float HEAL_RANGE = 40.0f;
    if (!_bot->IsWithinDistInMap(ally, HEAL_RANGE))
    {
        if (!_clientControlled)
        {
            _bot->GetMotionMaster()->Clear();
            _bot->GetMotionMaster()->MoveChase(ally, 0.0f);
            _chaseGuid = ally->GetGUID();
        }
        return true;
    }

    if (!_clientControlled && !_bot->IsWithinMeleeRange(ally))
        _bot->StopMoving();

    _bot->SetSelection(ally->GetGUID());
    BotRotation::CastHealSpell(_bot, ally, healId);
    return true;
}

Unit* PlayerbotAI::SelectTankTarget()
{
    // Without tank assist, only fight what is already on us.
    if (!_tankAssist)
    {
        for (Unit* attacker : _bot->getAttackers())
            if (attacker && attacker->IsAlive() && _bot->IsValidAttackTarget(attacker))
                return attacker;
        return nullptr;
    }

    Group* group = _bot->GetGroup();
    if (!group)
    {
        for (Unit* attacker : _bot->getAttackers())
            if (attacker && attacker->IsAlive() && _bot->IsValidAttackTarget(attacker))
                return attacker;
        return nullptr;
    }

    Unit* best = nullptr;
    int bestScore = -1;

    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || !member->IsAlive() || !_bot->IsInMap(member))
            continue;

        // Score the member's role so peels off healers beat peels off DPS.
        int memberPriority = 1;
        {
            // Cheap role guess from our own GetCombatRole mapping isn't available
            // for other players' AI; use class/spec heuristics via specialization.
            uint32 specId = member->GetTalentSpecialization(member->GetActiveSpec());
            uint8 cls = member->getClass();
            uint32 const* specs = GetClassSpecializations(cls);
            bool isHeal = false;
            bool isTank = false;
            if (specs)
            {
                switch (cls)
                {
                    case CLASS_PALADIN: isTank = (specId == specs[1]); isHeal = (specId == specs[0]); break;
                    case CLASS_WARRIOR: isTank = (specId == specs[2]); break;
                    case CLASS_DEATH_KNIGHT: isTank = (specId == specs[0]); break;
                    case CLASS_DRUID: isTank = (specId == specs[2]); isHeal = (specId == specs[3]); break;
                    case CLASS_MONK: isTank = (specId == specs[0]); isHeal = (specId == specs[1]); break;
                    case CLASS_PRIEST: isHeal = (specId != specs[2]); break;
                    case CLASS_SHAMAN: isHeal = (specId == specs[2]); break;
                    default: break;
                }
            }
            if (isHeal)
                memberPriority = 3;
            else if (!isTank)
                memberPriority = 2;
            else
                memberPriority = 0; // already on us / another tank
        }

        for (Unit* attacker : member->getAttackers())
        {
            if (!attacker || !attacker->IsAlive() || !_bot->IsValidAttackTarget(attacker))
                continue;
            if (!_bot->IsWithinDistInMap(attacker, 60.0f))
                continue;

            Unit* victim = attacker->GetVictim();
            int score = 0;
            if (victim && victim != _bot)
                score += 1000 + memberPriority * 100;
            else if (victim == _bot)
                score += 100; // already on us — still a candidate for multi-target
            else
                score += 50;

            // Prefer closer when scores tie.
            score -= int(_bot->GetDistance(attacker));

            if (score > bestScore)
            {
                bestScore = score;
                best = attacker;
            }
        }
    }

    return best;
}

Unit* PlayerbotAI::SelectGroupThreatTarget()
{
    Group* group = _bot->GetGroup();
    if (!group)
        return nullptr;

    Unit* best = nullptr;
    float bestHp = 2.0f; // fraction

    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || !member->IsAlive() || !_bot->IsInMap(member))
            continue;

        for (Unit* attacker : member->getAttackers())
        {
            if (!attacker || !attacker->IsAlive() || !_bot->IsValidAttackTarget(attacker))
                continue;
            if (!_bot->IsWithinDistInMap(attacker, 60.0f))
                continue;

            float const hp = attacker->GetMaxHealth()
                ? float(attacker->GetHealth()) / float(attacker->GetMaxHealth())
                : 1.0f;
            if (!best || hp < bestHp)
            {
                best = attacker;
                bestHp = hp;
            }
        }
    }

    return best;
}

Unit* PlayerbotAI::SelectAssistTankTarget()
{
    Group* group = _bot->GetGroup();
    if (!group)
        return nullptr;

    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || !member->IsAlive() || member == _bot || !_bot->IsInMap(member))
            continue;

        // Detect tank the same way GetCombatRole does for us.
        uint32 specId = member->GetTalentSpecialization(member->GetActiveSpec());
        uint8 cls = member->getClass();
        uint32 const* specs = GetClassSpecializations(cls);
        bool isTank = false;
        if (specs)
        {
            switch (cls)
            {
                case CLASS_WARRIOR: isTank = (specId == specs[2]); break;
                case CLASS_PALADIN: isTank = (specId == specs[1]); break;
                case CLASS_DEATH_KNIGHT: isTank = (specId == specs[0]); break;
                case CLASS_DRUID: isTank = (specId == specs[2]); break;
                case CLASS_MONK: isTank = (specId == specs[0]); break;
                default: break;
            }
        }
        if (!isTank)
            continue;

        Unit* victim = member->GetVictim();
        if (victim && victim->IsAlive() && _bot->IsValidAttackTarget(victim) &&
            _bot->IsWithinDistInMap(victim, 60.0f))
            return victim;
    }

    return nullptr;
}

Unit* PlayerbotAI::SelectLowestHpGroupEnemy()
{
    // Alias of SelectGroupThreatTarget — kept separate so SelectTarget's priority
    // list stays readable if we later weight them differently.
    return SelectGroupThreatTarget();
}

Player* PlayerbotAI::SelectHealTarget()
{
    Group* group = _bot->GetGroup();
    Player* best = nullptr;
    float bestPct = 90.0f; // only heal when below this

    auto consider = [&](Player* member)
    {
        if (!member || !member->IsAlive() || !_bot->IsInMap(member))
            return;
        if (!_bot->IsWithinDistInMap(member, 50.0f))
            return;
        if (member->GetMaxHealth() == 0)
            return;
        float const pct = 100.0f * float(member->GetHealth()) / float(member->GetMaxHealth());
        if (pct < bestPct)
        {
            bestPct = pct;
            best = member;
        }
    };

    if (group)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            consider(itr->GetSource());
    }
    else
        consider(_bot);

    return best;
}

uint32 PlayerbotAI::GetTauntSpell() const
{
    // Prefer a known, off-cooldown taunt. MoP DBC names 62124 "Reckoning"
    // (Hand of Reckoning in the spellbook).
    uint32 candidates[3] = { 0, 0, 0 };
    switch (_bot->getClass())
    {
        case CLASS_WARRIOR:
            candidates[0] = 355;      // Taunt
            break;
        case CLASS_PALADIN:
            candidates[0] = 62124;    // Reckoning / Hand of Reckoning
            candidates[1] = 31789;    // Righteous Defense (multi, needs ally)
            break;
        case CLASS_DEATH_KNIGHT:
            candidates[0] = 56222;    // Dark Command
            candidates[1] = 49576;    // Death Grip
            break;
        case CLASS_DRUID:
            candidates[0] = 6795;     // Growl
            break;
        case CLASS_MONK:
            candidates[0] = 115546;   // Provoke
            break;
        default:
            return 0;
    }

    for (uint32 id : candidates)
        if (id && _bot->HasSpell(id) && !_bot->HasSpellCooldown(id))
            return id;
    return 0;
}

uint32 PlayerbotAI::GetAoeThreatSpell() const
{
    uint32 candidates[3] = { 0, 0, 0 };
    switch (_bot->getClass())
    {
        case CLASS_WARRIOR:
            candidates[0] = 6343;     // Thunder Clap
            candidates[1] = 1680;     // Whirlwind
            break;
        case CLASS_PALADIN:
            candidates[0] = 31935;    // Avenger's Shield (ranged)
            candidates[1] = 53595;    // Hammer of the Righteous
            candidates[2] = 26573;    // Consecration
            break;
        case CLASS_DEATH_KNIGHT:
            candidates[0] = 48721;    // Blood Boil
            candidates[1] = 43265;    // Death and Decay
            break;
        case CLASS_DRUID:
            candidates[0] = 77758;    // Thrash
            candidates[1] = 106785;   // Swipe
            break;
        case CLASS_MONK:
            candidates[0] = 121253;   // Keg Smash
            candidates[1] = 115181;   // Breath of Fire
            break;
        default:
            return 0;
    }

    for (uint32 id : candidates)
        if (id && _bot->HasSpell(id) && !_bot->HasSpellCooldown(id))
            return id;
    return 0;
}

uint32 PlayerbotAI::GetHealSpell() const
{
    float lowestPct = 100.0f;
    if (Group* group = _bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsAlive() || member->GetMaxHealth() == 0)
                continue;
            float const pct = 100.0f * float(member->GetHealth()) / float(member->GetMaxHealth());
            if (pct < lowestPct)
                lowestPct = pct;
        }
    }
    else if (_bot->GetMaxHealth())
        lowestPct = 100.0f * float(_bot->GetHealth()) / float(_bot->GetMaxHealth());

    bool const urgent = lowestPct < 40.0f;

    switch (_bot->getClass())
    {
        case CLASS_PALADIN:
            if (urgent && BotRotation::SpellReady(_bot, 19750))
                return 19750; // Flash of Light
            if (BotRotation::SpellReady(_bot, 20473))
                return 20473; // Holy Shock
            return BotRotation::SpellReady(_bot, 635) ? 635 : 19750; // Holy Light / FoL
        case CLASS_PRIEST:
            if (urgent && BotRotation::SpellReady(_bot, 2061))
                return 2061; // Flash Heal
            if (BotRotation::SpellReady(_bot, 2060))
                return 2060; // Heal
            return 2061;
        case CLASS_SHAMAN:
            if (urgent && BotRotation::SpellReady(_bot, 8004))
                return 8004; // Healing Surge
            if (BotRotation::SpellReady(_bot, 77472))
                return 77472; // Greater Healing Wave
            return BotRotation::SpellReady(_bot, 331) ? 331 : 8004;
        case CLASS_DRUID:
            if (urgent && BotRotation::SpellReady(_bot, 8936))
                return 8936; // Regrowth
            if (BotRotation::SpellReady(_bot, 5185))
                return 5185; // Healing Touch
            return BotRotation::SpellReady(_bot, 774) ? 774 : 5185; // Rejuvenation
        case CLASS_MONK:
            if (BotRotation::SpellReady(_bot, 116694))
                return 116694; // Surging Mist
            return 115175; // Soothing Mist
        default:
            return 0;
    }
}

void PlayerbotAI::DoTankExtras(Unit* target)
{
    if (!target || _bot->HasUnitState(UNIT_STATE_CASTING))
        return;

    // Taunt when the current focus mob is hitting someone else.
    Unit* victim = target->GetVictim();
    if (victim && victim != _bot)
    {
        uint32 taunt = GetTauntSpell();
        // Righteous Defense is cast on the ally being attacked, not the mob.
        if (taunt == 31789)
        {
            if (Player* ally = victim->ToPlayer())
                if (BotRotation::CanTryCast(_bot, taunt) && BotRotation::CastSpell(_bot, ally, taunt))
                    return;
        }
        else if (taunt && BotRotation::CanTryCast(_bot, taunt))
        {
            if (BotRotation::CastSpell(_bot, target, taunt))
                return;
        }
    }

    // Multi-target: drop an AoE / ranged threat ability when 2+ hostiles are nearby.
    if (BotRotation::CountNearbyEnemies(_bot, 10.0f) >= 2)
    {
        if (uint32 aoe = GetAoeThreatSpell())
            if (BotRotation::CanTryCast(_bot, aoe) && BotRotation::CastSpell(_bot, target, aoe))
                return;
    }
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
    if (!_loot)
        return false;

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

    // Cancel solo wander state so a mid-walk / pause never resumes after invite.
    _wanderTimer = 0;
    _lootGuid = 0;

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

    MovementGeneratorType const moveType = _bot->GetMotionMaster()->GetCurrentMovementGeneratorType();
    if (_followGuid != leaderGuid || moveType != FOLLOW_MOTION_TYPE)
    {
        // Clear first so an in-progress wander MovePoint/spline is fully stopped
        // before Follow takes over (Mutate alone left bots mid-wander visually).
        _bot->GetMotionMaster()->Clear();
        float angle = float(_bot->GetGUIDLow() % 16) / 16.0f * TWO_PI;
        _bot->GetMotionMaster()->MoveFollow(leader, BOT_FOLLOW_DIST, angle);
        _followGuid = leaderGuid;
        _chaseGuid = 0;
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

    // Self-bot whispering to yourself re-enters OnChat. If the reply looks like
    // a co/nc command (e.g. "co +healer dps"), that recurses until stack overflow.
    if (from->GetGUID() == _bot->GetGUID())
    {
        if (WorldSession* session = from->GetSession())
            ChatHandler(session).SendSysMessage(text.c_str());
        return;
    }

    _bot->Whisper(text, Language::LANG_UNIVERSAL, from->GetGUID());
}

float PlayerbotAI::HealthPct() const
{
    if (!_bot || !_bot->GetMaxHealth())
        return 100.0f;
    return 100.0f * float(_bot->GetHealth()) / float(_bot->GetMaxHealth());
}

float PlayerbotAI::ManaPct() const
{
    if (!_bot || !UsesMana() || !_bot->GetMaxPower(POWER_MANA))
        return 100.0f;
    return 100.0f * float(_bot->GetPower(POWER_MANA)) / float(_bot->GetMaxPower(POWER_MANA));
}

bool PlayerbotAI::UsesMana() const
{
    return _bot && _bot->GetMaxPower(POWER_MANA) > 0;
}

bool PlayerbotAI::GroupInCombat() const
{
    if (!_bot)
        return false;
    if (_bot->IsInCombat())
        return true;
    Group* group = _bot->GetGroup();
    if (!group)
        return false;
    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (member && member->IsAlive() && member->IsInCombat() && _bot->IsInMap(member))
            return true;
    }
    return false;
}

void PlayerbotAI::StopResting()
{
    _forceRest = false;
    _resting = false;
    if (!_bot)
        return;
    // Self-bot owns stand/sit (AFK sit, manual food/drink). Never yank them up.
    if (_clientControlled)
        return;
    if (_bot->getStandState() != UNIT_STAND_STATE_STAND)
        _bot->SetStandState(UNIT_STAND_STATE_STAND);
}

bool PlayerbotAI::HasFoodOrDrinkAura() const
{
    if (!_bot)
        return false;

    Unit::AuraApplicationMap const& auras = _bot->GetAppliedAuras();
    for (Unit::AuraApplicationMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
    {
        AuraApplication const* app = itr->second;
        if (!app || !app->GetBase())
            continue;
        SpellInfo const* info = app->GetBase()->GetSpellInfo();
        if (!info)
            continue;
        // Food/drink buffs break if you stand (NOT_SEATED interrupt).
        if (!(info->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED))
            continue;
        if (info->HasAura(SPELL_AURA_OBS_MOD_HEALTH) || info->HasAura(SPELL_AURA_MOD_POWER_REGEN))
            return true;
    }
    return false;
}

bool PlayerbotAI::TryUseFoodOrDrinkItem()
{
    if (!_bot || _bot->IsInCombat())
        return false;
    if (_bot->IsNonMeleeSpellCasted(false) || HasFoodOrDrinkAura())
        return true;

    bool const needHp = HealthPct() < 98.0f;
    bool const needMana = UsesMana() && ManaPct() < 98.0f;
    if (!needHp && !needMana)
        return false;

    auto categoryOf = [](ItemTemplate const* proto) -> uint32
    {
        if (!proto)
            return 0;
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (proto->Spells[i].SpellId && proto->Spells[i].SpellCategory)
                return proto->Spells[i].SpellCategory;
        }
        return 0;
    };

    auto prefer = [&](Item* item) -> int
    {
        if (!item)
            return -1;
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || proto->Class != ITEM_CLASS_CONSUMABLE || proto->SubClass != ITEM_SUBCLASS_FOOD_DRINK)
            return -1;
        if (_bot->CanUseItem(item) != EQUIP_ERR_OK)
            return -1;

        uint32 const cat = categoryOf(proto);
        // Higher score = better match for what we still need.
        if (needHp && needMana)
            return 3;
        if (needMana && cat == SPELL_CATEGORY_DRINK)
            return 3;
        if (needHp && cat == SPELL_CATEGORY_FOOD)
            return 3;
        if (needMana && cat == SPELL_CATEGORY_FOOD)
            return 1; // some foods also restore mana via refreshment
        if (needHp && cat == SPELL_CATEGORY_DRINK)
            return 0;
        return 2;
    };

    Item* best = nullptr;
    int bestScore = -1;

    auto consider = [&](Item* item)
    {
        int const score = prefer(item);
        if (score > bestScore)
        {
            bestScore = score;
            best = item;
        }
    };

    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        consider(_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* container = _bot->GetBagByPos(bag))
            for (uint32 slot = 0; slot < container->GetBagSize(); ++slot)
                consider(_bot->GetItemByPos(bag, uint8(slot)));
    }

    if (!best || bestScore < 0)
        return false;

    SpellCastTargets targets;
    targets.SetUnitTarget(_bot);
    _bot->CastItemUseSpell(best, targets, 1, 0);
    // Only treat as success if a cast/aura actually started — otherwise fall
    // through to direct regen (failed item use used to skip recovery entirely).
    return _bot->IsNonMeleeSpellCasted(false) || HasFoodOrDrinkAura();
}

void PlayerbotAI::ApplyDirectRestRegen()
{
    if (!_bot)
        return;
    // Never fight the client or food/drink auras — SetPower/SetHealth every AI tick
    // desyncs the mana bar (100% → real → 100% flicker).
    if (_clientControlled)
        return;
    if (_bot->IsNonMeleeSpellCasted(false) || HasFoodOrDrinkAura())
        return;

    if (uint32 const maxHp = _bot->GetMaxHealth())
    {
        uint32 const cur = _bot->GetHealth();
        if (cur < maxHp)
        {
            uint32 const gain = std::max<uint32>(1, uint32(float(maxHp) * BOT_REST_REGEN_PCT));
            _bot->SetHealth(std::min(maxHp, cur + gain));
        }
    }
    if (UsesMana())
    {
        int32 const maxMana = _bot->GetMaxPower(POWER_MANA);
        int32 const cur = _bot->GetPower(POWER_MANA);
        if (maxMana > 0 && cur < maxMana)
        {
            int32 const gain = std::max(1, int32(float(maxMana) * BOT_REST_REGEN_PCT));
            _bot->SetPower(POWER_MANA, std::min(maxMana, cur + gain));
        }
    }
}

bool PlayerbotAI::PartyNeedsRest() const
{
    if (!_bot || !_food)
        return false;

    auto memberNeeds = [&](Player* member) -> bool
    {
        if (!member || !member->IsAlive() || member->IsInCombat())
            return false;
        float const hp = member->GetMaxHealth()
            ? (100.0f * float(member->GetHealth()) / float(member->GetMaxHealth())) : 100.0f;
        if (hp < sPlayerbotMgr->GetRestHealthPct())
            return true;
        if (member->GetMaxPower(POWER_MANA) > 0)
        {
            float const mana = 100.0f * float(member->GetPower(POWER_MANA))
                / float(member->GetMaxPower(POWER_MANA));
            if (mana < sPlayerbotMgr->GetRestManaPct())
                return true;
        }
        return false;
    };

    if (memberNeeds(_bot))
        return true;

    Group* group = _bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (member && member != _bot && memberNeeds(member))
            return true;
    }
    return false;
}

bool PlayerbotAI::StartRefreshment()
{
    if (!_bot)
        return false;

    // Self-bot: only regen while the player is already sitting (they own movement).
    // Never force sit/stand here — that interrupted AFK sit and clicked drinks.
    if (_clientControlled)
    {
        if (_bot->getStandState() != UNIT_STAND_STATE_SIT)
            return false;
    }
    else if (_bot->getStandState() != UNIT_STAND_STATE_SIT)
        _bot->SetStandState(UNIT_STAND_STATE_SIT);

    if (!_clientControlled)
    {
        _bot->GetMotionMaster()->Clear();
        _bot->GetMotionMaster()->MoveIdle();
        _followGuid = 0;
        _chaseGuid = 0;
        if (_bot->GetVictim())
            _bot->AttackStop();
    }
    _resting = true;

    // Already eating/drinking: wait on the aura — do not SetPower (causes mana flicker).
    if (_bot->IsNonMeleeSpellCasted(false) || HasFoodOrDrinkAura())
        return true;

    // Prefer real bag food/drink.
    if (TryUseFoodOrDrinkItem())
        return true;

    // Socket-bot fallback only when no consumable is active.
    ApplyDirectRestRegen();
    return true;
}

bool PlayerbotAI::HandleRest()
{
    if (!_bot)
        return false;
    // Only block on *this* bot's combat — party combat must not stop drinking.
    if (_bot->IsInCombat())
    {
        StopResting();
        return false;
    }

    float const hpPct = HealthPct();
    float const manaPct = ManaPct();
    bool const needHp = hpPct < sPlayerbotMgr->GetRestHealthPct();
    bool const needMana = UsesMana() && manaPct < sPlayerbotMgr->GetRestManaPct();
    bool const lowResources = needHp || needMana;
    // Non-mana classes only care about HP; mana classes need both near full.
    bool const nearlyFull = hpPct >= 98.0f && (!UsesMana() || manaPct >= 98.0f);
    bool const itemResting = HasFoodOrDrinkAura() || (_bot->IsSitState() && _bot->IsNonMeleeSpellCasted(false));

    // Only THIS bot rests. Full / non-mana bots keep following while others drink.
    // (PartyNeedsRest still gates new pulls in HandleCombat, not follow.)
    bool const shouldRest = _forceRest || _resting || (_food && lowResources) || itemResting;

    if (!shouldRest)
        return false;

    // Done recovering — stand (socket bots) and resume follow/wander next tick.
    if (nearlyFull && !itemResting)
    {
        StopResting();
        return false;
    }

    return StartRefreshment();
}

std::string PlayerbotAI::FormatStrategies(bool combat) const
{
    std::string out;
    auto add = [&](char const* name, bool on)
    {
        if (!out.empty())
            out += ", ";
        out += on ? "+" : "-";
        out += name;
    };

    CombatRole const role = GetCombatRole();

    if (combat)
    {
        add("passive", _passive);
        add("grind", _grind);
        switch (role)
        {
            case CombatRole::Tank:
                add("tank", _tankMode);
                add("tank assist", _tankAssist);
                add("dps", !_tankMode);
                break;
            case CombatRole::Healer:
                add("heal", !_healerDps);
                add("healer dps", _healerDps);
                add("save mana", _saveMana);
                break;
            case CombatRole::Damage:
            default:
                add("dps", _dpsMode);
                add("threat", _threat);
                break;
        }
    }
    else
    {
        add("food", _food);
        add("follow", !_stay);
        add("stay", _stay);
        add("loot", _loot);
        add("passive", _passive);
        add("grind", _grind);
    }
    return out;
}

bool PlayerbotAI::StrategyAllowed(bool combat, std::string const& name) const
{
    if (name == "passive" || name == "grind")
        return true;

    if (!combat)
        return name == "food" || name == "follow" || name == "stay" || name == "loot";

    switch (GetCombatRole())
    {
        case CombatRole::Tank:
            return name == "tank" || name == "tank assist" || name == "tankassist" || name == "dps";
        case CombatRole::Healer:
            return name == "heal" || name == "healer dps" || name == "healdps"
                || name == "heal dps" || name == "save mana" || name == "savemana";
        case CombatRole::Damage:
        default:
            return name == "dps" || name == "threat";
    }
}

bool PlayerbotAI::ApplyStrategyChange(bool combat, char op, std::string const& name, std::string& report)
{
    if (name.empty())
        return false;

    if (!StrategyAllowed(combat, name))
    {
        report += (report.empty() ? "" : ", ");
        report += "!" + name + "(wrong role)";
        return true;
    }

    bool enable = (op == '+');

    auto setFlag = [&](bool& flag, char const* label)
    {
        if (op == '~')
            flag = !flag;
        else
            flag = enable;
        report += (report.empty() ? "" : ", ");
        report += flag ? "+" : "-";
        report += label;
    };

    if (!combat)
    {
        if (name == "food")
        {
            setFlag(_food, "food");
            if (!_food)
                StopResting();
            return true;
        }
        if (name == "follow")
        {
            if (op == '~')
                _stay = !_stay;
            else
                _stay = !enable;
            if (!_stay)
            {
                _forceRest = false;
                if (_bot->getStandState() != UNIT_STAND_STATE_STAND)
                    _bot->SetStandState(UNIT_STAND_STATE_STAND);
                _followGuid = 0;
            }
            report += (report.empty() ? "" : ", ");
            report += (!_stay ? "+follow" : "+stay");
            return true;
        }
        if (name == "stay")
        {
            setFlag(_stay, "stay");
            if (_stay)
            {
                _grind = false;
                ClearForcedTarget();
                if (!_clientControlled)
                {
                    _bot->GetMotionMaster()->Clear();
                    _bot->GetMotionMaster()->MoveIdle();
                }
                _followGuid = 0;
                _chaseGuid = 0;
            }
            return true;
        }
        if (name == "loot")
        {
            setFlag(_loot, "loot");
            if (!_loot)
                _lootGuid = 0;
            return true;
        }
        if (name == "passive")
        {
            setFlag(_passive, "passive");
            return true;
        }
        if (name == "grind")
        {
            setFlag(_grind, "grind");
            if (_grind) { _passive = false; _stay = false; }
            return true;
        }
        return false;
    }

    if (name == "passive")
    {
        setFlag(_passive, "passive");
        if (_passive) { _grind = false; ClearForcedTarget(); }
        return true;
    }
    if (name == "grind")
    {
        setFlag(_grind, "grind");
        if (_grind) { _passive = false; _stay = false; }
        return true;
    }
    if (name == "tank")
    {
        setFlag(_tankMode, "tank");
        _dpsMode = !_tankMode;
        return true;
    }
    if (name == "tank assist" || name == "tankassist")
    {
        setFlag(_tankAssist, "tank assist");
        return true;
    }
    if (name == "dps")
    {
        if (GetCombatRole() == CombatRole::Tank)
        {
            if (op == '~')
                _tankMode = !_tankMode;
            else
                _tankMode = !enable;
            _dpsMode = !_tankMode;
            report += (report.empty() ? "" : ", ");
            report += _tankMode ? "+tank" : "+dps";
            return true;
        }
        setFlag(_dpsMode, "dps");
        return true;
    }
    if (name == "threat")
    {
        setFlag(_threat, "threat");
        return true;
    }
    if (name == "heal")
    {
        if (op == '~')
            _healerDps = !_healerDps;
        else
            _healerDps = !enable;
        report += (report.empty() ? "" : ", ");
        report += !_healerDps ? "+heal" : "+healer dps";
        return true;
    }
    if (name == "healer dps" || name == "healdps" || name == "heal dps")
    {
        setFlag(_healerDps, "healer dps");
        return true;
    }
    if (name == "save mana" || name == "savemana")
    {
        setFlag(_saveMana, "save mana");
        return true;
    }
    return false;
}

bool PlayerbotAI::HandleStrategyCommand(Player* from, std::string const& cmd, bool /*acknowledge*/)
{
    bool combat = false;
    std::string body;
    if (cmd == "co" || cmd == "co?" || cmd == "co ?")
    {
        combat = true;
        body = "?";
    }
    else if (cmd == "nc" || cmd == "nc?" || cmd == "nc ?")
    {
        combat = false;
        body = "?";
    }
    else if (cmd.rfind("co ", 0) == 0)
    {
        combat = true;
        body = cmd.substr(3);
    }
    else if (cmd.rfind("nc ", 0) == 0)
    {
        combat = false;
        body = cmd.substr(3);
    }
    else
        return false;

    while (!body.empty() && body.front() == ' ')
        body.erase(body.begin());
    while (!body.empty() && body.back() == ' ')
        body.pop_back();

    // Always whisper strategy info back (party orders used to be silent).
    auto reply = [&](std::string const& msg)
    {
        ReplyTo(from, msg);
    };

    if (body.empty() || body == "?")
    {
        reply(std::string(combat ? "co: " : "nc: ") + FormatStrategies(combat));
        // Also show the other bucket so players see full state in one ask.
        reply(std::string(combat ? "nc: " : "co: ") + FormatStrategies(!combat));
        return true;
    }

    std::string report;
    std::string unknown;
    size_t pos = 0;
    while (pos < body.size())
    {
        size_t comma = body.find(',', pos);
        std::string token = body.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        pos = (comma == std::string::npos) ? body.size() : comma + 1;

        while (!token.empty() && token.front() == ' ')
            token.erase(token.begin());
        while (!token.empty() && token.back() == ' ')
            token.pop_back();
        if (token.empty())
            continue;

        char op = token[0];
        if (op != '+' && op != '-' && op != '~')
        {
            unknown += (unknown.empty() ? "" : ", ") + token;
            continue;
        }

        std::string name = token.substr(1);
        while (!name.empty() && name.front() == ' ')
            name.erase(name.begin());
        while (!name.empty() && name.back() == ' ')
            name.pop_back();

        if (!ApplyStrategyChange(combat, op, name, report))
            unknown += (unknown.empty() ? "" : ", ") + token;
    }

    // Use "co:" / "nc:" (no space) so status replies never re-parse as commands
    // if they somehow re-enter OnChat (e.g. self-whisper before ReplyTo was fixed).
    if (!report.empty())
        reply(std::string(combat ? "co:" : "nc:") + report);
    if (!unknown.empty())
        reply(std::string("unknown: ") + unknown);

    reply(std::string(combat ? "co: " : "nc: ") + FormatStrategies(combat));
    return true;
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

    // AC-style combat / non-combat strategy engine: "co +save mana", "nc -food", "co ?"
    if (HandleStrategyCommand(from, cmd, acknowledge))
        return true;

    auto ack = [&](char const* msg)
    {
        if (acknowledge)
            ReplyTo(from, msg);
    };

    if (cmd == "help")
    {
        ack("Orders: stay, follow, flee, leave, summon, grind, reset, passive, aggressive, attack, tank/dps attack, eat/drink, maintenance. Strategies: co/nc +name,-name,~name or co?/nc?. Filters: @tank/@dps/@heal/@ranged.");
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
        _forceRest = false;
        ClearForcedTarget();
        if (_bot->getStandState() != UNIT_STAND_STATE_STAND)
            _bot->SetStandState(UNIT_STAND_STATE_STAND);
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
        _forceRest = false;
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
        if (_bot->getStandState() != UNIT_STAND_STATE_STAND)
            _bot->SetStandState(UNIT_STAND_STATE_STAND);
        ack("Reset.");
        return true;
    }

    if (cmd == "maintenance" || cmd == "autogear")
    {
        sPlayerbotMgr->InitializeBot(_bot);
        ack(cmd == "autogear" ? "Autogear applied." : "Maintenance applied.");
        return true;
    }

    if (cmd == "eat" || cmd == "drink" || cmd == "food")
    {
        if (_bot->IsInCombat())
        {
            ack("In combat — cannot eat/drink.");
            return true;
        }
        _forceRest = true;
        _food = true;
        _grind = false;
        ClearForcedTarget();
        if (_bot->GetVictim())
            _bot->AttackStop();
        // Socket bots sit via StartRefreshment. Self-bot: sit only if standing so
        // an explicit eat/drink order can start; never fight an already-sitting player.
        if (_clientControlled && _bot->getStandState() != UNIT_STAND_STATE_SIT)
            _bot->SetStandState(UNIT_STAND_STATE_SIT);
        StartRefreshment();
        ack(_clientControlled
            ? "Eating/drinking (stay seated until full, or use your food/drink)."
            : "Eating/drinking.");
        return true;
    }

    if (cmd == "heal")
    {
        std::string report;
        ApplyStrategyChange(true, '+', "heal", report);
        ack(report.empty() ? "heal not available for this role." : report.c_str());
        return true;
    }

    if (cmd == "healer dps" || cmd == "healdps" || cmd == "heal dps")
    {
        std::string report;
        ApplyStrategyChange(true, '+', "healer dps", report);
        ack(report.empty() ? "healer dps not available for this role." : report.c_str());
        return true;
    }

    if (cmd == "save mana" || cmd == "savemana" || cmd == "save mana on")
    {
        std::string report;
        ApplyStrategyChange(true, '+', "save mana", report);
        ack(report.empty() ? "save mana not available for this role." : report.c_str());
        return true;
    }

    if (cmd == "save mana off" || cmd == "savemana off")
    {
        std::string report;
        ApplyStrategyChange(true, '-', "save mana", report);
        ack(report.empty() ? "save mana not available for this role." : report.c_str());
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
