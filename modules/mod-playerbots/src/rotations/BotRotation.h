/*
 * Playerbots - per-spec combat rotation picker.
 *
 * Priorities are hand-ported from MoP Hekili/SimC lists (local Hekili/
 * reference). Unhandled specs return 0 so the AI falls back to fillers.
 */

#ifndef _SF_BOT_ROTATION_H
#define _SF_BOT_ROTATION_H

#include "Define.h"

class Player;
class Unit;

namespace BotRotation
{
    struct Context
    {
        Player* bot = nullptr;
        Unit* target = nullptr;
        uint32 enemies = 1;
        float targetHealthPct = 100.0f;
        int8 comboPoints = 0;
    };

    // First ready spell for the bot's active specialization, or 0.
    uint32 SelectNextSpell(Player* bot, Unit* target);

    // Cast helper: routes self-buffs to the bot, damage to the enemy.
    bool CastSpell(Player* bot, Unit* enemy, uint32 spellId);

    // Apply a fixed recommended talent spell loadout for Wave-1 DPS specs.
    void ApplyRecommendedTalents(Player* bot);

    // Helpers used by per-spec lists.
    uint32 CountNearbyEnemies(Player* bot, float range);
    float AuraRemains(Unit* unit, uint32 spellId);
    bool HasAuraUp(Unit* unit, uint32 spellId);
    uint32 AuraStacks(Unit* unit, uint32 spellId);
    bool SpellReady(Player* bot, uint32 spellId);
    bool CanTryCast(Player* bot, uint32 spellId);
}

#endif // _SF_BOT_ROTATION_H
