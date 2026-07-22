/*
 * Shared rotation helpers and spec dispatch.
 */

#include "BotRotation.h"
#include "BotRotationLists.h"

#include "Group.h"
#include "GroupReference.h"
#include "Item.h"
#include "ItemPrototype.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Spell.h"
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
            case 54428:  // Divine Plea
            case 31884:  // Avenging Wrath
            case 31842:  // Divine Favor
            case 105809: // Holy Avenger
            case 20572:  // Blood Fury (AP)
            case 33702:  // Blood Fury (SP)
            case 33697:  // Blood Fury (both)
            case 26297:  // Berserking
            case 28730:  // Arcane Torrent (mana)
            case 25046:  // Arcane Torrent (energy)
            case 50613:  // Arcane Torrent (runic)
            case 69179:  // Arcane Torrent (rage)
            case 80483:  // Arcane Torrent (focus)
            case 129597: // Arcane Torrent (chi)
            case 69041:  // Rocket Barrage
            case 20549:  // War Stomp
            case 59752:  // Every Man for Himself
            case 7744:   // Will of the Forsaken
            case 20594:  // Stoneform
            case 20589:  // Escape Artist
            case 5394:   // Healing Stream Totem
            case 109964: // Spirit Shell
            case 10060:  // Power Infusion
            case 64843:  // Divine Hymn
            case 33891:  // Tree of Life
            case 115294: // Mana Tea
            case 116680: // Thunder Focus Tea
            case 132158: // Nature's Swiftness (druid)
            case 52127:  // Water Shield
                return true;
            default:
                return false;
        }
    }

    uint32 InterruptSpellForClass(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:      return 6552;   // Pummel
            case CLASS_ROGUE:        return 1766;   // Kick
            case CLASS_MAGE:         return 2139;   // Counterspell
            case CLASS_SHAMAN:       return 57994;  // Wind Shear
            case CLASS_DEATH_KNIGHT: return 47528;  // Mind Freeze
            case CLASS_PALADIN:      return 96231;  // Rebuke
            case CLASS_MONK:         return 116705; // Spear Hand Strike
            case CLASS_DRUID:        return 106839; // Skull Bash
            case CLASS_HUNTER:       return 147362; // Counter Shot
            case CLASS_PRIEST:       return 15487;  // Silence
            case CLASS_WARLOCK:      return 119910; // Spell Lock (Command Demon)
            default:                 return 0;
        }
    }

    float UnitHealthPct(Unit const* unit)
    {
        if (!unit || !unit->GetMaxHealth())
            return 100.0f;
        return 100.0f * float(unit->GetHealth()) / float(unit->GetMaxHealth());
    }
}

bool IsSelfCastSpell(uint32 spellId)
{
    return IsSelfCast(spellId);
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

bool CastHealSpell(Player* bot, Player* ally, uint32 spellId)
{
    if (!bot || !spellId || !CanTryCast(bot, spellId))
        return false;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return false;

    Unit* castTarget = ally;
    if (IsSelfCast(spellId) || !info->NeedsExplicitUnitTarget())
        castTarget = bot;
    if (!castTarget)
        return false;

    bot->CastSpell(castTarget, spellId, false);
    return true;
}

bool TryInterrupt(Player* bot, Unit* target)
{
    if (!bot || !target || !target->IsAlive())
        return false;
    if (!target->IsNonMeleeSpellCasted(false))
        return false;

    uint32 interruptId = InterruptSpellForClass(bot->getClass());
    // Hunter fallback to Silencing Shot when Counter Shot is unknown.
    if (bot->getClass() == CLASS_HUNTER && !SpellReady(bot, interruptId))
        interruptId = 34490;
    // Warlock Optical Blast fallback.
    if (bot->getClass() == CLASS_WARLOCK && !SpellReady(bot, interruptId))
        interruptId = 119911;

    if (!CanTryCast(bot, interruptId))
        return false;

    bot->CastSpell(target, interruptId, false);
    return true;
}

bool TryRacial(Player* bot, Unit* /*targetOrSelf*/)
{
    if (!bot || bot->HasUnitState(UNIT_STATE_CASTING))
        return false;

    float const hpPct = UnitHealthPct(bot);

    // Defensive / escape racials when low.
    if (hpPct < 35.0f)
    {
        static uint32 const defensive[] = {
            59752, // Every Man for Himself
            7744,  // Will of the Forsaken
            20594, // Stoneform
            20589, // Escape Artist
        };
        for (uint32 id : defensive)
            if (CanTryCast(bot, id))
            {
                bot->CastSpell(bot, id, false);
                return true;
            }
    }

    if (!bot->IsInCombat())
        return false;

    // Offensive racials while fighting.
    static uint32 const offensive[] = {
        33697,  // Blood Fury (both)
        20572,  // Blood Fury (AP)
        33702,  // Blood Fury (SP)
        26297,  // Berserking
        69041,  // Rocket Barrage
        28730,  // Arcane Torrent (mana)
        25046,  // Arcane Torrent (energy)
        50613,  // Arcane Torrent (runic)
        69179,  // Arcane Torrent (rage)
        80483,  // Arcane Torrent (focus)
        129597, // Arcane Torrent (chi)
    };
    for (uint32 id : offensive)
        if (CanTryCast(bot, id))
        {
            bot->CastSpell(bot, id, false);
            return true;
        }

    if (CountNearbyEnemies(bot, 8.0f) >= 2 && CanTryCast(bot, 20549)) // War Stomp
    {
        bot->CastSpell(bot, 20549, false);
        return true;
    }

    return false;
}

bool TryTrinkets(Player* bot)
{
    if (!bot || bot->HasUnitState(UNIT_STATE_CASTING))
        return false;

    for (uint8 slot = EQUIPMENT_SLOT_TRINKET1; slot <= EQUIPMENT_SLOT_TRINKET2; ++slot)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            continue;

        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            _Spell const& data = proto->Spells[i];
            if (data.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE || !data.SpellId)
                continue;

            if (bot->HasSpellCooldown(data.SpellId) || bot->GetSpellCooldownDelay(data.SpellId) > 0)
                continue;

            SpellInfo const* info = sSpellMgr->GetSpellInfo(data.SpellId);
            if (!info)
                continue;

            SpellCastTargets targets;
            Unit* victim = bot->GetVictim();
            if (info->NeedsExplicitUnitTarget() && victim && bot->IsValidAttackTarget(victim))
                targets.SetUnitTarget(victim);
            else
                targets.SetUnitTarget(bot);

            bot->CastItemUseSpell(item, targets, 0, 0);
            return true;
        }
    }
    return false;
}

bool TryCombatUtilities(Player* bot, Unit* enemy)
{
    if (!bot)
        return false;
    if (bot->HasUnitState(UNIT_STATE_CASTING))
        return false;

    if (enemy && TryInterrupt(bot, enemy))
        return true;
    if (TryRacial(bot, enemy ? enemy : bot))
        return true;
    if (TryTrinkets(bot))
        return true;
    return false;
}

uint32 SelectNextSpell(Player* bot, Unit* target)
{
    if (!bot || !target || !target->IsAlive())
        return 0;

    Context ctx;
    ctx.bot = bot;
    ctx.target = target;
    ctx.enemies = CountNearbyEnemies(bot, 10.0f);
    ctx.targetHealthPct = UnitHealthPct(target);
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

uint32 SelectNextHeal(Player* bot, Player* ally)
{
    if (!bot || !ally || !ally->IsAlive())
        return 0;

    HealContext ctx;
    ctx.bot = bot;
    ctx.healTarget = ally;
    ctx.healTargetHealthPct = UnitHealthPct(ally);
    ctx.lowestAllyHealthPct = ctx.healTargetHealthPct;
    ctx.injuredAllies = 0;
    ctx.enemies = CountNearbyEnemies(bot, 10.0f);
    ctx.manaPct = bot->GetMaxPower(POWER_MANA)
        ? (100.0f * float(bot->GetPower(POWER_MANA)) / float(bot->GetMaxPower(POWER_MANA)))
        : 100.0f;

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsAlive() || !bot->IsInMap(member))
                continue;
            if (!bot->IsWithinDistInMap(member, 40.0f))
                continue;
            float const pct = UnitHealthPct(member);
            if (pct < ctx.lowestAllyHealthPct)
                ctx.lowestAllyHealthPct = pct;
            if (pct < 90.0f)
                ++ctx.injuredAllies;
        }
    }
    else if (ctx.healTargetHealthPct < 90.0f)
        ctx.injuredAllies = 1;

    uint32 const spec = bot->GetTalentSpecialization(bot->GetActiveSpec());
    switch (spec)
    {
        case SPEC_PALADIN_HOLY:        return SelectHolyPaladin(ctx);
        case SPEC_PRIEST_DISCIPLINE:   return SelectDiscipline(ctx);
        case SPEC_PRIEST_HOLY:         return SelectHolyPriest(ctx);
        case SPEC_SHAMAN_RESTORATION:  return SelectRestorationShaman(ctx);
        case SPEC_DRUID_RESTORATION:   return SelectRestorationDruid(ctx);
        case SPEC_MONK_MISTWEAVER:     return SelectMistweaver(ctx);
        default:                       return 0;
    }
}

} // namespace BotRotation
