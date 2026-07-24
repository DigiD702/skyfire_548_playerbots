/*
 * Thin AC-style strategy engine for SkyFire playerbots.
 *
 * Full Trigger/Action/Queue comes later. This layer owns named strategy sets
 * per BotState and ChangeStrategy(+/-/~/?) parsing so chat packs match
 * AzerothCore semantics while MoP rotations stay in rotations/.
 */

#ifndef SF_BOT_STRATEGY_ENGINE_H
#define SF_BOT_STRATEGY_ENGINE_H

#include "Define.h"
#include <set>
#include <string>
#include <vector>

enum class BotState : uint8
{
    Combat    = 0,
    NonCombat = 1
};

class BotStrategyEngine
{
public:
    void Clear();
    // cls: used for default +cc on Mage/Hunter/Warlock (0 = skip class defaults).
    void ResetToRoleDefaults(bool isTank, bool isHealer, uint8 cls = 0);

    // AC-style: "+follow,-passive,~grind,?" (comma-separated).
    // Returns a human-readable report of resulting flags for this state.
    std::string ChangeStrategy(std::string const& text, BotState state);

    bool Has(std::string const& name, BotState state) const;
    bool HasAny(BotState state) const;

    std::vector<std::string> List(BotState state) const;
    std::string Format(BotState state) const;

    void Add(std::string const& name, BotState state);
    void Remove(std::string const& name, BotState state);
    void Toggle(std::string const& name, BotState state);
    void Set(std::string const& name, BotState state, bool enabled);

    // Apply AC chat-shortcut packs (both states where AC does).
    void ApplyFollowPack();
    void ApplyStayPack();
    void ApplyFleePack();
    void ApplyGrindPack();
    void ApplyPassivePack();
    void ApplyAggressivePack();
    void ApplyResetPack();

private:
    using StrategySet = std::set<std::string>;

    StrategySet& SetFor(BotState state);
    StrategySet const& SetFor(BotState state) const;

    static std::string NormalizeName(std::string name);
    bool IsKnown(std::string const& name, BotState state) const;
    bool IsRoleAllowed(std::string const& name, BotState state, bool isTank, bool isHealer) const;

    void ApplyMutualExclusions(std::string const& name, BotState state, bool enabled);

    StrategySet _combat;
    StrategySet _nonCombat;

    bool _isTank = false;
    bool _isHealer = false;
    uint8 _cls = 0;
};

#endif
