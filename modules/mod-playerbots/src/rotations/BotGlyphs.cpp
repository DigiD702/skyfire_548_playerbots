/*
 * MoP glyph loadouts for .playerbots init.
 *
 * Each spec gets 3 major + 3 minor glyphs (item IDs). Slot mapping matches
 * AzerothCore / MoP socket order: major, minor, major, minor, minor, major
 * applied onto physical slots {0,1,3,2,4,5}.
 *
 * TypeFlags were validated against GlyphProperties.dbc for 5.4.8 — MoP remapped
 * many WotLK major/minor glyphs, so names must match this expansion's data.
 */

#include "BotRotation.h"

#include "DBCStores.h"
#include "ItemPrototype.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace BotRotation
{
namespace
{
    uint8 const kGlyphOrder[MAX_GLYPH_SLOT_INDEX] = { 0, 1, 3, 2, 4, 5 };

    struct SpecGlyphLoadout
    {
        uint32 specId;
        // Item IDs: major1, minor1, major2, minor2, minor3, major3
        uint32 items[MAX_GLYPH_SLOT_INDEX];
    };

    SpecGlyphLoadout const s_glyphLoadouts[] =
    {
        { SPEC_WARRIOR_ARMS, { 43421, 43395, 43423, 43396, 49084, 63481 } }, // Mortal Strike/Battle/Slam/Berserker Rage/Command/Colossus Smash
        { SPEC_WARRIOR_FURY, { 43432, 43395, 43416, 43396, 49084, 43414 } }, // Raging Blow/Battle/Bloodthirst/Berserker Rage/Command/Cleaving
        { SPEC_WARRIOR_PROTECTION, { 43425, 43395, 43415, 49084, 43396, 45797 } }, // Shield Slam/Battle/Devastate/Command/Berserker Rage/Shield Wall
        { SPEC_PALADIN_RETRIBUTION, { 45743, 43365, 41092, 43340, 43368, 41097 } }, // Templar's Verdict/Blessing of Kings/Judgement/Blessing of Might/Truth/Hammer of Wrath
        { SPEC_PALADIN_PROTECTION, { 41101, 43365, 45744, 41100, 80584, 41096 } }, // Focused Shield/Blessing of Kings/Shield of the Righteous/Righteousness/the Falling Avenger/Divine Protection
        { SPEC_PALADIN_HOLY, { 41105, 43365, 41106, 43366, 80584, 45741 } }, // Word of Glory/Blessing of Kings/Divine Favor/Insight/the Falling Avenger/Beacon of Light
        { SPEC_HUNTER_BEAST_MASTERY, { 42915, 43338, 42902, 87393, 43350, 42907 } }, // Kill Command/Revive Pet/Bestial Wrath/Fetch/Mend Pet/Misdirection
        { SPEC_HUNTER_MARKSMANSHIP, { 45625, 43338, 42914, 87393, 42897, 45734 } }, // Chimera Shot/Revive Pet/Steady Shot/Fetch/Aimed Shot/Scatter Shot
        { SPEC_HUNTER_SURVIVAL, { 45731, 43338, 42913, 87393, 42912, 42906 } }, // Explosive Shot/Revive Pet/Snake Trap/Fetch/Serpent Sting/Ice Trap
        { SPEC_ROGUE_ASSASSINATION, { 45761, 43380, 42964, 43379, 43378, 42969 } }, // Vendetta/Poisons/Garrote/Blurred Speed/Safe Fall/Rupture
        { SPEC_ROGUE_COMBAT, { 42954, 43380, 42957, 43379, 43378, 42972 } }, // Adrenaline Rush/Poisons/Blade Flurry/Blurred Speed/Safe Fall/Sinister Strike
        { SPEC_ROGUE_SUBTLETY, { 42955, 43380, 45764, 43379, 42967, 63420 } }, // Ambush/Poisons/Shadow Dance/Blurred Speed/Hemorrhage/Vanish
        { SPEC_PRIEST_SHADOW, { 79513, 43371, 79514, 43342, 77101, 45753 } }, // Mind Flay/Fortitude/Shadow Word: Death/Fading/Shadow/Dispersion
        { SPEC_PRIEST_DISCIPLINE, { 42408, 43371, 45756, 43342, 43374, 45760 } }, // Power Word: Shield/Fortitude/Penance/Fading/Shadowfiend/Pain Suppression
        { SPEC_PRIEST_HOLY, { 42409, 43371, 42396, 43342, 104122, 87902 } }, // Prayer of Healing/Fortitude/Circle of Healing/Fading/Inspired Hymns/Lightwell
        { SPEC_DEATH_KNIGHT_BLOOD, { 43554, 43671, 43536, 43672, 43539, 45799 } }, // Vampiric Blood/Path of Frost/Bone Shield/Resilient Grip/Death's Embrace/Dancing Rune Weapon
        { SPEC_DEATH_KNIGHT_FROST, { 43543, 43671, 43547, 43672, 43544, 43553 } }, // Frost Strike/Path of Frost/Obliterate/Resilient Grip/Horn of Winter/Pillar of Frost
        { SPEC_DEATH_KNIGHT_UNHOLY, { 43542, 43671, 43549, 43672, 43539, 45804 } }, // Death and Decay/Path of Frost/Raise Dead/Resilient Grip/Death's Embrace/Death Coil
        { SPEC_SHAMAN_ELEMENTAL, { 41518, 43381, 41536, 43388, 44923, 41531 } }, // Chain Lightning/Astral Recall/Lightning Bolt/Water Walking/Thunderstorm/Flame Shock
        { SPEC_SHAMAN_ENHANCEMENT, { 45771, 43381, 41530, 104128, 41540, 41539 } }, // Feral Spirit/Astral Recall/Fire Nova/Spirit Wolf/Lava Lash/Stormstrike
        { SPEC_SHAMAN_RESTORATION, { 45772, 43381, 41533, 43388, 43385, 41534 } }, // Riptide/Astral Recall/Healing Stream Totem/Water Walking/Renewed Life/Healing Wave
        { SPEC_MAGE_ARCANE, { 42736, 43339, 44955, 43359, 43364, 45737 } }, // Arcane Power/Arcane Brilliance/Arcane Blast/Conjuring/Slow Fall/Slow
        { SPEC_MAGE_FIRE, { 63539, 43339, 42739, 42751, 43364, 42754 } }, // Living Bomb/Arcane Brilliance/Fireball/Molten Armor/Slow Fall/Dragon's Breath
        { SPEC_MAGE_FROST, { 42746, 43339, 42745, 104106, 43364, 42741 } }, // Icy Veins/Arcane Brilliance/Ice Lance/Condensation/Slow Fall/Frost Nova
        { SPEC_WARLOCK_AFFLICTION, { 42472, 43389, 42466, 43391, 45789, 45779 } }, // Unstable Affliction/Unending Breath/Soul Swap/Eye of Kilrogg/Soul Link/Haunt
        { SPEC_WARLOCK_DEMONOLOGY, { 45780, 43389, 42465, 43391, 42459, 42455 } }, // Metamorphosis/Unending Breath/Imp/Eye of Kilrogg/Felguard/Corruption
        { SPEC_WARLOCK_DESTRUCTION, { 42454, 43389, 45781, 43391, 42453, 104054 } }, // Conflagrate/Unending Breath/Chaos Bolt/Eye of Kilrogg/Incinerate/Havoc
        { SPEC_MONK_WINDWALKER, { 87892, 87887, 85697, 87889, 87884, 85700 } }, // Fists of Fury/Spirit Roll/Spinning Crane Kick/Water Roll/Jab/Touch of Death
        { SPEC_MONK_BREWMASTER, { 85691, 87887, 87893, 87889, 87884, 85685 } }, // Guard/Spirit Roll/Fortifying Brew/Water Roll/Jab/Breath of Fire
        { SPEC_MONK_MISTWEAVER, { 85696, 87887, 87895, 87889, 87884, 85699 } }, // Renewing Mist/Spirit Roll/Life Cocoon/Water Roll/Jab/Surging Mist
        { SPEC_DRUID_BALANCE, { 45603, 43316, 40923, 43335, 68039, 40921 } }, // Starsurge/Aquatic Form/Moonfire/Mark of the Wild/the Treant/Starfall
        { SPEC_DRUID_FERAL, { 40902, 43316, 45604, 43335, 89868, 40901 } }, // Rip/Aquatic Form/Savage Roar/Mark of the Wild/the Cheetah/Shred
        { SPEC_DRUID_GUARDIAN, { 40896, 43316, 40897, 43335, 43334, 67484 } }, // Frenzied Regeneration/Aquatic Form/Maul/Mark of the Wild/Challenging Roar/Lacerate
        { SPEC_DRUID_RESTORATION, { 40913, 43316, 40912, 43335, 104102, 45602 } }, // Rejuvenation/Aquatic Form/Regrowth/Mark of the Wild/the Sprouting Mushroom/Wild Growth
    };

    uint32 const* FindGlyphItems(uint32 specId)
    {
        for (SpecGlyphLoadout const& loadout : s_glyphLoadouts)
            if (loadout.specId == specId)
                return loadout.items;
        return nullptr;
    }

    uint8 MaxGlyphSlotsForLevel(uint8 level)
    {
        // SkyFire InitGlyphsForLevel: 25→slots 0-1, 50→0-3, 75→0-5
        if (level >= 75)
            return MAX_GLYPH_SLOT_INDEX;
        if (level >= 50)
            return 4;
        if (level >= 25)
            return 2;
        return 0;
    }

    uint32 GlyphPropFromItem(uint32 itemId)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto || proto->Class != ITEM_CLASS_GLYPH)
            return 0;

        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            uint32 const spellId = proto->Spells[i].SpellId;
            if (!spellId)
                continue;
            SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
            if (!info)
                continue;
            for (uint8 eff = 0; eff < MAX_SPELL_EFFECTS; ++eff)
            {
                if (info->Effects[eff].Effect != SPELL_EFFECT_APPLY_GLYPH)
                    continue;
                if (uint32 glyph = info->Effects[eff].MiscValue)
                    return glyph;
            }
        }
        return 0;
    }

    void ClearGlyphSlot(Player* bot, uint8 slot)
    {
        uint32 const oldGlyph = bot->GetGlyph(bot->GetActiveSpec(), slot);
        if (!oldGlyph)
            return;
        if (GlyphPropertiesEntry const* oldGp = sGlyphPropertiesStore.LookupEntry(oldGlyph))
            bot->RemoveAurasDueToSpell(oldGp->SpellId);
        bot->SetGlyph(slot, 0);
    }

    bool ApplyGlyphProp(Player* bot, uint8 slot, uint32 glyphPropId)
    {
        if (!bot || !glyphPropId || slot >= MAX_GLYPH_SLOT_INDEX)
            return false;

        GlyphPropertiesEntry const* gp = sGlyphPropertiesStore.LookupEntry(glyphPropId);
        if (!gp)
            return false;

        GlyphSlotEntry const* gs = sGlyphSlotStore.LookupEntry(bot->GetGlyphSlot(slot));
        if (!gs || gp->TypeFlags != gs->TypeFlags)
            return false;

        if (bot->GetGlyph(bot->GetActiveSpec(), slot) == glyphPropId)
        {
            // Ensure the glyph aura is present even if the slot was already set.
            if (!bot->HasAura(gp->SpellId))
                bot->CastSpell(bot, gp->SpellId, true);
            return true;
        }

        ClearGlyphSlot(bot, slot);
        bot->CastSpell(bot, gp->SpellId, true);
        bot->SetGlyph(slot, glyphPropId);
        return true;
    }
}

void ApplyRecommendedGlyphs(Player* bot)
{
    if (!bot || bot->getLevel() < 25)
        return;

    bot->InitGlyphsForLevel();

    uint8 const maxSlots = MaxGlyphSlotsForLevel(bot->getLevel());
    if (!maxSlots)
        return;

    uint32 const spec = bot->GetTalentSpecialization(bot->GetActiveSpec());
    uint32 const* items = FindGlyphItems(spec);
    if (!items)
        return;

    bool changed = false;
    for (uint8 i = 0; i < maxSlots && i < MAX_GLYPH_SLOT_INDEX; ++i)
    {
        uint8 const realSlot = kGlyphOrder[i];
        uint32 const glyphProp = GlyphPropFromItem(items[i]);
        if (!glyphProp)
            continue;
        if (ApplyGlyphProp(bot, realSlot, glyphProp))
            changed = true;
    }

    if (changed)
        bot->SendTalentsInfoData();
}

} // namespace BotRotation
