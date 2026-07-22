/*
 * MoP talent loadouts for .playerbots init.
 *
 * MoP grants one talent per tier at levels 15/30/45/60/75/90 (Talent.dbc rows
 * 0..5). We reset the active-spec talent picks, then LearnTalent() exactly one
 * choice per unlocked row via Player::CalculateTalentsPoints().
 */

#include "BotRotation.h"

#include "DBCStores.h"
#include "DBCStructure.h"
#include "Player.h"
#include "SharedDefines.h"

namespace BotRotation
{
namespace
{
    uint8 const MAX_MOP_TALENT_ROWS = 6;

    // Preferred Talent.dbc IDs per row [0..5] for a specialization.
    // 0 = fall back to the first talent listed for that class/row in Talent.dbc.
    struct SpecTalentLoadout
    {
        uint32 specId;
        uint16 talentIds[MAX_MOP_TALENT_ROWS];
    };

    // Picks tuned for dungeon bots / existing rotation priorities.
    SpecTalentLoadout const s_loadouts[] =
    {
        // Priest
        { SPEC_PRIEST_SHADOW,      { 19752, 19756, 19761, 19762, 19765, 19763 } }, // Void Tendrils, Body and Soul, Solace, Desperate Prayer, PI, Halo
        { SPEC_PRIEST_DISCIPLINE,  { 19752, 19757, 19769, 19762, 19765, 19767 } }, // Void Tendrils, Angelic Feather, Mindbender, DP, PI, Cascade
        { SPEC_PRIEST_HOLY,        { 19752, 19757, 19769, 19762, 19766, 19767 } }, // Void Tendrils, Angelic Feather, Mindbender, DP, Divine Insight, Cascade

        // Paladin
        { SPEC_PALADIN_RETRIBUTION,{ 17565, 17575, 17583, 17589, 17597, 17609 } }, // Speed of Light, Repentance, Eternal Flame, Hand of Purity, Holy Avenger, ES
        { SPEC_PALADIN_HOLY,       { 17565, 17575, 17583, 17589, 17597, 17607 } }, // Speed of Light, Repentance, Eternal Flame, Hand of Purity, Holy Avenger, LH
        { SPEC_PALADIN_PROTECTION, { 17565, 17573, 21098, 17589, 17597, 17605 } }, // Speed of Light, Fist of Justice, Sacred Shield, Hand of Purity, Holy Avenger, Holy Prism

        // Monk
        { SPEC_MONK_WINDWALKER,    { 19818, 20185, 19992, 19995, 20175, 20184 } }, // Tiger's Lust, Chi Wave, Power Strikes, Leg Sweep, Dampen Harm, Xuen
        { SPEC_MONK_BREWMASTER,    { 19818, 20185, 19772, 19995, 20175, 19819 } }, // Tiger's Lust, Chi Wave, Chi Brew, Leg Sweep, Dampen Harm, RJW
        { SPEC_MONK_MISTWEAVER,    { 19818, 20185, 19771, 19995, 20173, 19819 } }, // Tiger's Lust, Chi Wave, Ascension, Leg Sweep, Diffuse Magic, RJW

        // Hunter
        { SPEC_HUNTER_BEAST_MASTERY,{ 19364, 19347, 19356, 19353, 19360, 19357 } }, // Pathing, Binding Shot, Iron Hawk, Dire Beast, Murder of Crows, Glaive Toss
        { SPEC_HUNTER_MARKSMANSHIP,{ 19364, 19347, 19356, 19353, 19360, 19349 } }, // Pathing, Binding Shot, Iron Hawk, Dire Beast, MoC, Barrage
        { SPEC_HUNTER_SURVIVAL,    { 19364, 19347, 19356, 19353, 19360, 19358 } }, // Pathing, Binding Shot, Iron Hawk, Dire Beast, MoC, Powershot

        // Warlock
        { SPEC_WARLOCK_AFFLICTION, { 19280, 19285, 19288, 19291, 19295, 19298 } }, // Soul Leech, Mortal Coil, Sacrificial Pact, Burning Rush, GoSac, Mannoroth's Fury
        { SPEC_WARLOCK_DEMONOLOGY, { 19280, 19285, 19288, 19291, 19295, 19298 } },
        { SPEC_WARLOCK_DESTRUCTION,{ 19280, 19286, 19288, 19291, 19295, 19298 } }, // Soul Leech, Shadowfury, Sac Pact, Burning Rush, GoSac, Mannoroth

        // Shaman
        { SPEC_SHAMAN_ELEMENTAL,   { 19264, 19259, 19275, 19271, 19269, 19267 } }, // Astral Shift, Frozen Power, Call of Elements, EM, Ancestral Guidance, Elemental Blast
        { SPEC_SHAMAN_ENHANCEMENT, { 19264, 19259, 19275, 19273, 19270, 19266 } }, // Astral Shift, Frozen Power, Call of Elements, Echo, Unleashed Fury, Primal Elementalist
        { SPEC_SHAMAN_RESTORATION, { 19264, 19260, 19275, 19272, 19269, 19267 } }, // Astral Shift, Earthgrab, Call of Elements, Ancestral Swiftness, AG, Elemental Blast

        // Druid
        { SPEC_DRUID_BALANCE,      { 18570, 19283, 18576, 18579, 18582, 18584 } }, // Wild Charge, Renewal, Mass Entanglement, Incarnation, Ursol's Vortex, HotW
        { SPEC_DRUID_FERAL,        { 18569, 19283, 18576, 18579, 18583, 18584 } }, // Feline Swiftness, Renewal, Mass Entangle, Incarnation, Mighty Bash, HotW
        { SPEC_DRUID_GUARDIAN,     { 18570, 19283, 18576, 18579, 18581, 18584 } }, // Wild Charge, Renewal, Mass Entangle, Incarnation, Disorienting Roar, HotW
        { SPEC_DRUID_RESTORATION,  { 18570, 18574, 18576, 18579, 18582, 18584 } }, // Wild Charge, Cenarion Ward, Mass Entangle, Incarnation, Ursol, HotW

        // Warrior
        { SPEC_WARRIOR_ARMS,       { 15774, 15757, 15769, 15759, 15766, 19138 } }, // Double Time, Second Wind, Piercing Howl, Bladestorm, Safeguard, Avatar
        { SPEC_WARRIOR_FURY,       { 15774, 15757, 15769, 16037, 15766, 19139 } }, // Double Time, Second Wind, Piercing Howl, Dragon Roar, Safeguard, Bloodbath
        { SPEC_WARRIOR_PROTECTION, { 15774, 16036, 15768, 15760, 19676, 19138 } }, // Double Time, Enraged Regen, Staggering Shout, Shockwave, Vigilance, Avatar

        // Rogue
        { SPEC_ROGUE_ASSASSINATION,{ 19235, 19238, 19239, 19243, 19247, 19249 } }, // Shadow Focus, Combat Readiness, Cheat Death, Shadowstep, Nerve Strike, MfD
        { SPEC_ROGUE_COMBAT,       { 19235, 19238, 19239, 19243, 19247, 19249 } },
        { SPEC_ROGUE_SUBTLETY,     { 19234, 19238, 19239, 19243, 19247, 19250 } }, // Subterfuge, Combat Readiness, Cheat Death, Shadowstep, Nerve Strike, Anticipation

        // Mage
        { SPEC_MAGE_ARCANE,        { 16013, 16025, 16019, 16029, 19299, 16032 } }, // Ice Floes, Ice Barrier, Ring of Frost, Cold Snap, Nether Tempest, Rune of Power
        { SPEC_MAGE_FIRE,          { 16013, 16025, 16019, 16029, 19300, 16032 } }, // Ice Floes, Ice Barrier, Ring of Frost, Cold Snap, Living Bomb, Rune of Power
        { SPEC_MAGE_FROST,         { 16013, 16025, 16019, 16029, 19301, 16032 } }, // Ice Floes, Ice Barrier, Ring of Frost, Cold Snap, Frost Bomb, Rune of Power

        // Death Knight
        { SPEC_DEATH_KNIGHT_BLOOD, { 19217, 19219, 19223, 19224, 19226, 19230 } }, // Unholy Blight, AMZ, Asphyxiate, Death Pact, Blood Tap, Gorefiend's Grasp
        { SPEC_DEATH_KNIGHT_FROST, { 19217, 19219, 19223, 19224, 19226, 19231 } }, // Unholy Blight, AMZ, Asphyxiate, Death Pact, Blood Tap, Remorseless Winter
        { SPEC_DEATH_KNIGHT_UNHOLY,{ 19217, 19219, 19223, 19224, 19226, 19232 } }, // Unholy Blight, AMZ, Asphyxiate, Death Pact, Blood Tap, Desecrated Ground
    };

    uint16 const* FindLoadout(uint32 specId)
    {
        for (SpecTalentLoadout const& loadout : s_loadouts)
            if (loadout.specId == specId)
                return loadout.talentIds;
        return nullptr;
    }

    uint16 FallbackTalentForRow(uint8 cls, uint8 row)
    {
        for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
        {
            TalentEntry const* talent = sTalentStore.LookupEntry(i);
            if (!talent || talent->playerClass != cls || talent->Row != row || !talent->SpellId)
                continue;
            return uint16(talent->TalentID);
        }
        return 0;
    }

    bool TalentMatchesClass(uint16 talentId, uint8 cls)
    {
        TalentEntry const* talent = sTalentStore.LookupEntry(talentId);
        return talent && talent->playerClass == cls && talent->SpellId != 0;
    }
}

void ApplyRecommendedTalents(Player* bot)
{
    if (!bot || bot->getLevel() < 15)
        return;

    // Refresh unlocked tier count, then clear any previous (often invalid) picks.
    bot->InitTalentForLevel();
    bot->ResetTalents(true, true, false);
    bot->InitTalentForLevel();

    uint32 const unlocked = bot->CalculateTalentsPoints(); // rows 0 .. unlocked-1
    if (!unlocked)
        return;

    uint8 const cls = bot->getClass();
    uint32 const spec = bot->GetTalentSpecialization(bot->GetActiveSpec());
    uint16 const* preferred = FindLoadout(spec);

    bool learnedAny = false;
    for (uint32 row = 0; row < unlocked && row < MAX_MOP_TALENT_ROWS; ++row)
    {
        uint16 talentId = 0;
        if (preferred && preferred[row] && TalentMatchesClass(preferred[row], cls))
        {
            TalentEntry const* talent = sTalentStore.LookupEntry(preferred[row]);
            if (talent && talent->Row == row)
                talentId = preferred[row];
        }
        if (!talentId)
            talentId = FallbackTalentForRow(cls, uint8(row));
        if (!talentId)
            continue;

        if (bot->LearnTalent(talentId))
            learnedAny = true;
    }

    if (learnedAny)
        bot->SendTalentsInfoData();
}

} // namespace BotRotation
