/*
 * Shared rotation helpers and spec dispatch.
 */

#include "BotRotation.h"
#include "BotRotationLists.h"

#include "DBCStores.h"
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
            case 26573:  // Consecration
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

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return false;
    if (bot->GetGlobalCooldownMgr().HasGlobalCooldown(info))
        return false;
    return true;
}

// Face + stop so SPELL_FAILED_UNIT_NOT_INFRONT / moving-cast failures don't
// silently no-op (bots previously called CastSpell and assumed success).
void PrepareHostileCast(Player* bot, Unit* castTarget)
{
    if (!bot || !castTarget || castTarget == bot)
        return;

    if (!bot->IsStopped())
        bot->StopMoving();

    bot->SetSelection(castTarget->GetGUID());
    if (!bot->HasInArc(static_cast<float>(M_PI), castTarget))
        bot->SetInFront(castTarget);
}

// True only if the cast actually started (GCD / spell CD / cast state).
// Returning true on CheckCast failure used to stall whole rotations on spells
// like Garrote (needs stealth) or Unleash Elements (needs weapon imbue).
bool CastStarted(Player* bot, SpellInfo const* info, uint32 spellId)
{
    if (!bot || !info)
        return false;
    if (bot->FindCurrentSpellBySpellId(spellId))
        return true;
    if (bot->HasUnitState(UNIT_STATE_CASTING))
        return true;
    if (bot->HasSpellCooldown(spellId) || bot->GetSpellCooldownDelay(spellId) > 0)
        return true;
    if (bot->GetGlobalCooldownMgr().HasGlobalCooldown(info))
        return true;
    return false;
}

bool CastSpell(Player* bot, Unit* enemy, uint32 spellId)
{
    if (!bot || !spellId || !CanTryCast(bot, spellId))
        return false;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return false;

    // Only force self for known self-buffs / ground AoE. Do NOT use
    // !NeedsExplicitUnitTarget() — that redirected many damage spells onto the
    // caster and made every cast fail (no abilities on recount).
    Unit* castTarget = IsSelfCast(spellId) ? static_cast<Unit*>(bot) : enemy;
    if (!castTarget)
        return false;

    if (castTarget != bot)
        PrepareHostileCast(bot, castTarget);

    bot->CastSpell(castTarget, spellId, false);
    return CastStarted(bot, info, spellId);
}

bool CastHealSpell(Player* bot, Player* ally, uint32 spellId)
{
    if (!bot || !spellId || !CanTryCast(bot, spellId))
        return false;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return false;

    Unit* castTarget = IsSelfCast(spellId) ? static_cast<Unit*>(bot) : static_cast<Unit*>(ally);
    if (!castTarget)
        return false;

    if (castTarget != bot)
    {
        if (!bot->IsStopped())
            bot->StopMoving();
        bot->SetSelection(castTarget->GetGUID());
        if (!bot->HasInArc(static_cast<float>(M_PI), castTarget))
            bot->SetInFront(castTarget);
    }

    bot->CastSpell(castTarget, spellId, false);
    return CastStarted(bot, info, spellId);
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

    return CastSpell(bot, target, interruptId);
}

bool TryRacial(Player* bot, Unit* /*targetOrSelf*/)
{
    if (!bot || bot->HasUnitState(UNIT_STATE_CASTING))
        return false;

    float const hpPct = UnitHealthPct(bot);

    auto trySelf = [&](uint32 id) -> bool
    {
        if (!CanTryCast(bot, id))
            return false;
        return CastSpell(bot, bot, id);
    };

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
            if (trySelf(id))
                return true;
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
        if (trySelf(id))
            return true;

    if (CountNearbyEnemies(bot, 8.0f) >= 2 && trySelf(20549)) // War Stomp
        return true;

    return false;
}

bool IsBursting(Player* bot)
{
    if (!bot)
        return false;

    // Major DPS cooldown buffs — trinkets should land in these windows.
    static uint32 const kBurstAuras[] =
    {
        13750,  // Adrenaline Rush
        121471, // Shadow Blades
        79140,  // Vendetta (debuff on target — also checked below)
        51713,  // Shadow Dance
        114050, // Ascendance (Elemental)
        114051, // Ascendance (Enhancement)
        114052, // Ascendance (Resto)
        16166,  // Elemental Mastery
        12472,  // Icy Veins
        12042,  // Arcane Power
        48108,  // Hot Streak (not a CD but burst)
        3045,   // Rapid Fire
        19574,  // Bestial Wrath
        121818, // Stampede
        1719,   // Recklessness
        107574, // Avatar
        46924,  // Bladestorm
        31884,  // Avenging Wrath
        105809, // Holy Avenger
        51271,  // Pillar of Frost
        49206,  // Summon Gargoyle / Dark Transformation window proxy
        49016,  // Unholy Frenzy
        103958, // Metamorphosis
        113860, // Dark Soul: Misery
        113861, // Dark Soul: Knowledge
        113858, // Dark Soul: Instability
        106951, // Berserk (Feral)
        102543, // Incarnation: King of the Jungle
        112071, // Celestial Alignment
        116740, // Tigereye Brew
        115288, // Energizing Brew
    };

    for (uint32 id : kBurstAuras)
        if (HasAuraUp(bot, id))
            return true;

    if (Unit* victim = bot->GetVictim())
        if (HasAuraUp(victim, 79140)) // Vendetta
            return true;

    return false;
}

bool HasGlyphSpell(Player* bot, uint32 glyphSpellId)
{
    if (!bot || !glyphSpellId)
        return false;
    if (bot->HasSpell(glyphSpellId) || bot->HasAura(glyphSpellId))
        return true;

    for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
    {
        uint32 const glyph = bot->GetGlyph(bot->GetActiveSpec(), slot);
        if (!glyph)
            continue;
        if (GlyphPropertiesEntry const* gp = sGlyphPropertiesStore.LookupEntry(glyph))
            if (gp->SpellId == glyphSpellId)
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

            if (bot->CanUseItem(item) != InventoryResult::EQUIP_ERR_OK)
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
            if (bot->HasSpellCooldown(data.SpellId) || bot->GetSpellCooldownDelay(data.SpellId) > 0)
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
        // Healer specs: damage line used when co +healer dps (nobody needs heals).
        case SPEC_PALADIN_HOLY:          return SelectHolyPaladinDps(ctx);
        case SPEC_PRIEST_DISCIPLINE:     return SelectDisciplineDps(ctx);
        case SPEC_PRIEST_HOLY:           return SelectHolyPriestDps(ctx);
        case SPEC_SHAMAN_RESTORATION:    return SelectRestorationShamanDps(ctx);
        case SPEC_DRUID_RESTORATION:     return SelectRestorationDruidDps(ctx);
        case SPEC_MONK_MISTWEAVER:       return SelectMistweaverDps(ctx);
        default:                         return 0;
    }
}

uint32 SelectNextHeal(Player* bot, Player* ally, bool saveMana, float saveManaThreshold)
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
    ctx.saveMana = saveMana;
    ctx.saveManaThreshold = saveManaThreshold;

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
