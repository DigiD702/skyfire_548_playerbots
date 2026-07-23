/*
 * SkyFire playerbots — AC-shaped Trigger / TriggerNode (thin port).
 */

#ifndef SF_BOT_TRIGGER_H
#define SF_BOT_TRIGGER_H

#include "BotAction.h"
#include <string>
#include <vector>

class PlayerbotAI;

class BotTrigger
{
public:
    BotTrigger(PlayerbotAI* ai, std::string name, uint32 checkIntervalMs = 500)
        : _ai(ai), _name(std::move(name)), _checkIntervalMs(checkIntervalMs), _lastCheck(0) {}
    virtual ~BotTrigger() = default;

    std::string const& GetName() const { return _name; }

    // Returns true when the trigger should fire this tick.
    virtual bool IsActive() = 0;

    bool NeedCheck(uint32 nowMs)
    {
        if (!_lastCheck || nowMs - _lastCheck >= _checkIntervalMs)
        {
            _lastCheck = nowMs;
            return true;
        }
        return false;
    }

protected:
    PlayerbotAI* _ai;
    std::string _name;
    uint32 _checkIntervalMs;
    uint32 _lastCheck;
};

class BotTriggerNode
{
public:
    BotTriggerNode(std::string name, std::vector<BotNextAction> handlers)
        : _name(std::move(name)), _handlers(std::move(handlers)), _trigger(nullptr) {}

    std::string const& GetName() const { return _name; }
    std::vector<BotNextAction> const& GetHandlers() const { return _handlers; }

    void SetTrigger(BotTrigger* t) { _trigger = t; }
    BotTrigger* GetTrigger() const { return _trigger; }

private:
    std::string _name;
    std::vector<BotNextAction> _handlers;
    BotTrigger* _trigger;
};

#endif
