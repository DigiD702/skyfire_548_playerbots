/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "InstanceScript.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "MotionMaster.h"
#include "ragefire_chasm.h"

enum AdaroggSpells
{
    SPELL_INFERNO_CHARGE_EFF = 119299,
    SPELL_INFERNO_CHARGE     = 119405,
    SPELL_FLAME_BREATH       = 119420
};

enum AdaroggEvents
{
    EVENT_INFERNO_CHARGE = 1,
    EVENT_FLAME_BREATH   = 2
};

class boss_adarogg : public CreatureScript
{
public:
    boss_adarogg() : CreatureScript("boss_adarogg") { }

    struct boss_adaroggAI : public BossAI
    {
        boss_adaroggAI(Creature* creature) : BossAI(creature, BOSS_ADAROGG) { }

        void Reset() OVERRIDE
        {
            _Reset();
            events.Reset();
            summons.DespawnAll();
            if (instance)
            {
                instance->SetData64(TYPE_INFERNO_TARGET, 0);
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
            }
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
                instance->SetBossState(BOSS_ADAROGG, IN_PROGRESS);
            }

            events.ScheduleEvent(EVENT_INFERNO_CHARGE, urand(11 * IN_MILLISECONDS, 12 * IN_MILLISECONDS));
        }

        void MovementInform(uint32 /*type*/, uint32 pointId) OVERRIDE
        {
            if (pointId != EVENT_CHARGE || !instance)
                return;

            if (Player* target = ObjectAccessor::FindPlayer(instance->GetData64(TYPE_INFERNO_TARGET)))
                DoCast(target, SPELL_INFERNO_CHARGE_EFF);
        }

        void JustDied(Unit* /*killer*/) OVERRIDE
        {
            _JustDied();
            if (instance)
            {
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
                instance->SetBossState(BOSS_ADAROGG, DONE);
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
                    case EVENT_INFERNO_CHARGE:
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, NonTankTargetSelector(me)))
                        {
                            me->CastSpell(target, SPELL_INFERNO_CHARGE, false);
                            if (instance)
                                instance->SetData64(TYPE_INFERNO_TARGET, target->GetGUID());
                        }
                        events.ScheduleEvent(EVENT_INFERNO_CHARGE, urand(11 * IN_MILLISECONDS, 12 * IN_MILLISECONDS));
                        events.ScheduleEvent(EVENT_FLAME_BREATH, urand(4 * IN_MILLISECONDS, 6 * IN_MILLISECONDS));
                        break;
                    case EVENT_FLAME_BREATH:
                        if (Unit* victim = me->GetVictim())
                            me->CastSpell(victim, SPELL_FLAME_BREATH, false);
                        break;
                    default:
                        break;
                }
            }

            if (!me->HasAura(SPELL_INFERNO_CHARGE))
                DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const OVERRIDE
    {
        return new boss_adaroggAI(creature);
    }
};

class spell_ragefire_inferno_charge : public SpellScriptLoader
{
public:
    spell_ragefire_inferno_charge() : SpellScriptLoader("spell_ragefire_inferno_charge") { }

    class spell_ragefire_inferno_charge_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_ragefire_inferno_charge_AuraScript);

        void OnAuraEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            InstanceScript* instance = caster->GetInstanceScript();
            if (!instance)
                return;

            Player* target = ObjectAccessor::FindPlayer(instance->GetData64(TYPE_INFERNO_TARGET));
            if (!target)
                return;

            caster->GetMotionMaster()->MoveCharge(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), 42.0f, EVENT_CHARGE);
        }

        void Register() OVERRIDE
        {
            OnEffectRemove += AuraEffectRemoveFn(spell_ragefire_inferno_charge_AuraScript::OnAuraEffectRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const OVERRIDE
    {
        return new spell_ragefire_inferno_charge_AuraScript();
    }
};

void AddSC_boss_adarogg()
{
    new boss_adarogg();
    new spell_ragefire_inferno_charge();
}
