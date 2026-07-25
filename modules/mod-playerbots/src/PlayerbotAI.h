/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*
* Playerbots module - per-bot AI controller.
*
* Phase 3 (foundation): one PlayerbotAI instance drives one bot Player, ticked
* from PlayerScript::OnUpdate. Chat orders (whisper / party / raid) set
* movement and combat modes; combat and follow build on top of those.
* Self-bots keep client movement and only run cast-only combat.
*
* Strategies (AC-style co/nc) are role-gated: tanks never take healer flags,
* healers never take tank flags, etc. Class rotations still run underneath.
*
* Decision loop: BotAiEngine (Trigger → Queue → Multiplier → Action). Spell
* selection stays in rotations/; the engine picks combat/rest/follow/stay/loot.
*/

#ifndef _SF_PLAYERBOT_AI_H
#define _SF_PLAYERBOT_AI_H

#include "engine/BotStrategyEngine.h"
#include "engine/BotTargetValues.h"
#include "Define.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Player;
class Unit;
class Creature;
class BotAiEngine;
class SpellInfo;
class Item;
class Quest;
struct MountCapabilityEntry;

class PlayerbotAI
{
public:
    // clientControlled: real player keeps WASD; AI only casts (self-bot mode).
    explicit PlayerbotAI(Player* bot, bool clientControlled = false);
    ~PlayerbotAI();

    void UpdateAI(uint32 diff);

    // Process a chat order from a real player (whisper or party/raid). Returns
    // true if the text was a recognized command (even if it had no effect).
    // When acknowledge is true the bot whispers a short confirmation back.
    bool HandleChatCommand(Player* from, std::string const& text, bool acknowledge = true);

    // Role filter for "@tank attack" style group orders (tank/heal/dps/ranged).
    bool MatchesRoleFilter(std::string const& filter) const;
    void SetHoldAssist(bool hold) { _holdAssist = hold; }
    bool IsHoldAssist() const { return _holdAssist; }

    // LFG role mask (tank/healer/damage) from the bot's active specialization.
    uint8 ComputeLfgRole();

    // Auto-respond to LFG role checks and proposals. Must run on the world
    // thread (PlayerbotMgr::Update) — UpdateProposal creates groups/teleports
    // and is not safe from Map::Update worker threads.
    void HandleLfg();

    Player* GetBot() const { return _bot; }
    bool IsClientControlled() const { return _clientControlled; }

    // Copy a real player's MoP phase-id set onto this bot and refresh that
    // player's client visibility of the bot (needed when not in a group).
    void SyncPhaseWithMaster(Unit* master);

    // Re-apply role-default strategies (after init/spec change).
    void ResetStrategiesToRoleDefaults();
    // After teleport/init: clear stale pathing and restore open-world activity
    // for ungrouped random bots (ResetStrategies alone leaves them on follow).
    void AfterInitRelocate(bool didTeleport);
    // Sync procedural flags after ChangeStrategy from outside the AI.
    void SyncFlagsFromStrategies();

    BotStrategyEngine& GetStrategyEngine() { return _strategies; }
    BotStrategyEngine const& GetStrategyEngine() const { return _strategies; }
    bool HasStrategy(std::string const& name, BotState state) const { return _strategies.Has(name, state); }

    bool IsAoeEnabled() const { return _aoe; }
    bool IsBoostEnabled() const { return _boost; }
    bool IsCcEnabled() const { return _cc; }
    bool IsAvoidAoeEnabled() const { return _avoidAoe; }

    // --- Public hooks for BotAiEngine / Values / Formation ---
    bool RunCombat();
    bool RunCombatCastOnly();
    bool RunRest();
    void RunFollow();
    void RunStay();
    bool RunLoot();
    void RunWander();
    void RunVendor();
    bool RunTravel();
    bool NeedsVendorWorkPublic() const;
    bool NeedsUrgentVendorPublic() const;
    bool HasQuestsStrategyPublic() const { return _quests; }
    // Grind or auto-quest: allow OOC combat ticks to pull (needed when mobs
    // will not aggro a high-level bot in a starter zone).
    bool WantsOpenWorldPullsPublic() const { return _grind || _quests; }
    bool IsGroupInCombatPublic() const;
    bool ShouldFollowPublic() const;
    bool NeedsRestPublic() const;
    bool IsForceResting() const { return _forceRest || _resting; }
    // Used by questgiver grid search — skip empty/bugged QUESTGIVER NPCs.
    bool QuestgiverHasUsefulWork(Creature const* npc) const;
    bool IsQuestNpcIgnored(uint32 entry) const;
    void IgnoreQuestNpc(uint32 entry, uint32 durationMs = 0);
    void RebuildAiEngine();

    // Same-map destination travel (chat go / position).
    bool HasTravelDestination() const { return _travel.active; }
    bool SetTravelDestination(uint32 mapId, float x, float y, float z);
    void ClearTravelDestination();
    bool IsTravelArrived(float tol = 3.0f) const;

    Unit* SelectLowestHpGroupEnemyPublic();
    Unit* SelectAssistTankTargetPublic();
    Unit* SelectTankTargetPublic();
    // 0=tank, 1=healer, 2=damage
    int GetCombatRolePublic() const;
    bool IsRangedClassPublic() const;
    bool ShouldWaitForAttack() const;
    // True when attack / tank attack / pull target is set — engage even while OOC.
    bool HasEngageTarget() const;
    bool HasNearbyLootPublic() const;
    // Safe wrapper: never call IsValidAttackTarget when either side lacks a map
    // (GetMap() asserts) — e.g. mid-teleport bots or stale selected units.
    bool IsSafeAttackTarget(Unit const* target) const;
    BotTargetValues& GetTargetValues() { return _targets; }
    BotTargetValues const& GetTargetValues() const { return _targets; }

private:
    // Coarse combat role used by filters and strategy gating.
    enum class CombatRole { Tank, Healer, Damage };

    struct TravelPoint
    {
        uint32 mapId = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct TravelDest : TravelPoint
    {
        bool active = false;
    };

    // Behaviour steps.
    void HandlePendingInvites();
    // On join: follow pack (-quests/-grind). On leave (random bots): restore +quests/+grind.
    void SyncPartyStrategies();
    void HandleInteractions();
    bool HandleCombat();
    bool HandleCombatCastOnly();
    bool HandleHealing();
    bool TryAcceptResurrect();
    bool HandleResurrect();
    bool HandleRest();
    bool HandleLoot();
    void HandleFollow();
    void HandleWander();
    void HandleStay();
    void HandleVendor();
    bool HandleQuestNpcs();
    bool HandleAutoQuesting();
    bool TryGossipForVendorOrQuest(Creature* npc, bool wantVendor, bool wantQuest);
    bool SelectGossipOption(Creature* npc, uint32 gossipListId);
    uint32 BestRewardIndex(Quest const* quest) const;
    bool TravelToVendorHub(bool preferRepair);
    bool TravelToQuestObjective();
    bool TravelToNearbyQuestgiver();
    uint32 CountFreeBagSlots() const;
    bool ShouldVendorDumpItem(Item* item) const;
    bool HasVendorJunk() const;
    uint32 SellVendorJunk(Creature* vendor);
    enum class BotMountKind : uint8 { None = 0, Ground, SwiftGround, Flying, SwiftFlying };
    static BotMountKind ClassifyMountSpell(SpellInfo const* info);
    static BotMountKind ClassifyUnitMount(Unit const* unit);
    static bool MountSpellCanFly(SpellInfo const* info);
    static float GetMountSpellSpeedBonus(SpellInfo const* info);
    static bool MountCapabilityIsFlight(MountCapabilityEntry const* cap);
    static MountCapabilityEntry const* FindBotMountCapability(Player const* bot, uint32 mountType, bool preferFlight);
    uint32 FindMountSpell(BotMountKind preferred) const;
    bool TryMount(BotMountKind preferred = BotMountKind::None);
    void TryDismount();
    void SyncMountWithMaster();
    // MoP visibility uses phase-id sets. FinalizeBotTeleport → UpdateAreaPhase
    // applies the bot's own quest conditions and can desync from nearby players
    // (visible only with .gm on / in a group). Re-align to a nearby real player.
    void EnsureVisiblePhases();
    // True when master is clearly above terrain (not skimming ground on a flyer).
    static bool IsMasterAirborne(Unit const* master);
    void ClampBotToGround();
    // True flight state for clients: CAN_FLY + DISABLE_GRAVITY + FLYING (+ move update).
    bool EnsureFlightMountCapability();
    // Ground SpeedModSpell for the current mount (flyer on the ground included).
    bool EnsureGroundMountCapability();
    void SetBotFlyingMovement(bool enable);
    void TeleportToLeader(Player* leader);
    void TeleportToPlayer(Player* master);
    void StopResting();
    bool StartRefreshment();
    bool HasFoodOrDrinkAura() const;
    static bool MemberHasFoodOrDrinkAura(Player const* player);
    bool CastRefreshmentSpell();
    void ApplyDirectRestRegen();
    void CancelRestConsumables();
    bool PartyNeedsRest() const;
    bool PartyNotAlmostReady() const;
    // True when the group leader is parked nearby — safe to drink / hold for allies.
    // False while the master is moving or far ahead (keep following; no convoy stutter).
    bool IsMasterWaitingForRest() const;
    bool HandleLootRolls();

    // AC-style co / nc strategy engine (role-gated).
    bool HandleStrategyCommand(Player* from, std::string const& cmd, bool acknowledge);
    std::string FormatStrategies(bool combat) const;
    bool StrategyAllowed(bool combat, std::string const& name) const;

    // Combat helpers.
    Unit* SelectTarget();
    Unit* SelectGrindTarget();
    Unit* SelectQuestObjectiveTarget();
    Unit* SelectTankTarget();
    Unit* SelectGroupThreatTarget();
    Unit* SelectAssistTankTarget();
    Unit* SelectLowestHpGroupEnemy();
    Player* SelectHealTarget();
    Unit* GetForcedTarget() const;
    void SetForcedTarget(Unit* target);
    void ClearForcedTarget();
    CombatRole GetCombatRole() const;
    bool IsRangedClass() const;
    bool GroupInCombat() const;
    // True when this hostile is currently beating on a non-tank group ally.
    bool IsUrgentTankPeel(Unit* target) const;
    int ScoreTankPeelMember(Player* member) const;
    float HealthPct() const;
    float ManaPct() const;
    bool UsesMana() const;
    bool ShouldThrottleThreat(Unit* target) const;
    uint32 GetFillerSpell() const;
    uint32 GetTauntSpell() const;
    uint32 GetAoeThreatSpell() const;
    uint32 GetPullOpenerSpell(Unit* target) const;
    float GetPullOpenerRange(Unit* target) const;
    bool HandlePullSequence();
    void BeginPullSequence(Unit* target);
    void EndPullSequence(bool keepForcedTarget);
    bool TryCombatFlee(Unit* focus);
    // Dungeon + tank: maintain cast range. Solo/open-world: stand ground above 30% HP.
    bool WantsRangedKiting() const;
    bool TryAvoidAoe();
    bool TryAutoMarkRti();
    // Healer/ranged: stand at cast range from focus — never master formation in combat.
    void HoldRangedCombatPosition(Unit* focus, float maxRange);
    Unit* SelectHealerCombatAnchor();
    uint32 GetHealSpell() const;
    void DoRotation(Unit* target);
    void DoTankExtras(Unit* target, bool closing = false);

    void ReplyTo(Player* from, std::string const& text);
    // Whisper class/spec combat spells the bot actually knows (debug / verify init).
    void ReportClassSpells(Player* from);
    bool BeginTravelTo(float x, float y, float z, Player* from, bool acknowledge);
    bool HandleGoCommand(Player* from, std::string const& args, bool acknowledge);
    bool HandlePositionCommand(Player* from, std::string const& args, bool acknowledge);

    Creature* FindNearbyLoot();
    Creature* FindNearbyRepairer();
    Creature* FindNearbyVendor();
    Creature* FindNearbyQuestgiver();
    bool NeedsRepair() const;
    bool HasGrayJunk() const;
    uint32 SellGrayJunk(Creature* vendor);
    void MoveToPosition(float x, float y, float z);

    Player* _bot;
    bool _clientControlled;
    uint32 _updateTimer;

    uint64 _chaseGuid;
    uint64 _followGuid;
    uint64 _lootGuid;
    uint32 _wanderTimer;
    // Corpses skipped after a full-bag loot attempt (still open once for rolls).
    std::unordered_set<uint64> _lootBagFullSkip;
    // Corpses we already opened so group rolls could start.
    std::unordered_set<uint64> _lootRollOpened;

    BotStrategyEngine _strategies;
    std::unique_ptr<BotAiEngine> _aiEngine;
    BotTargetValues _targets;
    time_t _combatStartTime = 0;
    time_t _lastCombatFlee = 0;
    time_t _pullStartTime = 0;
    enum class PullPhase : uint8 { None, Reach, Opener };
    PullPhase _pullPhase = PullPhase::None;

    TravelDest _travel;
    uint8 _travelFailCount = 0;
    std::unordered_map<std::string, TravelPoint> _savedPositions;
    bool _forceVendor = false;
    bool _forceSellAll = false;
    bool _forceQuest = false;
    // creature_template.entry → getMSTime() expiry for empty/bugged questgivers.
    std::unordered_map<uint32, uint32> _ignoredQuestNpcEntries;

    // --- Flags synced from _strategies (procedural AI still reads these) ---
    bool _stay;          // nc stay (implies not follow)
    bool _food;          // nc food
    bool _loot;          // nc loot
    bool _quests;        // nc quests (open-world auto-quest)

    bool _passive;       // co/nc passive
    bool _grind;         // co/nc grind

    bool _tankMode;      // tank role: peel + hold threat (off = play as DPS)
    bool _tankAssist;    // tank role: peel allies
    bool _dpsMode;       // damage role marker / tank-as-dps
    bool _dpsAssist;     // damage: assist party (AC dps assist)
    bool _threat;        // damage: throttle when high on threat
    bool _healerDps;     // healer: damage when nobody needs heals
    bool _saveMana;      // healer: efficient heals when low mana
    bool _waitForAttack; // non-tanks delay DPS after combat starts
    bool _aoe;           // multi-target rotation branches
    bool _boost;         // trinkets + offensive racials
    bool _cc;            // crowd control utilities
    bool _avoidAoe;      // step out of damaging ground effects

    bool _forceRest;
    bool _resting;
    bool _holdAssist;    // @tank attack: hold until tank/party engages, then assist
    uint64 _forcedTargetGuid;

    bool _lfgRoleResponded;
    bool _lfgProposalResponded;
    bool _wasGrouped;    // detect invite/leave for strategy packs
};

#endif // _SF_PLAYERBOT_AI_H
