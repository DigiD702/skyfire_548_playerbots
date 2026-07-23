/*
 * SkyFire playerbots — Multiplier (AC PassiveMultiplier thin port).
 */

#ifndef SF_BOT_MULTIPLIER_H
#define SF_BOT_MULTIPLIER_H

#include "BotAction.h"
#include <string>

class PlayerbotAI;

class BotMultiplier
{
public:
    BotMultiplier(PlayerbotAI* ai, std::string name) : _ai(ai), _name(std::move(name)) {}
    virtual ~BotMultiplier() = default;

    std::string const& GetName() const { return _name; }
    virtual float GetValue(BotAction* action) = 0;

protected:
    PlayerbotAI* _ai;
    std::string _name;
};

// AC PassiveMultiplier: zeroes almost all actions; allows follow/stay/rest.
class BotPassiveMultiplier : public BotMultiplier
{
public:
    explicit BotPassiveMultiplier(PlayerbotAI* ai) : BotMultiplier(ai, "passive") {}
    float GetValue(BotAction* action) override;
};

#endif
