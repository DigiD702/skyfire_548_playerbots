/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "InstanceScript.h"
#include "Containers.h"
#include "Object.h"
#include "ragefire_chasm.h"
#include <vector>
#include <algorithm>

Position const SlagMawPoints[3] =
{
    { -256.643f, 172.957f, -16.253f, 5.383f },
    { -261.183f, 137.642f, -12.348f, 0.574f },
    { -225.866f, 164.639f, -15.437f, 3.885f }
};

enum SlagmawSpells
{
    SPELL_LAVA_SPIT         = 119434,
    SPELL_COSMETIC_SUBMERGE = 140483,
    SPELL_SUBMERGE          = 120384
};

enum SpagmawEvents
{
    EVENT_LAVA_SPIT = 1,
    EVENT_SUBMERGE  = 2
};

class boss_slagmaw : public CreatureScript
{
public:
    boss_slagmaw() : CreatureScript("boss_slagmaw") { }

    struct boss_slagmawAI : public BossAI
    {
        boss_slagmawAI(Creature* creature) : BossAI(creature, BOSS_SLAGMAW), m_cPoint(1)
        {
            SetCombatMovement(false);
        }

        uint8 m_cPoint;

        void Reset() OVERRIDE
        {
            _Reset();
            events.Reset();
            summons.DespawnAll();
            m_cPoint = 1;
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
                instance->SetBossState(BOSS_SLAGMAW, IN_PROGRESS);
            }

            events.ScheduleEvent(EVENT_LAVA_SPIT, urand(6 * IN_MILLISECONDS, 9 * IN_MILLISECONDS));
            events.ScheduleEvent(EVENT_SUBMERGE, 20 * IN_MILLISECONDS);
        }

        void MovementInform(uint32 type, uint32 pointId) OVERRIDE
        {
            if (type != POINT_MOTION_TYPE)
                return;

            if (pointId == 0)
            {
                me->CastSpell(me, SPELL_COSMETIC_SUBMERGE, true);
                me->RemoveAurasDueToSpell(SPELL_SUBMERGE);
            }
        }

        void JustDied(Unit* /*killer*/) OVERRIDE
        {
            _JustDied();
            if (instance)
            {
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
                instance->SetBossState(BOSS_SLAGMAW, DONE);
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
                    case EVENT_LAVA_SPIT:
                        if (me->HasAura(SPELL_SUBMERGE) || me->HasAura(SPELL_COSMETIC_SUBMERGE))
                        {
                            events.RescheduleEvent(EVENT_LAVA_SPIT, 5 * IN_MILLISECONDS);
                            break;
                        }
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, NonTankTargetSelector(me)))
                            me->CastSpell(target, SPELL_LAVA_SPIT, false);
                        else if (Unit* target = me->GetVictim())
                            me->CastSpell(target, SPELL_LAVA_SPIT);
                        events.ScheduleEvent(EVENT_LAVA_SPIT, urand(6 * IN_MILLISECONDS, 9 * IN_MILLISECONDS));
                        break;
                    case EVENT_SUBMERGE:
                    {
                        std::vector<uint8> points = { 1, 2, 3 };
                        if (m_cPoint)
                            points.erase(std::find(points.begin(), points.end(), m_cPoint));
                        m_cPoint = Skyfire::Containers::SelectRandomContainerElement(points);
                        me->CastSpell(me, SPELL_SUBMERGE, false);
                        me->GetMotionMaster()->MovePoint(0, SlagMawPoints[m_cPoint - 1]);
                        events.ScheduleEvent(EVENT_SUBMERGE, 30 * IN_MILLISECONDS);
                        break;
                    }
                    default:
                        break;
                }
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const OVERRIDE
    {
        return new boss_slagmawAI(creature);
    }
};

void AddSC_boss_slagmaw()
{
    new boss_slagmaw();
}
