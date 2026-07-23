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
#include <unordered_set>

class Player;
class Unit;
class Creature;
class BotAiEngine;

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

    // Re-apply role-default strategies (after init/spec change).
    void ResetStrategiesToRoleDefaults();

    BotStrategyEngine& GetStrategyEngine() { return _strategies; }
    BotStrategyEngine const& GetStrategyEngine() const { return _strategies; }
    bool HasStrategy(std::string const& name, BotState state) const { return _strategies.Has(name, state); }

    // --- Public hooks for BotAiEngine / Values / Formation ---
    bool RunCombat();
    bool RunCombatCastOnly();
    bool RunRest();
    void RunFollow();
    void RunStay();
    bool RunLoot();
    void RunWander();
    void RunVendor();
    bool IsGroupInCombatPublic() const;
    bool ShouldFollowPublic() const;
    bool NeedsRestPublic() const;
    bool IsForceResting() const { return _forceRest || _resting; }
    void RebuildAiEngine();

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
    BotTargetValues& GetTargetValues() { return _targets; }
    BotTargetValues const& GetTargetValues() const { return _targets; }

private:
    // Coarse combat role used by filters and strategy gating.
    enum class CombatRole { Tank, Healer, Damage };

    // Behaviour steps.
    void HandlePendingInvites();
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
    void TeleportToLeader(Player* leader);
    void TeleportToPlayer(Player* master);
    void StopResting();
    bool StartRefreshment();
    bool HasFoodOrDrinkAura() const;
    bool CastRefreshmentSpell();
    void ApplyDirectRestRegen();
    void CancelRestConsumables();
    bool PartyNeedsRest() const;
    bool HandleLootRolls();

    // AC-style co / nc strategy engine (role-gated).
    bool HandleStrategyCommand(Player* from, std::string const& cmd, bool acknowledge);
    void SyncFlagsFromStrategies();
    std::string FormatStrategies(bool combat) const;
    bool StrategyAllowed(bool combat, std::string const& name) const;

    // Combat helpers.
    Unit* SelectTarget();
    Unit* SelectGrindTarget();
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
    float HealthPct() const;
    float ManaPct() const;
    bool UsesMana() const;
    bool ShouldThrottleThreat(Unit* target) const;
    uint32 GetFillerSpell() const;
    uint32 GetTauntSpell() const;
    uint32 GetAoeThreatSpell() const;
    uint32 GetHealSpell() const;
    void DoRotation(Unit* target);
    void DoTankExtras(Unit* target);

    void ReplyTo(Player* from, std::string const& text);

    Creature* FindNearbyLoot();
    Creature* FindNearbyRepairer();
    bool NeedsRepair() const;
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

    // --- Flags synced from _strategies (procedural AI still reads these) ---
    bool _stay;          // nc stay (implies not follow)
    bool _food;          // nc food
    bool _loot;          // nc loot

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

    bool _forceRest;
    bool _resting;
    bool _holdAssist;    // @tank attack: hold until tank/party engages, then assist
    uint64 _forcedTargetGuid;

    bool _lfgRoleResponded;
    bool _lfgProposalResponded;
};

#endif // _SF_PLAYERBOT_AI_H
