/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*
* Playerbots module - per-bot AI controller.
*
* Phase 3 (foundation): one PlayerbotAI instance drives one bot Player, ticked
* from PlayerScript::OnUpdate. This first iteration implements safe, observable
* behaviour (auto-accepting group invites and defensive auto-attack). Movement,
* class rotations, and the strategy/action engine build on top of this.
*/

#ifndef _SF_PLAYERBOT_AI_H
#define _SF_PLAYERBOT_AI_H

#include "Define.h"

class Player;
class Unit;

class PlayerbotAI
{
public:
    explicit PlayerbotAI(Player* bot);

    void UpdateAI(uint32 diff);

    Player* GetBot() const { return _bot; }

private:
    // Behaviour steps (kept small and independent so strategies can be added).
    void HandlePendingInvites();
    bool HandleCombat();   // returns true if the bot is engaged (chasing a target)
    void HandleFollow();   // out of combat: stick with the group leader
    void TeleportToLeader(Player* leader);

    // Combat helpers.
    Unit* SelectTarget();          // own attacker, else assist the group leader
    bool IsRangedClass() const;    // caster/ranged classes hold at range
    uint32 GetFillerSpell() const;              // primary spammable attack for this class
    void DoRotation(Unit* target, uint32 spellId); // cast the class filler if ready

    Player* _bot;
    uint32 _updateTimer;

    // Movement state, used to avoid re-issuing the same generator every tick
    // (which would restart the movement and cause stutter).
    uint64 _chaseGuid;
    uint64 _followGuid;
};

#endif // _SF_PLAYERBOT_AI_H
