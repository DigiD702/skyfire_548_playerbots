/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "PlayerbotAI.h"
#include "PlayerbotMgr.h"
#include "engine/BotAiEngine.h"
#include "engine/BotFormation.h"
#include "engine/BotMovement.h"
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
#include "ObjectMgr.h"
#include "Opcodes.h"
#include <unordered_set>
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

    // Conjured Mana Pudding wrapper (item cast). Actual seated regen auras:
    constexpr uint32 BOT_REFRESHMENT_SPELL = 128701;
    constexpr uint32 BOT_FOOD_AURA_SPELL = 104935;   // Food (OBS_MOD_HEALTH)
    constexpr uint32 BOT_DRINK_AURA_SPELL = 92800;   // Drink (MOD_POWER_REGEN via periodic dummy)

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
    // Keep loot near the group — long seeks cause follow↔loot thrash.
    constexpr float BOT_LOOT_SEEK_DIST = 25.0f;
    constexpr float BOT_LOOT_LEADER_RADIUS = 40.0f;
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

    bool CanFitLootItem(Player* looter, uint32 itemId, uint32 count)
    {
        if (!looter || !itemId || !count)
            return false;
        ItemPosCountVec dest;
        return looter->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count) == EQUIP_ERR_OK;
    }

    // Free loot the bot can actually put in bags (or gold). Does not include
    // roll-blocked threshold items — those are opened once to start rolls.
    bool HasStorableFreeLoot(Player* looter, Loot* loot)
    {
        if (!looter || !loot || loot->isLooted())
            return false;
        if (loot->gold)
            return true;
        for (LootItem const& item : loot->items)
        {
            if (item.is_looted || item.is_blocked)
                continue;
            if (CanFitLootItem(looter, item.itemid, item.count))
                return true;
        }
        return false;
    }

    bool HasPendingLootRolls(Loot* loot)
    {
        return loot && loot->hasOverThresholdItem();
    }

    // Skip corpses that only have roll-blocked threshold items (LFG NBG thrash),
    // or free loot the bot cannot store (full bags) after already opening for rolls.
    bool HasTakeableLoot(Player* looter, Creature* creature,
        std::unordered_set<uint64> const* bagFullSkip = nullptr,
        std::unordered_set<uint64> const* rollOpened = nullptr)
    {
        if (!looter || !creature || creature->IsAlive() || !looter->isAllowedToLoot(creature))
            return false;
        Loot* loot = &creature->loot;
        if (!loot || loot->isLooted())
            return false;

        uint64 const guid = creature->GetGUID();
        bool const pendingRolls = HasPendingLootRolls(loot);
        bool const needOpenForRolls = pendingRolls && (!rollOpened || !rollOpened->count(guid));
        if (needOpenForRolls)
            return true;

        if (bagFullSkip && bagFullSkip->count(guid) && !HasStorableFreeLoot(looter, loot))
            return false;

        return HasStorableFreeLoot(looter, loot);
    }

    struct BotLootCreatureCheck
    {
        BotLootCreatureCheck(Player* looter, float range,
            std::unordered_set<uint64> const* bagFullSkip,
            std::unordered_set<uint64> const* rollOpened)
            : _looter(looter), _range(range), _bagFullSkip(bagFullSkip), _rollOpened(rollOpened) { }

        bool operator()(Creature* creature) const
        {
            if (!creature || creature->IsAlive())
                return false;
            if (!_looter->IsWithinDist(creature, _range))
                return false;
            return HasTakeableLoot(_looter, creature, _bagFullSkip, _rollOpened);
        }

        Player* _looter;
        float _range;
        std::unordered_set<uint64> const* _bagFullSkip;
        std::unordered_set<uint64> const* _rollOpened;
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
      _tankMode(false), _tankAssist(false), _dpsMode(false), _dpsAssist(false),
      _threat(false), _healerDps(false), _saveMana(false), _waitForAttack(false),
      _forceRest(false), _resting(false), _holdAssist(false),
      _forcedTargetGuid(0), _lfgRoleResponded(false), _lfgProposalResponded(false)
{
    ResetStrategiesToRoleDefaults();
    _aiEngine = std::make_unique<BotAiEngine>(this);
}

PlayerbotAI::~PlayerbotAI() = default;

void PlayerbotAI::ResetStrategiesToRoleDefaults()
{
    CombatRole const role = GetCombatRole();
    _strategies.ResetToRoleDefaults(role == CombatRole::Tank, role == CombatRole::Healer);
    SyncFlagsFromStrategies();
    _forceRest = false;
    _holdAssist = false;
    ClearForcedTarget();
}

void PlayerbotAI::SyncFlagsFromStrategies()
{
    _food = _strategies.Has("food", BotState::NonCombat);
    _loot = _strategies.Has("loot", BotState::NonCombat);
    _stay = _strategies.Has("stay", BotState::NonCombat)
        || _strategies.Has("stay", BotState::Combat);
    if (_strategies.Has("follow", BotState::NonCombat))
        _stay = false;

    _passive = _strategies.Has("passive", BotState::Combat)
        || _strategies.Has("passive", BotState::NonCombat);
    _grind = _strategies.Has("grind", BotState::Combat)
        || _strategies.Has("grind", BotState::NonCombat);

    _tankMode = _strategies.Has("tank", BotState::Combat);
    _tankAssist = _strategies.Has("tank assist", BotState::Combat);
    _dpsMode = _strategies.Has("dps", BotState::Combat);
    _dpsAssist = _strategies.Has("dps assist", BotState::Combat);
    _threat = _strategies.Has("threat", BotState::Combat);
    _healerDps = _strategies.Has("healer dps", BotState::Combat);
    _saveMana = _strategies.Has("save mana", BotState::Combat);
    _waitForAttack = _strategies.Has("wait for attack", BotState::Combat);
    if (_strategies.Has("heal", BotState::Combat))
        _healerDps = false;

    // Tank without explicit +dps stays in tank mode.
    if (GetCombatRole() == CombatRole::Tank && !_strategies.Has("dps", BotState::Combat))
        _tankMode = true;
    if (GetCombatRole() == CombatRole::Damage && !_dpsMode)
        _dpsMode = true; // damage bots always "dps" unless somehow cleared

    RebuildAiEngine();
}

void PlayerbotAI::RebuildAiEngine()
{
    if (_aiEngine)
        _aiEngine->Rebuild();
}

bool PlayerbotAI::RunCombat() { return HandleCombat(); }
bool PlayerbotAI::RunCombatCastOnly() { return HandleCombatCastOnly(); }
bool PlayerbotAI::RunRest() { return HandleRest(); }
void PlayerbotAI::RunFollow() { HandleFollow(); }
void PlayerbotAI::RunStay() { HandleStay(); }
bool PlayerbotAI::RunLoot() { return HandleLoot(); }
void PlayerbotAI::RunWander() { HandleWander(); }
void PlayerbotAI::RunVendor() { HandleVendor(); }
bool PlayerbotAI::IsGroupInCombatPublic() const { return GroupInCombat(); }

Unit* PlayerbotAI::SelectLowestHpGroupEnemyPublic() { return SelectLowestHpGroupEnemy(); }
Unit* PlayerbotAI::SelectAssistTankTargetPublic() { return SelectAssistTankTarget(); }
Unit* PlayerbotAI::SelectTankTargetPublic() { return SelectTankTarget(); }

int PlayerbotAI::GetCombatRolePublic() const
{
    switch (GetCombatRole())
    {
        case CombatRole::Tank:   return 0;
        case CombatRole::Healer: return 1;
        default:                 return 2;
    }
}

bool PlayerbotAI::IsRangedClassPublic() const { return IsRangedClass(); }

bool PlayerbotAI::HasEngageTarget() const
{
    if (_forcedTargetGuid && GetForcedTarget())
        return true;
    if (_targets.GetPullGuid())
        return _targets.GetPullTarget(const_cast<PlayerbotAI*>(this)) != nullptr;
    return false;
}

bool PlayerbotAI::ShouldWaitForAttack() const
{
    if (!_waitForAttack || !_bot)
        return false;
    // Explicit attack/pull orders never wait — engage immediately.
    if (HasEngageTarget())
        return false;
    // Tanks never wait — they are the pull.
    if (GetCombatRole() == CombatRole::Tank && _tankMode)
        return false;
    // Always fight back if something is hitting us.
    if (!_bot->getAttackers().empty())
        return false;
    if (!GroupInCombat() && !_bot->IsInCombat())
        return false;
    if (!_combatStartTime)
        return false;
    uint32 const waitSec = sPlayerbotMgr->GetWaitForAttackSeconds();
    return (time(nullptr) - _combatStartTime) < time_t(waitSec);
}

bool PlayerbotAI::ShouldFollowPublic() const
{
    if (!_bot || _clientControlled || _stay)
        return false;
    if (HasEngageTarget())
        return false;
    Group* group = _bot->GetGroup();
    if (!group)
        return false;
    uint64 const leaderGuid = group->GetLeaderGUID();
    return leaderGuid && leaderGuid != _bot->GetGUID();
}

bool PlayerbotAI::NeedsRestPublic() const
{
    if (!_bot)
        return false;
    // Never report rest need while fighting — self-bot was sitting mid-pull when
    // mana dipped and IsInCombat alone flickered false between casts.
    if (_bot->IsInCombat() || GroupInCombat() || !_bot->getAttackers().empty())
        return false;
    if (HasEngageTarget())
        return false;
    if (_forceRest || _resting)
        return true;
    float const hpPct = HealthPct();
    float const manaPct = ManaPct();
    bool const needHp = hpPct < sPlayerbotMgr->GetRestHealthPct();
    bool const needMana = UsesMana() && manaPct < sPlayerbotMgr->GetRestManaPct();
    return needHp || needMana || HasFoodOrDrinkAura();
}

void PlayerbotAI::UpdateAI(uint32 diff)
{
    if (!_bot || !_bot->IsInWorld())
        return;

    // Invites / loot rolls / rez accepts must react immediately — they time out
    // (or stall forever for bots) if only handled on the coarse AI interval.
    HandlePendingInvites();
    HandleLootRolls();
    if (!_bot->IsAlive())
    {
        TryAcceptResurrect();
        return;
    }

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

    HandleInteractions();

    // Rez dead party members before buffs / combat (OOC rez or battle-rez).
    if (HandleResurrect())
        return;

    // Keep raid/self buffs topped up out of combat when not mid-cast.
    if (!_bot->IsInCombat() && _bot->getAttackers().empty()
        && !_bot->HasUnitState(UNIT_STATE_CASTING)
        && BotRotation::TryMaintainBuffs(_bot))
        return;

    // AC-style Trigger → Action → Queue. MoP rotations run inside the combat action.
    if (_aiEngine && _aiEngine->DoNextAction())
        return;

    // Fallback if the engine queued nothing useful (should be rare).
    if (_clientControlled)
    {
        if (HandleCombatCastOnly())
            return;
        // Self-bot: never auto-drink while combat is active (engine RestAction
        // is also gated; keep the fallback aligned).
        if (!_bot->IsInCombat() && !GroupInCombat() && _bot->getAttackers().empty())
            HandleRest();
        return;
    }

    if (HandleCombat())
        return;
    if (HandleRest())
        return;
    if (_stay)
    {
        HandleStay();
        HandleVendor();
        return;
    }
    if (ShouldFollowPublic())
        HandleFollow();
    else
    {
        if (HandleLoot())
            return;
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
    // Drop refreshment the moment combat is relevant — do not stay seated.
    if (_bot->IsInCombat() || GroupInCombat() || !_bot->getAttackers().empty())
    {
        if (_resting || _bot->IsSitState() || HasFoodOrDrinkAura())
            StopResting();
    }

    // Track group/self combat start for wait-for-attack (AC WaitForAttackStrategy).
    if (_bot->IsInCombat() || GroupInCombat())
    {
        if (!_combatStartTime)
            _combatStartTime = time(nullptr);
    }
    else if (_combatStartTime)
    {
        _combatStartTime = 0;
        _targets.OnCombatEnded();
    }

    // Non-tanks with +wait for attack hold DPS until the tank has threat time.
    if (ShouldWaitForAttack())
    {
        if (!_clientControlled && !_stay)
            HandleFollow();
        return true;
    }

    // Between pulls: sit/drink with the party instead of chasing the next pack.
    // Explicit attack/pull orders always engage — don't stall for drinks.
    if (!HasEngageTarget() && !_bot->IsInCombat() && !GroupInCombat() && _food && PartyNeedsRest())
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

        // Nothing to fight: drop lingering attack/chase/selection so we don't
        // keep facing a corpse like a stuck loot attempt.
        if (_bot->GetVictim())
            _bot->AttackStop();
        if (Unit* selected = _bot->GetSelectedUnit())
            if (!selected->IsAlive())
                _bot->SetSelection(0);
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
                if (BotMovement::MoveChase(_bot, target, 0.0f))
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
    bool const casting = _bot->IsNonMeleeSpellCasted(false)
        || _bot->HasUnitState(UNIT_STATE_CASTING);

    // Ensure we are not in a melee-attack state (chase _reachTarget can force it).
    if (_bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING))
    {
        _bot->ClearUnitState(UNIT_STATE_MELEE_ATTACKING);
        _bot->SendMeleeAttackStop(target);
    }
    _bot->Attack(target, false);

    if (inRange && hasLos)
    {
        // Never StopMoving / Clear while casting — that interrupts the spell.
        if (!casting)
        {
            BotMovement::StopAndIdle(_bot);
            _bot->SetSelection(target->GetGUID());
            BotMovement::FaceUnit(_bot, target);
        }

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
    // Drop refreshment as soon as combat is relevant.
    if (_bot->IsInCombat() || GroupInCombat() || !_bot->getAttackers().empty())
    {
        if (_resting || _bot->IsSitState() || HasFoodOrDrinkAura())
            StopResting();
    }

    // Seated / mid-cast OOC: never clear rest or start a rotation — that cancelled
    // clicked food/drink and wiped eat/drink state every AI tick.
    if (!_bot->IsInCombat() && _bot->getAttackers().empty() && !GroupInCombat())
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

// Target priority (AC Values: pull / tank / rti / dps / current):
//   1) Pull / forced command target
//   2) Tanks: peel mobs off healers/party, then hold the pack
//   3) Own attackers
//   4) Group combat: RTI mark → least-HP dps assist → tank victim
//   5) Grind (explicit) / self-bot selected unit
Unit* PlayerbotAI::SelectTarget()
{
    if (Unit* pull = _targets.GetPullTarget(this))
        return pull;
    if (Unit* forced = GetForcedTarget())
    {
        _targets.SetCurrentTarget(forced);
        return forced;
    }

    // Tanks in tank-mode own peel / pack selection.
    if (GetCombatRole() == CombatRole::Tank && _tankMode)
        if (Unit* tankTarget = _targets.GetTankTarget(this))
        {
            _targets.SetCurrentTarget(tankTarget);
            return tankTarget;
        }

    for (Unit* attacker : _bot->getAttackers())
        if (attacker && attacker->IsAlive() && _bot->IsValidAttackTarget(attacker))
        {
            _holdAssist = false; // got aggro — fight back
            _targets.SetCurrentTarget(attacker);
            return attacker;
        }

    // Passive bots only fight back; they do not assist or pull.
    if (_passive)
        return nullptr;

    // @tank attack: non-tanks hold until a mob is actually swinging on the party.
    if (_holdAssist)
    {
        if (SelectGroupThreatTarget())
            _holdAssist = false;
        else
            return nullptr;
    }

    // AC dps assist: damage bots without +dps assist only fight forced/own aggro.
    bool const canAssist = (GetCombatRole() != CombatRole::Damage) || _dpsAssist || _grind;

    if (canAssist)
    {
        // Prefer the configured raid-target icon (default skull) for focus fire.
        if (Unit* rti = _targets.GetRtiTarget(this))
        {
            _targets.SetCurrentTarget(rti);
            return rti;
        }
        if (Unit* dps = _targets.GetDpsTarget(this))
        {
            _targets.SetCurrentTarget(dps);
            return dps;
        }
        if (Unit* tankAssist = _targets.GetAssistTankTarget(this))
        {
            _targets.SetCurrentTarget(tankAssist);
            return tankAssist;
        }
    }

    if (_grind)
    {
        Map* map = _bot->GetMap();
        bool const inInstance = map && map->IsInstance();
        if (!inInstance && !PartyNeedsRest())
            if (Unit* grind = SelectGrindTarget())
            {
                _targets.SetCurrentTarget(grind);
                return grind;
            }
    }

    // Self-bot only: the real player's selected unit while they are in combat.
    if (_clientControlled && _bot->IsInCombat())
    {
        if (Unit* selected = _bot->GetSelectedUnit())
            if (selected->IsAlive() && _bot->IsValidAttackTarget(selected))
            {
                _targets.SetCurrentTarget(selected);
                return selected;
            }
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
    _targets.SetPullTarget(target);
}

void PlayerbotAI::ClearForcedTarget()
{
    _forcedTargetGuid = 0;
    _targets.SetPullTarget(nullptr);
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
    if (!target || !target->IsAlive())
        return;

    if (_bot->HasUnitState(UNIT_STATE_CASTING) || _bot->IsNonMeleeSpellCasted(false))
        return;

    // DPS threat strategy: pause damage if we are about to rip aggro from the tank.
    if (ShouldThrottleThreat(target))
        return;

    // Interrupts first. Keep raid/self buffs up, then sync trinkets to burst.
    if (BotRotation::TryInterrupt(_bot, target))
        return;
    if (BotRotation::TryMaintainBuffs(_bot))
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

bool PlayerbotAI::TryAcceptResurrect()
{
    if (!_bot || _bot->IsAlive())
        return false;
    // Real self-bots see the rez popup on their client — leave the click to them.
    if (_clientControlled)
        return false;
    if (!_bot->IsRessurectRequested())
        return false;

    _bot->ResurectUsingRequestData();

    // ResurectUsingRequestData teleports to the caster, then schedules
    // DELAYED_RESURRECT_PLAYER while IsBeingTeleported(). Socketless bots never
    // send the teleport ack, so finalize now (same pattern as LFG/summon).
    if (WorldSession* session = _bot->GetSession())
    {
        if (session->IsBot() && _bot->IsBeingTeleported())
            session->FinalizeBotTeleport();
    }

    return _bot->IsAlive();
}

bool PlayerbotAI::HandleResurrect()
{
    if (_bot->HasUnitState(UNIT_STATE_CASTING) || _bot->IsNonMeleeSpellCasted(false))
        return true;

    Player* dead = BotRotation::FindPartyMemberToResurrect(_bot);
    if (!dead)
        return false;

    uint32 rezId = BotRotation::SelectResurrectSpell(_bot);
    if (!rezId)
        return false;

    constexpr float REZ_RANGE = 30.0f;
    if (!_bot->IsWithinDistInMap(dead, REZ_RANGE))
    {
        if (!_clientControlled)
        {
            _bot->GetMotionMaster()->Clear();
            _bot->GetMotionMaster()->MoveChase(dead, 0.0f);
            _chaseGuid = dead->GetGUID();
        }
        return true;
    }

    if (!_clientControlled && !_bot->IsWithinMeleeRange(dead))
        _bot->StopMoving();

    _bot->SetSelection(dead->GetGUID());
    return BotRotation::CastHealSpell(_bot, dead, rezId);
}

bool PlayerbotAI::HandleHealing()
{
    if (_bot->HasUnitState(UNIT_STATE_CASTING) || _bot->IsNonMeleeSpellCasted(false))
        return true;

    // Kick / racial / trinket before heals when a hostile is available.
    if (BotRotation::TryMaintainBuffs(_bot))
        return true;

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

    // Fallback only — prefer SelectNextHeal. No big heals above ~65%.
    bool const urgent = lowestPct < 40.0f;
    bool const big = lowestPct < 65.0f;
    bool const topOff = lowestPct < 85.0f;
    if (!topOff)
        return 0;

    switch (_bot->getClass())
    {
        case CLASS_PALADIN:
            if (urgent && BotRotation::SpellReady(_bot, 19750))
                return 19750; // Flash of Light
            if (BotRotation::SpellReady(_bot, 20473))
                return 20473; // Holy Shock
            if (big && BotRotation::SpellReady(_bot, 635))
                return 635; // Holy Light
            return 0;
        case CLASS_PRIEST:
            if (urgent && BotRotation::SpellReady(_bot, 2061))
                return 2061; // Flash Heal
            if (big && BotRotation::SpellReady(_bot, 2060))
                return 2060; // Heal / Greater Heal
            if (BotRotation::SpellReady(_bot, 139))
                return 139; // Renew
            return 0;
        case CLASS_SHAMAN:
            if (urgent && BotRotation::SpellReady(_bot, 8004))
                return 8004; // Healing Surge
            if (big && BotRotation::SpellReady(_bot, 77472))
                return 77472; // Greater Healing Wave
            if (topOff && BotRotation::SpellReady(_bot, 331))
                return 331; // Healing Wave
            return 0;
        case CLASS_DRUID:
            if (urgent && BotRotation::SpellReady(_bot, 8936))
                return 8936; // Regrowth
            if (big && BotRotation::SpellReady(_bot, 5185))
                return 5185; // Healing Touch
            if (BotRotation::SpellReady(_bot, 774))
                return 774; // Rejuvenation
            return 0;
        case CLASS_MONK:
            if (big && BotRotation::SpellReady(_bot, 116694))
                return 116694; // Surging Mist
            return BotRotation::SpellReady(_bot, 115175) ? 115175 : 0; // Soothing Mist
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
        if (!corpse || corpse->IsAlive()
            || !HasTakeableLoot(_bot, corpse, &_lootBagFullSkip, &_lootRollOpened))
        {
            _lootGuid = 0;
            return false;
        }

        // Do not open while a real player is already looking at this corpse.
        Player* leader = nullptr;
        if (Group* group = _bot->GetGroup())
        {
            if (uint64 const leaderGuid = group->GetLeaderGUID())
                if (leaderGuid != _bot->GetGUID())
                    leader = ObjectAccessor::FindPlayer(leaderGuid);
        }
        if (leader && leader->GetLootGUID() == corpse->GetGUID())
            return true;

        if (!_bot->IsWithinDistInMap(corpse, INTERACTION_DISTANCE))
        {
            if (_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                MoveToPosition(corpse->GetPositionX(), corpse->GetPositionY(), corpse->GetPositionZ());
            return true;
        }

        // In range: open loot (starts group rolls), take free loot that fits.
        // Never StoreLootItem on roll-blocked slots — that calls SendLootRelease
        // and aborts Need/Greed.
        uint64 const corpseGuid = corpse->GetGUID();
        _bot->SendLoot(corpseGuid, LootType::LOOT_CORPSE);
        if (HasPendingLootRolls(&corpse->loot))
            _lootRollOpened.insert(corpseGuid);

        Loot* loot = &corpse->loot;
        uint32 maxSlot = loot->GetMaxSlotInLootFor(_bot);
        bool storedOrTookGold = false;
        bool sawFreeItem = false;
        bool bagFull = false;
        for (uint32 slot = 0; slot < maxSlot; ++slot)
        {
            LootItem* item = loot->LootItemInSlot(slot, _bot);
            if (!item || item->is_blocked)
                continue;
            sawFreeItem = true;
            if (!CanFitLootItem(_bot, item->itemid, item->count))
            {
                bagFull = true;
                continue;
            }
            _bot->StoreLootItem(uint8(slot), loot, corpseGuid);
            storedOrTookGold = true;
        }

        if (loot->gold)
        {
            WorldPacket money(CMSG_LOOT_MONEY);
            _bot->GetSession()->HandleLootMoneyOpcode(money);
            storedOrTookGold = true;
        }

        _bot->GetSession()->DoLootRelease(corpseGuid);

        // Full bags and nothing stored: do not keep pathing back to this corpse.
        // Rolls were opened above if needed; HandleLootRolls covers voting.
        if (bagFull && !storedOrTookGold && sawFreeItem)
            _lootBagFullSkip.insert(corpseGuid);
        else if (storedOrTookGold)
            _lootBagFullSkip.erase(corpseGuid);

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
    BotLootCreatureCheck check(_bot, BOT_LOOT_SEEK_DIST, &_lootBagFullSkip, &_lootRollOpened);
    Skyfire::CreatureListSearcher<BotLootCreatureCheck> searcher(_bot, corpses, check);
    _bot->VisitNearbyGridObject(BOT_LOOT_SEEK_DIST, searcher);

    // Prefer corpses near the group leader so bots don't march off to distant
    // sparkles while follow keeps yanking them back.
    Player* leader = nullptr;
    if (Group* group = _bot->GetGroup())
    {
        uint64 const leaderGuid = group->GetLeaderGUID();
        if (leaderGuid && leaderGuid != _bot->GetGUID())
            leader = ObjectAccessor::FindPlayer(leaderGuid);
    }

    Creature* best = nullptr;
    float bestDist = BOT_LOOT_SEEK_DIST + 1.0f;
    for (Creature* c : corpses)
    {
        if (leader && leader->IsInWorld() && leader->GetMap() == _bot->GetMap())
        {
            if (leader->GetDistance(c) > BOT_LOOT_LEADER_RADIUS)
                continue;
        }
        float d = _bot->GetDistance(c);
        if (d < bestDist)
        {
            bestDist = d;
            best = c;
        }
    }
    return best;
}

bool PlayerbotAI::HasNearbyLootPublic() const
{
    if (!_bot || !_loot || _clientControlled)
        return false;
    if (_bot->IsInCombat() || GroupInCombat())
        return false;
    if (_lootGuid)
        return true;
    return const_cast<PlayerbotAI*>(this)->FindNearbyLoot() != nullptr;
}

bool PlayerbotAI::HandleLootRolls()
{
    if (!_bot || !_bot->IsInWorld())
        return false;

    Group* group = _bot->GetGroup();
    if (!group || !group->isRollLootActive())
        return false;

    bool voted = false;
    for (Roll* roll : group->GetRolls())
    {
        if (!roll || !roll->isValid())
            continue;

        auto voteItr = roll->playerVote.find(_bot->GetGUID());
        if (voteItr == roll->playerVote.end() || voteItr->second != RollType::MAX_ROLL_TYPE)
            continue;

        RollType vote = RollType::ROLL_GREED;
        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(roll->itemid))
        {
            // Simple heuristic: greed on most loot; need on usable weapons/armor
            // the bot can equip (keeps LFG thresholds moving).
            if (proto->Class == ITEM_CLASS_WEAPON || proto->Class == ITEM_CLASS_ARMOR)
            {
                if (_bot->CanUseItem(proto) == EQUIP_ERR_OK)
                    vote = RollType::ROLL_NEED;
            }
        }

        // Master loot / FFA rolls still expect a vote so the timer clears.
        LootMethod const method = group->GetLootMethod();
        if (method == LootMethod::MASTER_LOOT || method == LootMethod::FREE_FOR_ALL)
            vote = RollType::ROLL_PASS;

        group->CountRollVote(_bot->GetGUID(), roll->itemGUID, vote);
        voted = true;
        break; // one vote per tick
    }
    return voted;
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

    // Mid-loot path: do not overwrite POINT motion with MoveFollow (that causes
    // the loot↔follow thrash when a corpse is still in seek range).
    if (_lootGuid)
        return;

    if (!leader || !leader->IsInWorld() || !leader->IsAlive())
    {
        if (_followGuid)
        {
            BotMovement::StopAndIdle(_bot);
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

    // Never interrupt casts for follow adjustments (AC Follow/ChaseCastStop pitfall).
    if (BotMovement::IsCasting(_bot))
        return;

    float const dist = _bot->GetDistance(leader);
    MovementGeneratorType const moveType = _bot->GetMotionMaster()->GetCurrentMovementGeneratorType();

    // Catch up when far / follow lost. When already in range, park Idle — do NOT
    // mirror the master's facing, and clear dead target selection.
    if (_followGuid != leaderGuid || dist > BOT_FOLLOW_DIST + 3.5f
        || (moveType != FOLLOW_MOTION_TYPE && moveType != IDLE_MOTION_TYPE && !_bot->IsStopped()))
    {
    if (BotMovement::MoveFollowLeader(_bot, leader,
            BotFormation::FollowDistance(this), BotFormation::FollowAngle(this)))
    {
        _followGuid = leaderGuid;
        _chaseGuid = 0;
    }
    }
    else if (dist <= BOT_FOLLOW_DIST + 2.5f
        && (moveType == FOLLOW_MOTION_TYPE || !_bot->IsStopped()))
    {
        BotMovement::StopAndIdle(_bot);
        BotMovement::ClearDeadSelection(_bot);
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
    // Drop food/drink before standing — leftover OBS_MOD_POWER refreshment ticks
    // were spiking displayed mana toward 100% while already in combat.
    CancelRestConsumables();
    if (_bot->getStandState() != UNIT_STAND_STATE_STAND)
        _bot->SetStandState(UNIT_STAND_STATE_STAND);
}

bool PlayerbotAI::HasFoodOrDrinkAura() const
{
    if (!_bot)
        return false;

    // Explicit refreshment aura IDs (do not require NOT_SEATED — some MoP
    // wrappers omit that flag and we must not re-cast every AI tick).
    if (_bot->HasAura(BOT_REFRESHMENT_SPELL)
        || _bot->HasAura(BOT_FOOD_AURA_SPELL)
        || _bot->HasAura(BOT_DRINK_AURA_SPELL))
        return true;

    Unit::AuraApplicationMap const& auras = _bot->GetAppliedAuras();
    for (Unit::AuraApplicationMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
    {
        AuraApplication const* app = itr->second;
        if (!app || !app->GetBase())
            continue;
        SpellInfo const* info = app->GetBase()->GetSpellInfo();
        if (!info)
            continue;
        if (!(info->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED))
            continue;
        if (info->HasAura(SPELL_AURA_OBS_MOD_HEALTH)
            || info->HasAura(SPELL_AURA_MOD_POWER_REGEN)
            || info->HasAura(SPELL_AURA_MOD_POWER_REGEN_PERCENT)
            || info->HasAura(SPELL_AURA_PERIODIC_ENERGIZE)
            || info->HasAura(SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT)
            || info->HasAura(SPELL_AURA_OBS_MOD_POWER))
            return true;
    }
    return false;
}

void PlayerbotAI::CancelRestConsumables()
{
    if (!_bot)
        return;
    // Only interrupt while seated (food/drink cast). StopResting() runs every
    // combat tick — interrupting here was canceling Lightning Bolt / heals after
    // ~1s and also canceling the player's own casts in self-bot mode.
    if (_bot->IsSitState() && _bot->IsNonMeleeSpellCasted(false))
        _bot->InterruptNonMeleeSpells(false);
    _bot->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_NOT_SEATED);
    _bot->RemoveAurasDueToSpell(BOT_REFRESHMENT_SPELL);
    _bot->RemoveAurasDueToSpell(BOT_FOOD_AURA_SPELL);
    _bot->RemoveAurasDueToSpell(BOT_DRINK_AURA_SPELL);
}

bool PlayerbotAI::CastRefreshmentSpell()
{
    if (!_bot || _bot->IsInCombat())
        return false;
    // Already eating/drinking — never re-cast (re-casts stacked regen auras and
    // dumped mana to 100% in one pulse, then HandleRest stood us up).
    if (_bot->IsNonMeleeSpellCasted(false) || HasFoodOrDrinkAura())
        return true;

    bool const needHp = HealthPct() < 98.0f;
    bool const needMana = UsesMana() && ManaPct() < 98.0f;
    if (!needHp && !needMana)
        return false;

    if (_bot->getStandState() != UNIT_STAND_STATE_SIT)
        _bot->SetStandState(UNIT_STAND_STATE_SIT);

    // Apply Food / Drink auras directly. CastSpell(false) often fails without an
    // item cast context, so self-bot / socket bots sat but never gained the aura.
    // Drink 92800 uses the core periodic-dummy → MOD_POWER_REGEN path.
    if (needHp && sSpellMgr->GetSpellInfo(BOT_FOOD_AURA_SPELL))
        _bot->AddAura(BOT_FOOD_AURA_SPELL, _bot);
    if (needMana && sSpellMgr->GetSpellInfo(BOT_DRINK_AURA_SPELL))
        _bot->AddAura(BOT_DRINK_AURA_SPELL, _bot);

    // Non-mana classes that only need HP still get Food; mana users get both.
    if (!needHp && !needMana)
        return false;

    return HasFoodOrDrinkAura();
}

void PlayerbotAI::ApplyDirectRestRegen()
{
    // Disabled: SetHealth/SetPower every AI tick fights food/drink aura ticks and
    // causes the mana bar to flash 100% ↔ real value (self-bot and socket bots).
    // Recovery is spell-based only (CastRefreshmentSpell / 128701).
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

    bool const wasSitting = _bot->IsSitState();

    // Sit so refreshment auras can start (self-bot and socket bots).
    if (_bot->getStandState() != UNIT_STAND_STATE_SIT)
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

    // Already eating/drinking — wait on the aura (no SetPower).
    if (_bot->IsNonMeleeSpellCasted(false) || HasFoodOrDrinkAura())
    {
        _resting = true;
        return true;
    }

    if (!CastRefreshmentSpell())
    {
        // Do not leave the bot seated with no regen (self-bot idle sit bug).
        _resting = false;
        if (!wasSitting && _bot->getStandState() != UNIT_STAND_STATE_STAND)
            _bot->SetStandState(UNIT_STAND_STATE_STAND);
        return false;
    }

    _resting = true;
    return true;
}

bool PlayerbotAI::HandleRest()
{
    if (!_bot)
        return false;
    // Never sit/drink while anyone in the party is fighting (self-bot healers
    // were parking mid-pull because only self-combat was checked).
    if (_bot->IsInCombat() || GroupInCombat() || !_bot->getAttackers().empty())
    {
        CancelRestConsumables();
        StopResting();
        return false;
    }

    float const hpPct = HealthPct();
    float const manaPct = ManaPct();
    bool const needHp = hpPct < sPlayerbotMgr->GetRestHealthPct();
    bool const needMana = UsesMana() && manaPct < sPlayerbotMgr->GetRestManaPct();
    bool const lowResources = needHp || needMana;
    bool const nearlyFull = hpPct >= 98.0f && (!UsesMana() || manaPct >= 98.0f);
    bool const itemResting = HasFoodOrDrinkAura() || _bot->IsNonMeleeSpellCasted(false)
        || _bot->HasAura(BOT_REFRESHMENT_SPELL);

    bool const shouldRest = _forceRest || _resting || (_food && lowResources) || itemResting;

    if (!shouldRest)
        return false;

    if (nearlyFull)
    {
        CancelRestConsumables();
        StopResting();
        return false;
    }

    return StartRefreshment();
}

std::string PlayerbotAI::FormatStrategies(bool combat) const
{
    return _strategies.Format(combat ? BotState::Combat : BotState::NonCombat);
}

bool PlayerbotAI::StrategyAllowed(bool combat, std::string const& name) const
{
    // Kept for callers; engine enforces the same gates.
    CombatRole const role = GetCombatRole();
    std::string n = name;
    if (n == "tankassist") n = "tank assist";
    if (n == "healdps" || n == "heal dps") n = "healer dps";
    if (n == "savemana") n = "save mana";
    if (n == "dpsassist") n = "dps assist";

    if (n == "passive" || n == "grind")
        return true;
    if (!combat)
        return n == "food" || n == "follow" || n == "stay" || n == "loot";
    if (role == CombatRole::Tank)
        return n == "tank" || n == "tank assist" || n == "dps";
    if (role == CombatRole::Healer)
        return n == "heal" || n == "healer dps" || n == "save mana" || n == "wait for attack";
    return n == "dps" || n == "dps assist" || n == "threat" || n == "wait for attack";
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

    auto reply = [&](std::string const& msg)
    {
        ReplyTo(from, msg);
    };

    BotState const state = combat ? BotState::Combat : BotState::NonCombat;

    if (body.empty() || body == "?")
    {
        reply(std::string(combat ? "co: " : "nc: ") + FormatStrategies(combat));
        reply(std::string(combat ? "nc: " : "co: ") + FormatStrategies(!combat));
        return true;
    }

    std::string const report = _strategies.ChangeStrategy(body, state);
    SyncFlagsFromStrategies();

    if (!_food)
        StopResting();
    if (_stay && !_clientControlled)
    {
        ClearForcedTarget();
        _bot->GetMotionMaster()->Clear();
        _bot->GetMotionMaster()->MoveIdle();
        _followGuid = 0;
        _chaseGuid = 0;
    }
    else if (!_stay)
    {
        _forceRest = false;
        if (_bot->getStandState() != UNIT_STAND_STATE_STAND)
            _bot->SetStandState(UNIT_STAND_STATE_STAND);
        _followGuid = 0;
    }
    if (!_loot)
        _lootGuid = 0;
    if (_passive)
        ClearForcedTarget();

    // "co:" / "nc:" (no space) so status replies never re-parse as commands.
    reply(std::string(combat ? "co:" : "nc:") + report);
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
        ack("Orders: stay, follow, flee, leave, summon, grind, reset, passive, aggressive, attack, tank/dps attack, pull, rti, eat/drink, maintenance. Strategies: co/nc +name,-name,~name or co?/nc?. Filters: @tank/@dps/@heal/@ranged.");
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
        _strategies.ApplyStayPack();
        SyncFlagsFromStrategies();
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
        _strategies.ApplyFollowPack();
        SyncFlagsFromStrategies();
        _forceRest = false;
        _holdAssist = false;
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
        // AC flee pack: follow + passive (don't fight while running to master).
        _strategies.ApplyFleePack();
        SyncFlagsFromStrategies();
        ClearForcedTarget();
        if (_bot->GetVictim())
            _bot->AttackStop();
        _chaseGuid = 0;
        _followGuid = 0;
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
        _strategies.ApplyFollowPack();
        SyncFlagsFromStrategies();
        _followGuid = 0;
        ack("Summoned.");
        return true;
    }

    if (cmd == "grind")
    {
        _strategies.ApplyGrindPack();
        SyncFlagsFromStrategies();
        ClearForcedTarget();
        ack("Grinding.");
        return true;
    }

    if (cmd == "reset")
    {
        _strategies.ApplyResetPack();
        SyncFlagsFromStrategies();
        _forceRest = false;
        _holdAssist = false;
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
        if (_bot->IsSitState() && _bot->IsNonMeleeSpellCasted(false))
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
        _strategies.Add("food", BotState::NonCombat);
        _strategies.Remove("grind", BotState::NonCombat);
        _strategies.Remove("grind", BotState::Combat);
        SyncFlagsFromStrategies();
        ClearForcedTarget();
        if (_bot->GetVictim())
            _bot->AttackStop();
        // Socket bots sit via StartRefreshment. Self-bot: sit only if standing so
        // an explicit eat/drink order can start; never fight an already-sitting player.
        if (_clientControlled && _bot->getStandState() != UNIT_STAND_STATE_SIT)
            _bot->SetStandState(UNIT_STAND_STATE_SIT);
        StartRefreshment();
        ack(_clientControlled
            ? "Refreshing (drink 92800 / food 104935; auto when low with nc +food; cancels at full)."
            : "Refreshing.");
        return true;
    }

    if (cmd == "heal")
    {
        std::string const report = _strategies.ChangeStrategy("+heal", BotState::Combat);
        SyncFlagsFromStrategies();
        ack(report.c_str());
        return true;
    }

    if (cmd == "healer dps" || cmd == "healdps" || cmd == "heal dps")
    {
        std::string const report = _strategies.ChangeStrategy("+healer dps", BotState::Combat);
        SyncFlagsFromStrategies();
        ack(report.c_str());
        return true;
    }

    if (cmd == "save mana" || cmd == "savemana" || cmd == "save mana on")
    {
        std::string const report = _strategies.ChangeStrategy("+save mana", BotState::Combat);
        SyncFlagsFromStrategies();
        ack(report.c_str());
        return true;
    }

    if (cmd == "save mana off" || cmd == "savemana off")
    {
        std::string const report = _strategies.ChangeStrategy("-save mana", BotState::Combat);
        SyncFlagsFromStrategies();
        ack(report.c_str());
        return true;
    }

    if (cmd == "passive")
    {
        _strategies.ApplyPassivePack();
        SyncFlagsFromStrategies();
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
        _strategies.ApplyAggressivePack();
        SyncFlagsFromStrategies();
        _holdAssist = false;
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
        _strategies.ApplyAggressivePack();
        _strategies.Remove("grind", BotState::Combat);
        _strategies.Remove("grind", BotState::NonCombat);
        // Ordered attack: don't delay this pull.
        _strategies.Remove("wait for attack", BotState::Combat);
        SyncFlagsFromStrategies();
        _holdAssist = false;
        _combatStartTime = 0;
        SetForcedTarget(target);
        StopResting();
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
        // AC TankAttackChatShortcut: -passive on both engines, then pull.
        _strategies.ChangeStrategy("-passive", BotState::NonCombat);
        _strategies.ChangeStrategy("-passive", BotState::Combat);
        _strategies.Remove("wait for attack", BotState::Combat);
        SyncFlagsFromStrategies();
        _holdAssist = false;
        _combatStartTime = 0;
        SetForcedTarget(target);
        StopResting();
        ack("Tank attacking.");
        return true;
    }

    // AC pull: tanks engage the master's target, or the configured RTI mark.
    if (cmd == "pull")
    {
        if (GetCombatRole() != CombatRole::Tank)
            return true;
        Unit* target = from->GetSelectedUnit();
        if (!target || !target->IsAlive() || !_bot->IsValidAttackTarget(target))
            target = _targets.GetRtiTarget(this);
        if (!target || !target->IsAlive() || !_bot->IsValidAttackTarget(target))
        {
            ack("No valid pull target.");
            return true;
        }
        _strategies.ChangeStrategy("-passive", BotState::NonCombat);
        _strategies.ChangeStrategy("-passive", BotState::Combat);
        _strategies.Remove("wait for attack", BotState::Combat);
        SyncFlagsFromStrategies();
        _holdAssist = false;
        _combatStartTime = 0;
        SetForcedTarget(target);
        StopResting();
        ack("Pulling.");
        return true;
    }

    // AC rti: set which raid icon bots prefer for focus fire (default skull).
    if (cmd == "rti" || cmd == "rti ?" || cmd.rfind("rti ", 0) == 0)
    {
        if (cmd == "rti" || cmd == "rti ?")
        {
            std::string reply = "rti: " + _targets.GetRti();
            ack(reply.c_str());
            return true;
        }
        std::string icon = cmd.substr(4);
        while (!icon.empty() && icon.front() == ' ')
            icon.erase(icon.begin());
        if (icon == "?" || icon.empty())
        {
            std::string reply = "rti: " + _targets.GetRti();
            ack(reply.c_str());
            return true;
        }
        if (BotTargetValues::RtiIndexFromName(icon) < 0)
        {
            ack("rti icons: star circle diamond triangle moon square cross skull");
            return true;
        }
        _targets.SetRti(icon);
        std::string reply = "rti: " + _targets.GetRti();
        ack(reply.c_str());
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
        _strategies.ApplyAggressivePack();
        _strategies.Remove("wait for attack", BotState::Combat);
        SyncFlagsFromStrategies();
        _holdAssist = false;
        _combatStartTime = 0;
        SetForcedTarget(target);
        StopResting();
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
