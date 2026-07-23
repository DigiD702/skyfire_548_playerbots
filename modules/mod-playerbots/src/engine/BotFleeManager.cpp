/*
 * Slim combat flee / kite destination picker.
 */

#include "BotFleeManager.h"

#include "Map.h"
#include "Player.h"
#include "Unit.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr float BOT_FLEE_MIN_GAIN = 3.0f;
    constexpr float BOT_FLEE_CONTACT = 1.5f;
}

BotFleeManager::BotFleeManager(Player* bot, float maxAllowedDistance, Unit* focus)
    : _bot(bot), _maxAllowedDistance(maxAllowedDistance), _focus(focus)
{
    if (_maxAllowedDistance < 5.0f)
        _maxAllowedDistance = 5.0f;
}

void BotFleeManager::GatherEnemies(std::vector<Unit*>& out) const
{
    out.clear();
    if (!_bot)
        return;

    if (_focus && _focus->IsAlive() && _bot->IsValidAttackTarget(_focus))
        out.push_back(_focus);

    for (Unit* attacker : _bot->getAttackers())
    {
        if (!attacker || !attacker->IsAlive() || !_bot->IsValidAttackTarget(attacker))
            continue;
        bool duplicate = false;
        for (Unit* existing : out)
            if (existing == attacker)
            {
                duplicate = true;
                break;
            }
        if (!duplicate)
            out.push_back(attacker);
    }
}

void BotFleeManager::ScorePoint(FleePoint& point, std::vector<Unit*> const& enemies) const
{
    point.minDistance = -1.0f;
    point.sumDistance = 0.0f;
    for (Unit* unit : enemies)
    {
        if (!unit)
            continue;
        float const d = unit->GetDistance2d(point.x, point.y);
        point.sumDistance += d;
        if (point.minDistance < 0.0f || d < point.minDistance)
            point.minDistance = d;
    }
}

bool BotFleeManager::IsUseful() const
{
    if (!_bot)
        return false;

    std::vector<Unit*> enemies;
    GatherEnemies(enemies);
    if (enemies.empty())
        return false;

    float const startX = _bot->GetPositionX();
    float const startY = _bot->GetPositionY();
    for (Unit* unit : enemies)
    {
        if (!unit)
            continue;
        // Useful when something is in (or nearly in) melee / too-close range.
        if (_bot->IsWithinMeleeRange(unit) || unit->GetDistance2d(startX, startY) < 8.0f)
            return true;
    }
    return false;
}

bool BotFleeManager::CalculateDestination(float& rx, float& ry, float& rz) const
{
    if (!_bot)
        return false;

    std::vector<Unit*> enemies;
    GatherEnemies(enemies);
    if (enemies.empty())
        return false;

    float const botX = _bot->GetPositionX();
    float const botY = _bot->GetPositionY();
    float const botZ = _bot->GetPositionZ();

    FleePoint start;
    start.x = botX;
    start.y = botY;
    start.z = botZ;
    ScorePoint(start, enemies);

    FleePoint best;
    bool haveBest = false;

    // Prefer directions opposite the average enemy bearing.
    float avgAngle = 0.0f;
    uint32 angleCount = 0;
    for (Unit* unit : enemies)
    {
        if (!unit)
            continue;
        avgAngle += _bot->GetAngle(unit);
        ++angleCount;
    }
    if (angleCount)
        avgAngle /= float(angleCount);
    float const preferAngle = avgAngle + static_cast<float>(M_PI);

    float const distStep = std::max(2.0f, _maxAllowedDistance / 5.0f);
    for (float dist = _maxAllowedDistance; dist >= 6.0f; dist -= distStep)
    {
        for (int i = 0; i < 12; ++i)
        {
            float const angle = preferAngle + (static_cast<float>(i) * static_cast<float>(M_PI) / 6.0f);
            float x = botX + std::cos(angle) * dist;
            float y = botY + std::sin(angle) * dist;
            float z = botZ + BOT_FLEE_CONTACT;
            _bot->UpdateAllowedPositionZ(x, y, z);

            Map* map = _bot->GetMap();
            if (map && map->IsInWater(x, y, z))
                continue;
            if (!_bot->IsWithinLOS(x, y, z))
                continue;
            if (_focus && !_focus->IsWithinLOS(x, y, z))
                continue;

            FleePoint candidate;
            candidate.x = x;
            candidate.y = y;
            candidate.z = z;
            ScorePoint(candidate, enemies);

            if (candidate.minDistance < 0.0f)
                continue;
            // Must improve separation vs current position.
            if (candidate.minDistance < start.minDistance + BOT_FLEE_MIN_GAIN
                && candidate.sumDistance <= start.sumDistance)
                continue;

            if (!haveBest
                || candidate.sumDistance > best.sumDistance
                || (candidate.sumDistance == best.sumDistance
                    && candidate.minDistance > best.minDistance))
            {
                best = candidate;
                haveBest = true;
            }
        }
    }

    if (!haveBest)
        return false;

    rx = best.x;
    ry = best.y;
    rz = best.z;
    return true;
}
