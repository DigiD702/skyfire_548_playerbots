/*
 * SkyFire playerbots — Trigger/Action/Queue engine (AC Engine thin port).
 *
 * Strategies (BotStrategyEngine) decide which triggers/defaults are armed.
 * MoP spell selection stays in rotations/; this engine picks the *mode*
 * (combat / rest / follow / stay / loot / wander / travel).
 */

#ifndef SF_BOT_AI_ENGINE_H
#define SF_BOT_AI_ENGINE_H

#include "BotAction.h"
#include "BotMultiplier.h"
#include "BotQueue.h"
#include "BotStrategyEngine.h"
#include "BotTrigger.h"
#include "Define.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class PlayerbotAI;

class BotAiEngine
{
public:
    explicit BotAiEngine(PlayerbotAI* ai);
    ~BotAiEngine();

    // Rebuild trigger nodes / multipliers from the current strategy set.
    void Rebuild();

    // One AI tick: process triggers, push defaults, execute best action.
    // Returns true if an action ran (caller should skip fallbacks).
    bool DoNextAction();

    BotState GetActiveState() const { return _activeState; }

private:
    void RegisterCoreActions();
    void ProcessTriggers();
    void PushDefaultActions();
    BotAction* FindAction(std::string const& name);

    PlayerbotAI* _ai;
    BotState _activeState = BotState::NonCombat;

    BotQueue _queue;
    std::unordered_map<std::string, std::unique_ptr<BotAction>> _actions;
    std::vector<std::unique_ptr<BotTrigger>> _triggerOwners;
    std::vector<BotTriggerNode> _triggerNodes;
    std::vector<std::unique_ptr<BotMultiplier>> _multipliers;
};

#endif
