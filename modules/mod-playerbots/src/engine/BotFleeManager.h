/*
 * Slim combat flee / kite helper (AC FleeManager concept without TravelMgr).
 * Samples points away from nearby hostiles and picks the one maximizing
 * distance-from-enemies while staying in LoS of the bot (and optional focus).
 */

#ifndef SF_BOT_FLEE_MANAGER_H
#define SF_BOT_FLEE_MANAGER_H

#include "Define.h"
#include <vector>

class Player;
class Unit;

class BotFleeManager
{
public:
    BotFleeManager(Player* bot, float maxAllowedDistance, Unit* focus = nullptr);

    bool IsUseful() const;
    bool CalculateDestination(float& rx, float& ry, float& rz) const;

private:
    struct FleePoint
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float sumDistance = 0.0f;
        float minDistance = 0.0f;
    };

    void GatherEnemies(std::vector<Unit*>& out) const;
    void ScorePoint(FleePoint& point, std::vector<Unit*> const& enemies) const;

    Player* _bot = nullptr;
    float _maxAllowedDistance = 20.0f;
    Unit* _focus = nullptr;
};

#endif
