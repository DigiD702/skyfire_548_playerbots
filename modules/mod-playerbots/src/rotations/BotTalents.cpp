/*
 * Recommended MoP talent spells for Wave-1 DPS specs.
 * Learned via learnSpell so rotation talent checks (HasSpell) succeed.
 * Not a full Talent.dbc spend simulation.
 */

#include "BotRotation.h"
#include "Player.h"
#include "SharedDefines.h"

namespace BotRotation
{
namespace
{
    void LearnSpellList(Player* bot, uint32 const* spells, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            uint32 id = spells[i];
            if (!id)
                continue;
            if (!bot->HasSpell(id))
            {
                bot->learnSpell(id, false);
                bot->AddTalent(id, bot->GetActiveSpec(), true);
            }
        }
    }
}

void ApplyRecommendedTalents(Player* bot)
{
    if (!bot || bot->getLevel() < 15)
        return;

    uint32 const spec = bot->GetTalentSpecialization(bot->GetActiveSpec());

    // Talent ability spell IDs commonly used by Wave-1 priorities.
    switch (spec)
    {
        case SPEC_PALADIN_RETRIBUTION:
        {
            // Holy Avenger, Execution Sentence, Holy Prism, Eternal Flame / Selfless Healer path omitted
            static uint32 const spells[] = {
                105809, // Holy Avenger
                114157, // Execution Sentence
                114852, // Holy Prism (or Lights Hammer alt 114158)
                85499,  // Speed of Light
                114039, // Hand of Purity
                26023,  // Pursuit of Justice (passive-ish)
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_MONK_WINDWALKER:
        {
            static uint32 const spells[] = {
                115098, // Chi Wave
                123904, // Invoke Xuen
                116847, // Rushing Jade Wind
                122278, // Dampen Harm
                121817, // Power Strikes (passive)
                115399, // Chi Brew
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_HUNTER_BEAST_MASTERY:
        {
            static uint32 const spells[] = {
                109260, // Aspect of the Iron Hawk
                120679, // Dire Beast
                131894, // A Murder of Crows
                117050, // Glaive Toss
                82726,  // Fervor
                109248, // Binding Shot
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_PRIEST_SHADOW:
        {
            static uint32 const spells[] = {
                10060,  // Power Infusion
                120517, // Halo
                109186, // From Darkness Comes Light / Surge path
                139139, // Solace and Insanity
                108942, // Phantasm
                64129,  // Body and Soul
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_WARLOCK_AFFLICTION:
        {
            static uint32 const spells[] = {
                108503, // Grimoire of Sacrifice
                111397, // Blood Horror
                108416, // Sacrificial Pact
                6789,   // Mortal Coil
                111400, // Burning Rush
                108482, // Unbound Will
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_SHAMAN_ELEMENTAL:
        {
            static uint32 const spells[] = {
                16166,  // Elemental Mastery
                117014, // Elemental Blast
                108283, // Echo of the Elements
                108271, // Astral Shift
                30884,  // Nature's Guardian
                63374,  // Frozen Power
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_SHAMAN_ENHANCEMENT:
        {
            static uint32 const spells[] = {
                117014, // Elemental Blast
                108283, // Echo of the Elements
                114049, // Ascendance (learned via spec usually)
                30884,  // Nature's Guardian
                16166,  // Elemental Mastery
                117013, // Primal Elementalist
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_DRUID_FERAL:
        {
            static uint32 const spells[] = {
                106731, // Incarnation
                102543, // Incarnation: King of the Jungle
                108288, // Heart of the Wild
                102351, // Cenarion Ward
                132158, // Nature's Swiftness
                106737, // Force of Nature
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_HUNTER_MARKSMANSHIP:
        case SPEC_HUNTER_SURVIVAL:
        {
            static uint32 const spells[] = {
                109260, // Aspect of the Iron Hawk
                120679, // Dire Beast
                131894, // A Murder of Crows
                117050, // Glaive Toss
                109248, // Binding Shot
                120360, // Barrage
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_WARRIOR_ARMS:
        case SPEC_WARRIOR_FURY:
        {
            static uint32 const spells[] = {
                107574, // Avatar
                12292,  // Bloodbath
                118000, // Dragon Roar
                107570, // Storm Bolt
                46924,  // Bladestorm
                12328,  // Sweeping Strikes (arms)
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_ROGUE_COMBAT:
        {
            static uint32 const spells[] = {
                137619, // Marked for Death
                108209, // Shadow Focus
                108208, // Subterfuge
                114015, // Anticipation
                74001,  // Combat Readiness
                31230,  // Cheat Death
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_MAGE_FROST:
        {
            static uint32 const spells[] = {
                114923, // Nether Tempest
                112948, // Frost Bomb
                108839, // Ice Floes
                11958,  // Cold Snap
                11426,  // Ice Barrier
                55342,  // Mirror Image
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_WARLOCK_DESTRUCTION:
        case SPEC_WARLOCK_DEMONOLOGY:
        {
            static uint32 const spells[] = {
                108503, // Grimoire of Sacrifice
                108416, // Sacrificial Pact
                6789,   // Mortal Coil
                111400, // Burning Rush
                108482, // Unbound Will
                30283,  // Shadowfury
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        case SPEC_DEATH_KNIGHT_UNHOLY:
        {
            static uint32 const spells[] = {
                45529,  // Blood Tap
                47568,  // Empower Rune Weapon
                108194, // Asphyxiate
                115989, // Unholy Blight
                51052,  // Anti-Magic Zone
                48707,  // Anti-Magic Shell
            };
            LearnSpellList(bot, spells, sizeof(spells) / sizeof(spells[0]));
            break;
        }
        default:
            break;
    }
}

} // namespace BotRotation
