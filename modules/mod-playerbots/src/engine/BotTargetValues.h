/*
 * AC-style combat target Values (thin port).
 * pull / current / rti / dps (least HP) / tank (peel) — used by SelectTarget.
 */

#ifndef SF_BOT_TARGET_VALUES_H
#define SF_BOT_TARGET_VALUES_H

#include "Define.h"
#include <string>

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

    // Raid-target icon preference (AC "rti"); default skull.
    void SetRti(std::string const& iconName);
    std::string const& GetRti() const { return _rtiName; }
    static int32 RtiIndexFromName(std::string const& name);
    static char const* RtiNameFromIndex(uint8 index);

    Unit* GetPullTarget(PlayerbotAI* ai) const;
    Unit* GetCurrentTarget(PlayerbotAI* ai) const;
    Unit* GetRtiTarget(PlayerbotAI* ai) const;

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
    std::string _rtiName = "skull";
};

#endif
