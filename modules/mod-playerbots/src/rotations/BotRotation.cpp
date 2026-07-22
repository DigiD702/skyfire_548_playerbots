/*
 * Shared rotation helpers and spec dispatch.
 */

#include "BotRotation.h"
#include "BotRotationLists.h"

#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Unit.h"

#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

namespace BotRotation
{
namespace
{
    // Spells that should be cast on the bot (buffs / forms / seals).
    bool IsSelfCast(uint32 spellId)
    {
        switch (spellId)
        {
            case 84963:  // Inquisition
            case 31801:  // Seal of Truth
            case 20154:  // Seal of Righteousness
            case 15473:  // Shadowform
            case 21562:  // Power Word: Fortitude
            case 588:    // Inner Fire
            case 324:    // Lightning Shield
            case 109773: // Dark Intent
            case 116740: // Tigereye Brew (consume)
            case 115288: // Energizing Brew
            case 74434:  // Soulburn
            case 1454:   // Life Tap
            case 113860: // Dark Soul: Misery
            case 82692:  // Focus Fire
            case 19574:  // Bestial Wrath
            case 3045:   // Rapid Fire
            case 121818: // Stampede
            case 883:    // Call Pet
            case 136:    // Mend Pet
            case 13165:  // Aspect of the Hawk
            case 109260: // Aspect of the Iron Hawk
            case 20217:  // Blessing of Kings
            case 19740:  // Blessing of Might
            case 115921: // Legacy of the Emperor
            case 116781: // Legacy of the White Tiger
            case 16166:  // Elemental Mastery
            case 114049: // Ascendance (shaman)
            case 79206:  // Spiritwalker's Grace
            case 8024:   // Flametongue Weapon
            case 8232:   // Windfury Weapon
            case 768:    // Cat Form
            case 52610:  // Savage Roar
            case 5217:   // Tiger's Fury
            case 106951: // Berserk (feral)
            case 6673:   // Battle Shout
            case 2458:   // Berserker Stance
            case 1719:   // Recklessness
            case 12328:  // Sweeping Strikes
            case 5171:   // Slice and Dice
            case 13750:  // Adrenaline Rush
            case 121471: // Shadow Blades
            case 13877:  // Blade Flurry
            case 12472:  // Icy Veins
            case 31687:  // Summon Water Elemental
            case 103958: // Metamorphosis
            case 80240:  // Havoc
            case 49016:  // Unholy Frenzy
            case 63560:  // Dark Transformation
            case 51533:  // Feral Spirit
            case 1126:   // Mark of the Wild
            case 108683: // Fire and Brimstone
            case 18499:  // Berserker Rage
            case 57330:  // Horn of Winter
            case 46584:  // Raise Dead
            case 113858: // Dark Soul: Instability
            case 113861: // Dark Soul: Knowledge
            case 24858:  // Moonkin Form
            case 5487:   // Bear Form
            case 71:     // Defensive Stance
            case 48263:  // Blood Presence
            case 20165:  // Seal of Insight
            case 6117:   // Mage Armor
            case 30482:  // Molten Armor
            case 1459:   // Arcane Brilliance
            case 51713:  // Shadow Dance
            case 49222:  // Bone Shield
            case 115069: // Stance of the Sturdy Ox
            case 115295: // Guard
            case 2565:   // Shield Block
            case 871:    // Shield Wall
            case 1160:   // Demoralizing Shout
            case 62606:  // Savage Defense
            case 132404: // Shield Block (MoP buff)
                return true;
            default:
                return false;
        }
    }
}

uint32 CountNearbyEnemies(Player* bot, float range)
{
    if (!bot)
        return 0;

    std::list<Unit*> list;
    Skyfire::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, range);
    Skyfire::UnitListSearcher<Skyfire::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, list, check);
    bot->VisitNearbyObject(range, searcher);

    uint32 count = 0;
    for (Unit* u : list)
        if (u && u->IsAlive() && bot->IsValidAttackTarget(u))
            ++count;
    return count ? count : 1;
}

float AuraRemains(Unit* unit, uint32 spellId)
{
    if (!unit)
        return 0.0f;
    if (Aura* aura = unit->GetAura(spellId))
        return aura->GetDuration() / 1000.0f;
    return 0.0f;
}

bool HasAuraUp(Unit* unit, uint32 spellId)
{
    return unit && unit->HasAura(spellId);
}

uint32 AuraStacks(Unit* unit, uint32 spellId)
{
    if (!unit)
        return 0;
    if (Aura* aura = unit->GetAura(spellId))
        return aura->GetStackAmount();
    return 0;
}

bool SpellReady(Player* bot, uint32 spellId)
{
    if (!bot || !spellId)
        return false;
    if (!bot->HasSpell(spellId))
        return false;
    if (bot->HasSpellCooldown(spellId))
        return false;
    return true;
}

bool CanTryCast(Player* bot, uint32 spellId)
{
    if (!SpellReady(bot, spellId))
        return false;
    if (bot->HasUnitState(UNIT_STATE_CASTING))
        return false;
    return true;
}

bool CastSpell(Player* bot, Unit* enemy, uint32 spellId)
{
    if (!bot || !spellId || !CanTryCast(bot, spellId))
        return false;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return false;

    // Self-buffs / stances / shouts / ground AoE around the caster must not be
    // forced onto the enemy (that path has crashed for prot warrior Shield Block).
    Unit* castTarget = enemy;
    if (IsSelfCast(spellId) || !info->NeedsExplicitUnitTarget())
        castTarget = bot;
    if (!castTarget)
        return false;

    bot->CastSpell(castTarget, spellId, false);
    return true;
}

uint32 SelectNextSpell(Player* bot, Unit* target)
{
    if (!bot || !target || !target->IsAlive())
        return 0;

    Context ctx;
    ctx.bot = bot;
    ctx.target = target;
    ctx.enemies = CountNearbyEnemies(bot, 10.0f);
    ctx.targetHealthPct = target->GetMaxHealth()
        ? (100.0f * float(target->GetHealth()) / float(target->GetMaxHealth()))
        : 100.0f;
    ctx.comboPoints = bot->GetComboPoints();

    uint32 const spec = bot->GetTalentSpecialization(bot->GetActiveSpec());
    switch (spec)
    {
        case SPEC_PALADIN_RETRIBUTION:   return SelectRetribution(ctx);
        case SPEC_PALADIN_PROTECTION:    return SelectProtectionPaladin(ctx);
        case SPEC_MONK_WINDWALKER:       return SelectWindwalker(ctx);
        case SPEC_MONK_BREWMASTER:       return SelectBrewmaster(ctx);
        case SPEC_HUNTER_BEAST_MASTERY:  return SelectBeastMastery(ctx);
        case SPEC_HUNTER_MARKSMANSHIP:   return SelectMarksmanship(ctx);
        case SPEC_HUNTER_SURVIVAL:       return SelectSurvival(ctx);
        case SPEC_PRIEST_SHADOW:         return SelectShadow(ctx);
        case SPEC_WARLOCK_AFFLICTION:    return SelectAffliction(ctx);
        case SPEC_WARLOCK_DESTRUCTION:   return SelectDestruction(ctx);
        case SPEC_WARLOCK_DEMONOLOGY:    return SelectDemonology(ctx);
        case SPEC_SHAMAN_ELEMENTAL:      return SelectElemental(ctx);
        case SPEC_SHAMAN_ENHANCEMENT:    return SelectEnhancement(ctx);
        case SPEC_DRUID_BALANCE:         return SelectBalance(ctx);
        case SPEC_DRUID_FERAL:           return SelectFeral(ctx);
        case SPEC_DRUID_GUARDIAN:        return SelectGuardian(ctx);
        case SPEC_WARRIOR_ARMS:          return SelectArms(ctx);
        case SPEC_WARRIOR_FURY:          return SelectFury(ctx);
        case SPEC_WARRIOR_PROTECTION:    return SelectProtectionWarrior(ctx);
        case SPEC_ROGUE_ASSASSINATION:   return SelectAssassination(ctx);
        case SPEC_ROGUE_COMBAT:          return SelectCombat(ctx);
        case SPEC_ROGUE_SUBTLETY:        return SelectSubtlety(ctx);
        case SPEC_MAGE_ARCANE:           return SelectArcane(ctx);
        case SPEC_MAGE_FIRE:             return SelectFire(ctx);
        case SPEC_MAGE_FROST:            return SelectFrostMage(ctx);
        case SPEC_DEATH_KNIGHT_BLOOD:    return SelectBlood(ctx);
        case SPEC_DEATH_KNIGHT_FROST:    return SelectFrostDK(ctx);
        case SPEC_DEATH_KNIGHT_UNHOLY:   return SelectUnholy(ctx);
        default:                         return 0;
    }
}

} // namespace BotRotation
