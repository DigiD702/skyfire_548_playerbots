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
#include "PlayerbotAI.h"
#include "PlayerbotMgr.h"
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
    PlayerbotAI* GetAI(Player* bot)
    {
        return sPlayerbotMgr->GetBotAI(bot);
    }

    bool StrategyOn(Player* bot, char const* name)
    {
        if (PlayerbotAI* ai = GetAI(bot))
            return ai->HasStrategy(name, BotState::Combat);
        // No AI attached — allow utilities so self-tests still work.
        return true;
    }

    // Spells that should be cast on the bot (buffs / forms / seals).
    bool IsSelfCast(uint32 spellId)
    {
        switch (spellId)
        {
            // Self-defensives used by TryPartySupport (must route onto the bot).
            case 642:    // Divine Shield
            case 498:    // Divine Protection
            case 871:    // Shield Wall
            case 12975:  // Last Stand
            case 55694:  // Enraged Regeneration
            case 48792:  // Icebound Fortitude
            case 48707:  // Anti-Magic Shell
            case 55233:  // Vampiric Blood
            case 61336:  // Survival Instincts
            case 22812:  // Barkskin
            case 19263:  // Deterrence
            case 45438:  // Ice Block
            case 47585:  // Dispersion
            case 586:    // Fade
            case 31224:  // Cloak of Shadows
            case 5277:   // Evasion
            case 104773: // Unending Resolve
            case 108271: // Astral Shift
            case 115203: // Fortifying Brew
            case 122278: // Dampen Harm
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
            case 49016:  // Unholy Frenzy
            case 63560:  // Dark Transformation
            case 51533:  // Feral Spirit
            case 1126:   // Mark of the Wild
            case 108683: // Fire and Brimstone
            case 18499:  // Berserker Rage
            case 107574: // Avatar
            case 12292:  // Bloodbath
            case 114207: // Skull Banner
            case 102560: // Incarnation: Chosen of Elune
            case 102543: // Incarnation: King of the Jungle
            case 1856:   // Vanish
            case 1949:   // Hellfire
            case 88751:  // Wild Mushroom: Detonate
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

Unit* FindSecondaryEnemy(Player* bot, Unit* exclude, float range)
{
    if (!bot)
        return nullptr;

    std::list<Unit*> list;
    Skyfire::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, range);
    Skyfire::UnitListSearcher<Skyfire::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, list, check);
    bot->VisitNearbyObject(range, searcher);

    Unit* best = nullptr;
    float bestDist = 0.0f;
    for (Unit* u : list)
    {
        if (!u || u == exclude || !u->IsAlive() || !bot->IsValidAttackTarget(u))
            continue;
        float const dist = bot->GetDistance(u);
        if (!best || dist < bestDist)
        {
            best = u;
            bestDist = dist;
        }
    }
    return best;
}

float AuraRemains(Unit* unit, uint32 spellId)
{
    if (!unit)
        return 0.0f;
    Aura* aura = unit->GetAuraOfRankedSpell(spellId);
    if (!aura)
        aura = unit->GetAura(spellId);
    if (!aura)
        return 0.0f;
    if (aura->IsPermanent() || aura->GetDuration() < 0)
        return 9999.0f;
    return aura->GetDuration() / 1000.0f;
}

bool HasAuraUp(Unit* unit, uint32 spellId)
{
    if (!unit)
        return false;
    if (unit->HasAura(spellId))
        return true;
    return unit->GetAuraOfRankedSpell(spellId) != nullptr;
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
    if (bot->IsNonMeleeSpellCasted(false) || bot->HasUnitState(UNIT_STATE_CASTING))
        return;

    // Self-bot owns WASD — never StopMoving (resets swing and cancels player movement).
    PlayerbotAI* ai = GetAI(bot);
    bool const selfBot = ai && ai->IsClientControlled();
    if (!selfBot)
    {
        if (!bot->IsStopped())
            bot->StopMoving();
        // isMoving() uses movement flags; strip leftovers so cast-time spells
        // are not rejected as SPELL_FAILED_MOVING after a point/face spline.
        bot->RemoveUnitMovementFlag(MOVEMENTFLAG_MASK_MOVING);
    }

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

    // Havoc marks a *secondary* target so Chaos Bolt / Incinerate cleave onto it.
    if (spellId == 80240) // Havoc
    {
        if (Unit* secondary = FindSecondaryEnemy(bot, enemy, 40.0f))
            castTarget = secondary;
        else
            return false;
    }

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
        // StopMoving interrupts an in-progress cast — only plant when idle.
        if (!bot->IsNonMeleeSpellCasted(false) && !bot->HasUnitState(UNIT_STATE_CASTING))
        {
            if (!bot->IsStopped())
                bot->StopMoving();
            bot->RemoveUnitMovementFlag(MOVEMENTFLAG_MASK_MOVING);
            bot->SetSelection(castTarget->GetGUID());
            if (!bot->HasInArc(static_cast<float>(M_PI), castTarget))
                bot->SetInFront(castTarget);
        }
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

    // Defensive / escape racials when low (not gated by boost).
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

    // Offensive racials require +boost.
    if (!StrategyOn(bot, "boost"))
        return false;

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
        102560, // Incarnation: Chosen of Elune
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
    if (!StrategyOn(bot, "boost"))
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

namespace
{
    bool HasAnyAura(Unit* unit, std::initializer_list<uint32> ids)
    {
        if (!unit)
            return false;
        for (uint32 id : ids)
            if (unit->HasAura(id))
                return true;
        return false;
    }

    bool TryCastBuff(Player* bot, uint32 spellId)
    {
        if (!CanTryCast(bot, spellId))
            return false;
        return CastSpell(bot, bot, spellId);
    }

    // True if every nearby group member (and the bot) already has a category aura.
    // Solo: just the bot. Missing anyone in range → cast so raid-wide buffs refresh.
    bool GroupCoveredByBuff(Player* bot, std::initializer_list<uint32> auraIds)
    {
        if (!bot)
            return true;
        if (!HasAnyAura(bot, auraIds))
            return false;

        Group* group = bot->GetGroup();
        if (!group)
            return true;

        float const range = 40.0f;
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsInWorld() || !member->IsAlive() || member == bot)
                continue;
            if (member->GetMap() != bot->GetMap())
                continue;
            if (!bot->IsWithinDistInMap(member, range, false))
                continue;
            if (!HasAnyAura(member, auraIds))
                return false;
        }
        return true;
    }
}

Player* FindPartyMemberToResurrect(Player* bot)
{
    if (!bot)
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* best = nullptr;
    float bestDist = 0.0f;
    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || member == bot || !member->IsInWorld())
            continue;
        if (member->IsAlive() || member->IsRessurectRequested())
            continue;
        // Released ghosts sit at the graveyard — only rez unreleased corpses.
        if (member->getDeathState() != DeathState::CORPSE)
            continue;
        if (member->GetMap() != bot->GetMap())
            continue;

        float const dist = bot->GetDistance(member);
        if (!best || dist < bestDist)
        {
            best = member;
            bestDist = dist;
        }
    }
    return best;
}

uint32 SelectResurrectSpell(Player* bot)
{
    if (!bot)
        return 0;

    // Combat: battle rez only (long CD). OOC: standard class resurrection.
    if (bot->IsInCombat())
    {
        switch (bot->getClass())
        {
            case CLASS_DRUID:
                if (CanTryCast(bot, 20484)) // Rebirth
                    return 20484;
                break;
            case CLASS_DEATH_KNIGHT:
                if (CanTryCast(bot, 61999)) // Raise Ally
                    return 61999;
                break;
            default:
                break;
        }
        return 0;
    }

    uint32 spellId = 0;
    switch (bot->getClass())
    {
        case CLASS_PRIEST:  spellId = 2006;   break; // Resurrection
        case CLASS_PALADIN: spellId = 7328;   break; // Redemption
        case CLASS_SHAMAN:  spellId = 2008;   break; // Ancestral Spirit
        case CLASS_DRUID:   spellId = 50769;  break; // Revive
        case CLASS_MONK:    spellId = 115178; break; // Resuscitate
        default:            return 0;
    }
    return CanTryCast(bot, spellId) ? spellId : 0;
}

bool TryMaintainBuffs(Player* bot)
{
    if (!bot || bot->HasUnitState(UNIT_STATE_CASTING) || bot->IsNonMeleeSpellCasted(false))
        return false;

    // Raid buff cast IDs vs applied aura IDs can differ (e.g. Legacy of the
    // Emperor 115921 applies aura 117666). Always check the auras that stick.
    std::initializer_list<uint32> const kStats = { 1126, 20217, 115921, 117666, 117667 };
    std::initializer_list<uint32> const kStamina = { 21562, 469 };
    std::initializer_list<uint32> const kAttackPower = { 6673, 57330 };
    std::initializer_list<uint32> const kSpellPower = { 1459, 109773 };
    std::initializer_list<uint32> const kMastery = { 19740 };
    std::initializer_list<uint32> const kMeleeCrit = { 116781 };

    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        {
            bool const preferStamina =
                bot->GetTalentSpecialization(bot->GetActiveSpec()) == SPEC_WARRIOR_PROTECTION;
            if (preferStamina)
            {
                if (!GroupCoveredByBuff(bot, kStamina) && TryCastBuff(bot, 469))
                    return true;
                if (!GroupCoveredByBuff(bot, kAttackPower) && TryCastBuff(bot, 6673))
                    return true;
            }
            else
            {
                if (!GroupCoveredByBuff(bot, kAttackPower) && TryCastBuff(bot, 6673))
                    return true;
                if (!GroupCoveredByBuff(bot, kStamina) && TryCastBuff(bot, 469))
                    return true;
            }
            break;
        }
        case CLASS_PALADIN:
            // Only Blessing of Kings for the stats raid buff. Do not auto-cast
            // Might — with MotW/Emperor up it would re-cast every tick (one
            // blessing per pala + incomplete mastery coverage checks).
            if (!GroupCoveredByBuff(bot, kStats) && TryCastBuff(bot, 20217))
                return true;
            break;
        case CLASS_PRIEST:
            if (!GroupCoveredByBuff(bot, kStamina) && TryCastBuff(bot, 21562))
                return true;
            if (!HasAuraUp(bot, 588) && !HasAuraUp(bot, 73413) && TryCastBuff(bot, 588))
                return true;
            break;
        case CLASS_MAGE:
            if (!GroupCoveredByBuff(bot, kSpellPower) && TryCastBuff(bot, 1459))
                return true;
            break;
        case CLASS_WARLOCK:
            if (!GroupCoveredByBuff(bot, kSpellPower) && TryCastBuff(bot, 109773))
                return true;
            break;
        case CLASS_DRUID:
            if (!GroupCoveredByBuff(bot, kStats) && TryCastBuff(bot, 1126))
                return true;
            break;
        case CLASS_DEATH_KNIGHT:
            if (!GroupCoveredByBuff(bot, kAttackPower) && TryCastBuff(bot, 57330))
                return true;
            break;
        case CLASS_MONK:
            if (!GroupCoveredByBuff(bot, kStats) && TryCastBuff(bot, 115921))
                return true;
            if (!GroupCoveredByBuff(bot, kMeleeCrit) && TryCastBuff(bot, 116781))
                return true;
            break;
        default:
            break;
    }

    return false;
}

bool TryPartySupport(Player* bot)
{
    if (!bot || bot->HasUnitState(UNIT_STATE_CASTING))
        return false;

    auto allyNeedsDispel = [](Player* ally, SpellInfo const* cleanseInfo) -> bool
    {
        if (!ally || !cleanseInfo)
            return false;

        uint32 const mask = cleanseInfo->GetDispelMask();
        if (!mask)
            return false;

        for (auto const& pair : ally->GetAppliedAuras())
        {
            AuraApplication const* app = pair.second;
            if (!app || app->IsPositive())
                continue;

            Aura const* aura = app->GetBase();
            if (!aura)
                continue;

            SpellInfo const* auraInfo = aura->GetSpellInfo();
            if (!auraInfo || !auraInfo->Dispel)
                continue;
            if (!(mask & SpellInfo::GetDispelMask(DispelType(auraInfo->Dispel))))
                continue;
            if (!cleanseInfo->CanDispelAura(auraInfo))
                continue;
            return true;
        }
        return false;
    };

    auto isTankSpec = [](Player* p) -> bool
    {
        if (!p)
            return false;
        uint32 const specId = p->GetTalentSpecialization(p->GetActiveSpec());
        uint8 const cls = p->getClass();
        uint32 const* specs = GetClassSpecializations(cls);
        if (!specs)
            return false;
        switch (cls)
        {
            case CLASS_WARRIOR:      return specs[2] == specId;
            case CLASS_PALADIN:      return specs[1] == specId;
            case CLASS_DEATH_KNIGHT: return specs[0] == specId;
            case CLASS_MONK:         return specs[0] == specId;
            case CLASS_DRUID:        return specs[2] == specId;
            default: return false;
        }
    };

    // --- 1) Paladin emergency externals ---
    if (bot->getClass() == CLASS_PALADIN)
    {
        Group* group = bot->GetGroup();
        if (group)
        {
            Player* lohTarget = nullptr;
            Player* hopTarget = nullptr;
            float lohPct = 100.0f;
            float hopPct = 100.0f;

            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member || !member->IsInWorld() || !member->IsAlive())
                    continue;
                if (member->GetMap() != bot->GetMap())
                    continue;
                if (!bot->IsWithinDistInMap(member, 40.0f, false))
                    continue;
                if (member->HasAura(25771)) // Forbearance
                    continue;

                float const pct = UnitHealthPct(member);
                if (pct < 25.0f && pct < lohPct)
                {
                    lohTarget = member;
                    lohPct = pct;
                }
                // HoP: critically low non-tanks (physical immunity stalls tank threat).
                if (pct < 35.0f && !isTankSpec(member) && !member->HasAura(1022) && pct < hopPct)
                {
                    hopTarget = member;
                    hopPct = pct;
                }
            }

            if (lohTarget && CanTryCast(bot, 633))
            {
                if (CastHealSpell(bot, lohTarget, 633))
                    return true;
            }
            if (hopTarget && CanTryCast(bot, 1022))
            {
                if (CastHealSpell(bot, hopTarget, 1022))
                    return true;
            }
        }
    }

    // --- 2) Self-defensives ---
    float const selfHp = UnitHealthPct(bot);
    if (selfHp < 40.0f)
    {
        uint32 defensiveId = 0;
        switch (bot->getClass())
        {
            case CLASS_PALADIN:
                if (CanTryCast(bot, 642)) defensiveId = 642;       // Divine Shield
                else if (CanTryCast(bot, 498)) defensiveId = 498;  // Divine Protection
                break;
            case CLASS_WARRIOR:
                if (CanTryCast(bot, 871)) defensiveId = 871;         // Shield Wall
                else if (CanTryCast(bot, 12975)) defensiveId = 12975; // Last Stand
                else if (CanTryCast(bot, 55694)) defensiveId = 55694; // Enraged Regeneration
                break;
            case CLASS_DEATH_KNIGHT:
                if (CanTryCast(bot, 48792)) defensiveId = 48792;     // Icebound Fortitude
                else if (CanTryCast(bot, 48707)) defensiveId = 48707; // Anti-Magic Shell
                else if (CanTryCast(bot, 55233)) defensiveId = 55233; // Vampiric Blood
                break;
            case CLASS_DRUID:
                if (CanTryCast(bot, 61336)) defensiveId = 61336;     // Survival Instincts
                else if (CanTryCast(bot, 22812)) defensiveId = 22812; // Barkskin
                break;
            case CLASS_HUNTER:
                if (CanTryCast(bot, 19263)) defensiveId = 19263;     // Deterrence
                break;
            case CLASS_MAGE:
                if (CanTryCast(bot, 45438)) defensiveId = 45438;     // Ice Block
                break;
            case CLASS_PRIEST:
                if (CanTryCast(bot, 47585)) defensiveId = 47585;     // Dispersion
                else if (CanTryCast(bot, 586)) defensiveId = 586;    // Fade
                break;
            case CLASS_ROGUE:
                if (CanTryCast(bot, 31224)) defensiveId = 31224;     // Cloak of Shadows
                else if (CanTryCast(bot, 5277)) defensiveId = 5277;  // Evasion
                break;
            case CLASS_WARLOCK:
                if (CanTryCast(bot, 104773)) defensiveId = 104773;   // Unending Resolve
                break;
            case CLASS_SHAMAN:
                if (CanTryCast(bot, 108271)) defensiveId = 108271;   // Astral Shift
                break;
            case CLASS_MONK:
                if (CanTryCast(bot, 115203)) defensiveId = 115203;   // Fortifying Brew
                else if (CanTryCast(bot, 122278)) defensiveId = 122278; // Dampen Harm
                break;
            default:
                break;
        }

        if (defensiveId && CastSpell(bot, bot, defensiveId))
            return true;
    }

    // --- 3) Party cleanse ---
    uint32 cleanseId = 0;
    switch (bot->getClass())
    {
        case CLASS_PALADIN: cleanseId = 4987; break;   // Cleanse
        case CLASS_PRIEST:  cleanseId = 527; break;    // Purify
        case CLASS_SHAMAN:  cleanseId = 51886; break;  // Cleanse Spirit
        case CLASS_MAGE:    cleanseId = 475; break;    // Remove Curse
        case CLASS_DRUID:   cleanseId = 88423; break;  // Nature's Cure
        case CLASS_MONK:    cleanseId = 115450; break; // Detox
        default: break;
    }

    if (cleanseId && CanTryCast(bot, cleanseId))
    {
        SpellInfo const* cleanseInfo = sSpellMgr->GetSpellInfo(cleanseId);
        Group* group = bot->GetGroup();
        if (cleanseInfo && group)
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member || !member->IsInWorld() || !member->IsAlive())
                    continue;
                if (member->GetMap() != bot->GetMap())
                    continue;
                if (!bot->IsWithinDistInMap(member, 40.0f, false))
                    continue;
                if (!allyNeedsDispel(member, cleanseInfo))
                    continue;
                if (CastHealSpell(bot, member, cleanseId))
                    return true;
            }
        }
        // Solo: cleanse self.
        if ((!group || group->GetMembersCount() <= 1) && allyNeedsDispel(bot, cleanseInfo))
        {
            if (CastHealSpell(bot, bot, cleanseId))
                return true;
        }
    }

    return false;
}

bool TryCrowdControl(Player* bot, Unit* enemy)
{
    if (!bot || !enemy || !enemy->IsAlive() || bot->HasUnitState(UNIT_STATE_CASTING))
        return false;
    if (!StrategyOn(bot, "cc"))
        return false;
    if (!bot->IsValidAttackTarget(enemy))
        return false;

    // Prefer an add that is not the tank's current target when possible.
    Unit* ccTarget = enemy;
    if (Group* group = bot->GetGroup())
    {
        Unit* tankVictim = nullptr;
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsAlive())
                continue;
            if (PlayerbotAI* mai = GetAI(member))
            {
                if (mai->GetCombatRolePublic() != 0)
                    continue;
            }
            else
                continue;
            if (Unit* v = member->GetVictim())
            {
                tankVictim = v;
                break;
            }
        }
        if (tankVictim && tankVictim == enemy)
        {
            if (Unit* alt = FindSecondaryEnemy(bot, enemy, 30.0f))
                ccTarget = alt;
            else
                return false; // Don't CC the tank's focus.
        }
    }

    if (ccTarget->HasAuraType(SPELL_AURA_MOD_STUN)
        || ccTarget->HasAuraType(SPELL_AURA_MOD_CONFUSE)
        || ccTarget->HasAuraType(SPELL_AURA_MOD_FEAR)
        || ccTarget->HasAuraType(SPELL_AURA_MOD_ROOT))
        return false;

    auto tryCc = [&](uint32 spellId) -> bool
    {
        if (!CanTryCast(bot, spellId))
            return false;
        return CastSpell(bot, ccTarget, spellId);
    };

    switch (bot->getClass())
    {
        case CLASS_MAGE:
            if (tryCc(118)) // Polymorph
                return true;
            break;
        case CLASS_WARLOCK:
            if (ccTarget->GetCreatureType() == CREATURE_TYPE_DEMON
                || ccTarget->GetCreatureType() == CREATURE_TYPE_ELEMENTAL)
            {
                if (tryCc(710)) // Banish
                    return true;
            }
            if (tryCc(5782)) // Fear
                return true;
            break;
        case CLASS_HUNTER:
            if (tryCc(1499)) // Freezing Trap
                return true;
            break;
        case CLASS_SHAMAN:
            if (tryCc(51514)) // Hex
                return true;
            break;
        case CLASS_PRIEST:
            if (ccTarget->GetCreatureType() == CREATURE_TYPE_UNDEAD && tryCc(9484)) // Shackle Undead
                return true;
            break;
        case CLASS_DRUID:
            if (tryCc(33786)) // Cyclone
                return true;
            break;
        case CLASS_ROGUE:
            if (bot->HasAuraType(SPELL_AURA_MOD_STEALTH) && tryCc(6770)) // Sap
                return true;
            break;
        default:
            break;
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
    if (enemy && TryCrowdControl(bot, enemy))
        return true;
    if (TryPartySupport(bot))
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
    if (!StrategyOn(bot, "aoe"))
        ctx.enemies = 1;
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
    if (!StrategyOn(bot, "aoe"))
        ctx.enemies = 1;
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
