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
*/

#ifndef _SF_PLAYERBOT_AI_H
#define _SF_PLAYERBOT_AI_H

#include "Define.h"
#include <string>

class Player;
class Unit;
class Creature;

class PlayerbotAI
{
public:
    // clientControlled: real player keeps WASD; AI only casts (self-bot mode).
    explicit PlayerbotAI(Player* bot, bool clientControlled = false);

    void UpdateAI(uint32 diff);

    // Process a chat order from a real player (whisper or party/raid). Returns
    // true if the text was a recognized command (even if it had no effect).
    // When acknowledge is true the bot whispers a short confirmation back.
    bool HandleChatCommand(Player* from, std::string const& text, bool acknowledge = true);

    // Role filter for "@tank attack" style group orders (tank/heal/dps/ranged).
    bool MatchesRoleFilter(std::string const& filter) const;

    Player* GetBot() const { return _bot; }
    bool IsClientControlled() const { return _clientControlled; }

private:
    // Coarse combat role used by "tank attack" / "dps attack" filters.
    enum class CombatRole { Tank, Healer, Damage };

    // Behaviour steps (kept small and independent so strategies can be added).
    void HandlePendingInvites();
    void HandleLfg();      // auto-respond to LFG role checks and proposals
    uint8 ComputeLfgRole();
    void HandleInteractions(); // trade / duel accept
    bool HandleCombat();   // returns true if the bot is engaged (chasing a target)
    bool HandleCombatCastOnly(); // self-bot: target + cast, no MotionMaster
    bool HandleLoot();     // walk to / loot nearby corpses; true while busy
    void HandleFollow();   // out of combat: stick with the group leader
    void HandleWander();   // solo idle: walk to a nearby random point
    void HandleStay();     // hold current position when ordered to stay
    void HandleVendor();   // repair when near a repair NPC
    void TeleportToLeader(Player* leader);
    void TeleportToPlayer(Player* master);

    // Combat helpers.
    Unit* SelectTarget();          // forced / grind / own attacker / assist leader
    Unit* SelectGrindTarget();     // nearest attackable in range
    Unit* GetForcedTarget() const;
    void SetForcedTarget(Unit* target);
    void ClearForcedTarget();
    CombatRole GetCombatRole() const;
    bool IsRangedClass() const;    // caster/ranged stance (spec-aware for hybrids)
    uint32 GetFillerSpell() const;              // fallback filler when no spec list
    void DoRotation(Unit* target);              // pick + cast next rotation / filler spell

    // Chat-order helpers.
    void ReplyTo(Player* from, std::string const& text);

    // World-interaction helpers.
    Creature* FindNearbyLoot();
    Creature* FindNearbyRepairer();
    bool NeedsRepair() const;
    void MoveToPosition(float x, float y, float z);

    Player* _bot;
    bool _clientControlled;
    uint32 _updateTimer;

    // Movement state, used to avoid re-issuing the same generator every tick
    // (which would restart the movement and cause stutter).
    uint64 _chaseGuid;
    uint64 _followGuid;
    uint64 _lootGuid;
    uint32 _wanderTimer;

    // Chat-order state.
    bool _stay;                 // hold position instead of follow/wander
    bool _passive;              // do not assist / initiate; still retaliate
    bool _grind;                // attack nearest hostiles when not forced
    uint64 _forcedTargetGuid;   // from "attack" / "tank attack" / "dps attack"

    // LFG state, so we only answer a role check / proposal once each.
    bool _lfgRoleResponded;
    bool _lfgProposalResponded;
};

#endif // _SF_PLAYERBOT_AI_H
