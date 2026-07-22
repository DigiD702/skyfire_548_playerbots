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

    // Healing priority context (ally-targeted spells).
    struct HealContext
    {
        Player* bot = nullptr;
        Player* healTarget = nullptr;
        float healTargetHealthPct = 100.0f;
        uint32 injuredAllies = 0;
        float lowestAllyHealthPct = 100.0f;
        float manaPct = 100.0f;
        uint32 enemies = 1;
        bool saveMana = false;
        float saveManaThreshold = 60.0f;
    };

    // First ready spell for the bot's active specialization, or 0.
    uint32 SelectNextSpell(Player* bot, Unit* target);

    // First ready heal for healer specs, or 0 (caller may fall back).
    // saveMana: skip expensive flashes/surges below saveManaThreshold unless critical.
    uint32 SelectNextHeal(Player* bot, Player* ally, bool saveMana = false,
        float saveManaThreshold = 60.0f);

    // Cast helper: routes self-buffs to the bot, damage to the enemy.
    bool CastSpell(Player* bot, Unit* enemy, uint32 spellId);

    // Cast a heal/buff: self spells on the bot, otherwise on the ally.
    bool CastHealSpell(Player* bot, Player* ally, uint32 spellId);

    // Kick -> racial -> on-use trinket. Returns true if a cast was started.
    bool TryCombatUtilities(Player* bot, Unit* enemy);
    bool TryInterrupt(Player* bot, Unit* target);
    bool TryRacial(Player* bot, Unit* targetOrSelf);
    bool TryTrinkets(Player* bot);

    // Apply a fixed recommended talent spell loadout for Wave-1 DPS specs.
    void ApplyRecommendedTalents(Player* bot);

    // Helpers used by per-spec lists.
    uint32 CountNearbyEnemies(Player* bot, float range);
    float AuraRemains(Unit* unit, uint32 spellId);
    bool HasAuraUp(Unit* unit, uint32 spellId);
    uint32 AuraStacks(Unit* unit, uint32 spellId);
    bool SpellReady(Player* bot, uint32 spellId);
    bool CanTryCast(Player* bot, uint32 spellId);
    bool IsSelfCastSpell(uint32 spellId);
}

#endif // _SF_BOT_ROTATION_H
