/*
 * AC-style combat target Values.
 */

#include "BotTargetValues.h"
#include "PlayerbotAI.h"

#include "Group.h"
#include "GroupReference.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Unit.h"

void BotTargetValues::Clear()
{
    _pullGuid = 0;
    _currentGuid = 0;
}

void BotTargetValues::SetPullTarget(Unit* target)
{
    _pullGuid = target ? target->GetGUID() : 0;
    if (target)
        _currentGuid = _pullGuid;
}

void BotTargetValues::SetCurrentTarget(Unit* target)
{
    _currentGuid = target ? target->GetGUID() : 0;
}

void BotTargetValues::OnCombatEnded()
{
    _currentGuid = 0;
}

Unit* BotTargetValues::ResolveGuid(PlayerbotAI* ai, uint64 guid) const
{
    if (!ai || !guid)
        return nullptr;
    Player* bot = ai->GetBot();
    if (!bot)
        return nullptr;
    Unit* unit = ObjectAccessor::GetUnit(*bot, guid);
    if (!unit || !unit->IsAlive() || !bot->IsValidAttackTarget(unit))
        return nullptr;
    return unit;
}

Unit* BotTargetValues::GetPullTarget(PlayerbotAI* ai) const
{
    return ResolveGuid(ai, _pullGuid);
}

Unit* BotTargetValues::GetCurrentTarget(PlayerbotAI* ai) const
{
    return ResolveGuid(ai, _currentGuid);
}

Unit* BotTargetValues::GetDpsTarget(PlayerbotAI* ai) const
{
    // Prefer master's pull / current if still valid, else lowest-HP group threat.
    if (Unit* pull = GetPullTarget(ai))
        return pull;
    if (Unit* cur = GetCurrentTarget(ai))
        return cur;
    return ai ? ai->SelectLowestHpGroupEnemyPublic() : nullptr;
}

Unit* BotTargetValues::GetAssistTankTarget(PlayerbotAI* ai) const
{
    return ai ? ai->SelectAssistTankTargetPublic() : nullptr;
}

Unit* BotTargetValues::GetTankTarget(PlayerbotAI* ai) const
{
    return ai ? ai->SelectTankTargetPublic() : nullptr;
}
