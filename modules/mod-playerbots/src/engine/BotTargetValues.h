/*
 * AC-style combat target Values (thin port).
 * pull / current / dps (least HP) / tank (peel) — used by SelectTarget.
 */

#ifndef SF_BOT_TARGET_VALUES_H
#define SF_BOT_TARGET_VALUES_H

#include "Define.h"

class PlayerbotAI;
class Unit;

class BotTargetValues
{
public:
    void Clear();
    void SetPullTarget(Unit* target);
    void SetCurrentTarget(Unit* target);
    uint64 GetPullGuid() const { return _pullGuid; }
    uint64 GetCurrentGuid() const { return _currentGuid; }

    Unit* GetPullTarget(PlayerbotAI* ai) const;
    Unit* GetCurrentTarget(PlayerbotAI* ai) const;

    // DPS assist: lowest-HP hostile already attacking the party.
    Unit* GetDpsTarget(PlayerbotAI* ai) const;
    // Tank assist: peel / pack focus (mirrors SelectTankTarget priority).
    Unit* GetTankTarget(PlayerbotAI* ai) const;
    // Tank's current victim (for DPS to stick after hold clears).
    Unit* GetAssistTankTarget(PlayerbotAI* ai) const;

    void OnCombatEnded();

private:
    Unit* ResolveGuid(PlayerbotAI* ai, uint64 guid) const;

    uint64 _pullGuid = 0;
    uint64 _currentGuid = 0;
};

#endif
