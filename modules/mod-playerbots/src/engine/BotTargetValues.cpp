/*
 * AC-style combat target Values.
 */

#include "BotTargetValues.h"
#include "PlayerbotAI.h"

#include "Group.h"
#include "GroupReference.h"
#include "Map.h"
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

void BotTargetValues::SetRti(std::string const& iconName)
{
    if (RtiIndexFromName(iconName) >= 0)
        _rtiName = iconName;
}

int32 BotTargetValues::RtiIndexFromName(std::string const& name)
{
    if (name == "star")
        return 0;
    if (name == "circle")
        return 1;
    if (name == "diamond")
        return 2;
    if (name == "triangle")
        return 3;
    if (name == "moon")
        return 4;
    if (name == "square")
        return 5;
    if (name == "cross" || name == "x")
        return 6;
    if (name == "skull")
        return 7;
    return -1;
}

char const* BotTargetValues::RtiNameFromIndex(uint8 index)
{
    switch (index)
    {
        case 0: return "star";
        case 1: return "circle";
        case 2: return "diamond";
        case 3: return "triangle";
        case 4: return "moon";
        case 5: return "square";
        case 6: return "cross";
        case 7: return "skull";
        default: return "";
    }
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

Unit* BotTargetValues::GetRtiTarget(PlayerbotAI* ai) const
{
    if (!ai)
        return nullptr;
    Player* bot = ai->GetBot();
    if (!bot)
        return nullptr;
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    int32 const index = RtiIndexFromName(_rtiName);
    if (index < 0)
        return nullptr;

    uint64 const guid = group->GetTargetIcon(uint8(index));
    Unit* unit = ResolveGuid(ai, guid);
    if (!unit)
        return nullptr;
    if (!bot->IsInMap(unit) || !bot->IsWithinDistInMap(unit, 80.0f, false))
        return nullptr;
    return unit;
}

Unit* BotTargetValues::GetDpsTarget(PlayerbotAI* ai) const
{
    // Prefer master's pull if still valid (explicit pull command).
    if (Unit* pull = GetPullTarget(ai))
        return pull;
    if (!ai)
        return nullptr;

    // Stick to the tank's victim before sticky current / least-HP so melee DPS
    // (e.g. ret) do not peel onto chain-pull adds or random low-HP ranged mobs.
    if (Unit* tankVic = GetAssistTankTarget(ai))
        return tankVic;

    if (Unit* cur = GetCurrentTarget(ai))
        return cur;

    return ai->SelectLowestHpGroupEnemyPublic();
}

Unit* BotTargetValues::GetAssistTankTarget(PlayerbotAI* ai) const
{
    return ai ? ai->SelectAssistTankTargetPublic() : nullptr;
}

Unit* BotTargetValues::GetTankTarget(PlayerbotAI* ai) const
{
    return ai ? ai->SelectTankTargetPublic() : nullptr;
}
