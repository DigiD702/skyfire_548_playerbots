/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "Creature.h"
#include "Map.h"
#include "ragefire_chasm.h"

class instance_ragefire_chasm : public InstanceMapScript
{
public:
    instance_ragefire_chasm() : InstanceMapScript("instance_ragefire_chasm", 389) { }

    struct instance_ragefire_chasm_InstanceMapScript : public InstanceScript
    {
        instance_ragefire_chasm_InstanceMapScript(Map* map) : InstanceScript(map)
        {
            SetBossNumber(TOTAL_ENCOUNTERS);
            AdaroggGUID = 0;
            CoranthalGUID = 0;
            SlagmawGUID = 0;
            GordothGUID = 0;
            InfernoTargetGUID = 0;
        }

        void OnCreatureCreate(Creature* creature) OVERRIDE
        {
            switch (creature->GetEntry())
            {
                case NPC_ADAROGG:
                    AdaroggGUID = creature->GetGUID();
                    break;
                case NPC_DARK_SHAMAN_CORANTHAL:
                    CoranthalGUID = creature->GetGUID();
                    break;
                case NPC_SLAGMAW:
                    SlagmawGUID = creature->GetGUID();
                    break;
                case NPC_GORDOTH:
                    GordothGUID = creature->GetGUID();
                    break;
                default:
                    break;
            }
        }

        void SetData64(uint32 type, uint64 data) OVERRIDE
        {
            if (type == TYPE_INFERNO_TARGET)
                InfernoTargetGUID = data;
        }

        uint64 GetData64(uint32 type) const OVERRIDE
        {
            switch (type)
            {
                case BOSS_ADAROGG:
                    return AdaroggGUID;
                case BOSS_DARK_SHAMAN_CORANTHAL:
                    return CoranthalGUID;
                case BOSS_SLAGMAW:
                    return SlagmawGUID;
                case BOSS_GORDOTH:
                    return GordothGUID;
                case TYPE_INFERNO_TARGET:
                    return InfernoTargetGUID;
                default:
                    return 0;
            }
        }

        std::string GetSaveData() OVERRIDE
        {
            OUT_SAVE_INST_DATA;

            std::ostringstream saveStream;
            saveStream << "R F C " << GetBossSaveData();

            OUT_SAVE_INST_DATA_COMPLETE;
            return saveStream.str();
        }

        void Load(char const* in) OVERRIDE
        {
            if (!in)
            {
                OUT_LOAD_INST_DATA_FAIL;
                return;
            }

            OUT_LOAD_INST_DATA(in);

            char dataHead1, dataHead2, dataHead3;
            std::istringstream loadStream(in);
            loadStream >> dataHead1 >> dataHead2 >> dataHead3;

            if (dataHead1 == 'R' && dataHead2 == 'F' && dataHead3 == 'C')
            {
                for (uint8 i = 0; i < TOTAL_ENCOUNTERS; ++i)
                {
                    uint32 tmpState;
                    loadStream >> tmpState;
                    if (tmpState == IN_PROGRESS || tmpState > SPECIAL)
                        tmpState = NOT_STARTED;
                    SetBossState(i, EncounterState(tmpState));
                }
            }
            else
                OUT_LOAD_INST_DATA_FAIL;

            OUT_LOAD_INST_DATA_COMPLETE;
        }

    protected:
        uint64 AdaroggGUID;
        uint64 CoranthalGUID;
        uint64 SlagmawGUID;
        uint64 GordothGUID;
        uint64 InfernoTargetGUID;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const OVERRIDE
    {
        return new instance_ragefire_chasm_InstanceMapScript(map);
    }
};

void AddSC_instance_ragefire_chasm()
{
    new instance_ragefire_chasm();
}
