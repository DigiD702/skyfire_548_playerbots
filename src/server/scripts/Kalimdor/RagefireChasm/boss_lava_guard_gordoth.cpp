/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "InstanceScript.h"
#include "ragefire_chasm.h"

enum GordothSpells
{
    SPELL_ENRAGE         = 145692,
    SPELL_SIESMIC_SLAM   = 120024,
    SPELL_GROUND_RUPTURE = 119999
};

enum GordothEvents
{
    EVENT_SLAM    = 1,
    EVENT_RUPTURE = 2
};

class boss_lava_guard_gordoth : public CreatureScript
{
public:
    boss_lava_guard_gordoth() : CreatureScript("boss_lava_guard_gordoth") { }

    struct boss_lava_guard_gordothAI : public BossAI
    {
        boss_lava_guard_gordothAI(Creature* creature) : BossAI(creature, BOSS_GORDOTH), hasEnrage(false) { }

        bool hasEnrage;

        void Reset() OVERRIDE
        {
            _Reset();
            events.Reset();
            summons.DespawnAll();
            hasEnrage = false;
            if (instance)
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        }

        void EnterEvadeMode() OVERRIDE
        {
            BossAI::EnterEvadeMode();
            if (instance)
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        }

        void EnterCombat(Unit* /*who*/) OVERRIDE
        {
            _EnterCombat();

            if (instance)
            {
                instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me);
                instance->SetBossState(BOSS_GORDOTH, IN_PROGRESS);
            }

            events.ScheduleEvent(EVENT_RUPTURE, urand(5 * IN_MILLISECONDS, 8 * IN_MILLISECONDS));
            events.ScheduleEvent(EVENT_SLAM, urand(20 * IN_MILLISECONDS, 22 * IN_MILLISECONDS));
        }

        void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/) OVERRIDE
        {
            if (HealthBelowPct(30) && !hasEnrage)
            {
                hasEnrage = true;
                me->CastSpell(me, SPELL_ENRAGE, false);
            }
        }

        void JustDied(Unit* /*killer*/) OVERRIDE
        {
            _JustDied();
            if (instance)
            {
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
                instance->SetBossState(BOSS_GORDOTH, DONE);
            }
        }

        void UpdateAI(uint32 diff) OVERRIDE
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_SLAM:
                        DoCast(me, SPELL_SIESMIC_SLAM);
                        events.ScheduleEvent(EVENT_SLAM, urand(20 * IN_MILLISECONDS, 22 * IN_MILLISECONDS));
                        break;
                    case EVENT_RUPTURE:
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, NonTankTargetSelector(me)))
                            me->CastSpell(target, SPELL_GROUND_RUPTURE, false);
                        else if (Unit* target = me->GetVictim())
                            me->CastSpell(target, SPELL_GROUND_RUPTURE);
                        events.ScheduleEvent(EVENT_RUPTURE, urand(5 * IN_MILLISECONDS, 8 * IN_MILLISECONDS));
                        break;
                    default:
                        break;
                }
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const OVERRIDE
    {
        return new boss_lava_guard_gordothAI(creature);
    }
};

void AddSC_boss_lava_guard_gordoth()
{
    new boss_lava_guard_gordoth();
}
