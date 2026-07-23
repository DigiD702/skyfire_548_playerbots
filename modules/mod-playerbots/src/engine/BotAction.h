/*
 * SkyFire playerbots — AC-shaped Action / NextAction (thin port).
 */

#ifndef SF_BOT_ACTION_H
#define SF_BOT_ACTION_H

#include "Define.h"
#include <string>
#include <vector>

class PlayerbotAI;

class BotNextAction
{
public:
    BotNextAction(std::string name, float relevance = 0.0f)
        : _name(std::move(name)), _relevance(relevance) {}

    std::string const& GetName() const { return _name; }
    float GetRelevance() const { return _relevance; }

private:
    std::string _name;
    float _relevance;
};

class BotAction
{
public:
    explicit BotAction(PlayerbotAI* ai, std::string name)
        : _ai(ai), _name(std::move(name)), _relevance(0.0f) {}
    virtual ~BotAction() = default;

    std::string const& GetName() const { return _name; }
    float GetRelevance() const { return _relevance; }
    void SetRelevance(float r) { _relevance = r; }

    virtual bool IsUseful() { return true; }
    virtual bool IsPossible() { return true; }
    virtual bool Execute() = 0;

protected:
    PlayerbotAI* _ai;
    std::string _name;
    float _relevance;
};

// Relevance bands (aligned with AC Strategy.h ACTION_* scale).
namespace BotRelevance
{
    constexpr float Idle     = 0.0f;
    constexpr float Default  = 5.0f;
    constexpr float Normal   = 10.0f;
    constexpr float High     = 20.0f;
    constexpr float Move     = 30.0f;
    constexpr float Rest     = 55.0f;
    constexpr float Combat   = 70.0f;
    constexpr float Emergency = 90.0f;
}

#endif
