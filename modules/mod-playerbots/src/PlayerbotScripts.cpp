/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*
* Playerbots module - core script hooks and admin commands.
*/

#include "Chat.h"
#include "Group.h"
#include "GroupReference.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotMgr.h"
#include "RBAC.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

#include <vector>

// Routes per-tick AI updates to the PlayerbotMgr for bot-controlled players.
class playerbot_player_script : public PlayerScript
{
public:
    playerbot_player_script() : PlayerScript("playerbot_player_script") { }

    void OnUpdate(Player* player, uint32 diff) override
    {
        if (player && player->GetSession() && player->GetSession()->IsBot())
            sPlayerbotMgr->UpdateBotAI(player, diff);
    }
};

// Loads/refreshes the module configuration together with the world config and
// tears down bot sessions cleanly on shutdown.
class playerbot_world_script : public WorldScript
{
public:
    playerbot_world_script() : WorldScript("playerbot_world_script") { }

    void OnConfigLoad(bool /*reload*/) override
    {
        sPlayerbotMgr->LoadConfig();
    }

    void OnStartup() override
    {
        // Optionally provision bot accounts/characters once the world is fully
        // loaded (all DBC/db data available for valid race/class/name checks).
        if (sPlayerbotMgr->IsEnabled() && sPlayerbotMgr->IsAutoCreateOnStartup())
        {
            std::string report;
            sPlayerbotMgr->CreateBotPopulation(&report);
            SF_LOG_INFO("modules", "[mod-playerbots] %s", report.c_str());
        }
    }

    void OnUpdate(uint32 diff) override
    {
        sPlayerbotMgr->Update(diff);
    }

    void OnShutdown() override
    {
        sPlayerbotMgr->LogoutAllBots();
    }
};

// ".playerbots ..." administration commands.
class playerbot_commandscript : public CommandScript
{
public:
    playerbot_commandscript() : CommandScript("playerbot_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> playerbotCommandTable =
        {
            { "status", rbac::RBAC_PERM_COMMAND_GM, true, &HandlePlayerbotStatusCommand, "", },
            { "add",    rbac::RBAC_PERM_COMMAND_GM, true, &HandlePlayerbotAddCommand,    "", },
            { "remove", rbac::RBAC_PERM_COMMAND_GM, true, &HandlePlayerbotRemoveCommand, "", },
            { "summon", rbac::RBAC_PERM_COMMAND_GM, false, &HandlePlayerbotSummonCommand, "", },
            { "list",   rbac::RBAC_PERM_COMMAND_GM, true, &HandlePlayerbotListCommand,   "", },
            { "reload", rbac::RBAC_PERM_COMMAND_GM, true, &HandlePlayerbotReloadCommand, "", },
            { "create", rbac::RBAC_PERM_COMMAND_GM, true, &HandlePlayerbotCreateCommand, "", },
        };

        static std::vector<ChatCommand> commandTable =
        {
            { "playerbots", rbac::RBAC_PERM_COMMAND_GM, true, NULL, "", playerbotCommandTable },
        };
        return commandTable;
    }

    static bool HandlePlayerbotStatusCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->PSendSysMessage("Playerbots module: %s.",
            sPlayerbotMgr->IsEnabled() ? "enabled" : "disabled");
        handler->PSendSysMessage("Random bots: %s (%u/%u, %u candidate(s)).",
            sPlayerbotMgr->IsRandomBotsEnabled() ? "enabled" : "disabled",
            sPlayerbotMgr->GetRandomBotCount(), sPlayerbotMgr->GetMaxRandomBots(),
            sPlayerbotMgr->GetCandidateCount());
        handler->PSendSysMessage("Active bots: %u.", sPlayerbotMgr->GetActiveBotCount());
        return true;
    }

    static bool HandlePlayerbotAddCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
            return false;

        std::string name = args;
        if (!normalizePlayerName(name))
        {
            handler->SendSysMessage("Invalid character name.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint64 guid = sObjectMgr->GetPlayerGUIDByName(name);
        if (!guid)
        {
            handler->PSendSysMessage("Character '%s' not found.", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string error;
        if (!sPlayerbotMgr->AddBot(guid, &error))
        {
            handler->PSendSysMessage("Could not add bot '%s': %s", name.c_str(), error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Bot '%s' logged in.", name.c_str());
        return true;
    }

    static bool HandlePlayerbotRemoveCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
            return false;

        std::string name = args;
        if (!normalizePlayerName(name))
        {
            handler->SendSysMessage("Invalid character name.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint64 guid = sObjectMgr->GetPlayerGUIDByName(name);
        if (!guid || !sPlayerbotMgr->RemoveBot(guid))
        {
            handler->PSendSysMessage("'%s' is not an active bot.", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Bot '%s' logged out.", name.c_str());
        return true;
    }

    static bool HandlePlayerbotSummonCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!master)
        {
            handler->SendSysMessage("This command must be used in-game.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Group* group = master->GetGroup();
        if (!group)
        {
            handler->SendSysMessage("You are not in a group with any bots.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 count = 0;
        for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || member == master)
                continue;
            if (!sPlayerbotMgr->IsBot(member->GetGUID()))
                continue;

            member->GetMotionMaster()->Clear();
            member->GetMotionMaster()->MoveIdle();
            if (member->TeleportTo(master->GetMapId(), master->GetPositionX(), master->GetPositionY(),
                master->GetPositionZ(), master->GetOrientation()))
            {
                // Bots have no client to ack the teleport; finalize it now.
                member->GetSession()->FinalizeBotTeleport();
                ++count;
            }
        }

        handler->PSendSysMessage("Summoned %u bot(s) to your position.", count);
        return true;
    }

    static bool HandlePlayerbotReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        sPlayerbotMgr->LoadConfig();
        sPlayerbotMgr->ReloadCandidates();
        handler->SendSysMessage("Playerbots configuration and candidate pool reloaded.");
        return true;
    }

    static bool HandlePlayerbotCreateCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!sPlayerbotMgr->IsEnabled())
        {
            handler->SendSysMessage("Playerbots module is disabled (set Playerbots.Enable = 1).");
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->SendSysMessage("Provisioning bot accounts and characters; this may take a moment...");

        std::string report;
        sPlayerbotMgr->CreateBotPopulation(&report);
        handler->PSendSysMessage("%s", report.c_str());
        return true;
    }

    static bool HandlePlayerbotListCommand(ChatHandler* handler, char const* /*args*/)
    {
        std::vector<uint64> guids;
        sPlayerbotMgr->GetBotGuids(guids);

        handler->PSendSysMessage("Active bots: %u", uint32(guids.size()));
        for (uint64 guid : guids)
        {
            std::string name;
            if (!sObjectMgr->GetPlayerNameByGUID(guid, name))
                name = "<unknown>";
            handler->PSendSysMessage("  %s (GUID %u)", name.c_str(), GUID_LOPART(guid));
        }
        return true;
    }
};

void AddSC_playerbot_scripts()
{
    new playerbot_world_script();
    new playerbot_player_script();
    new playerbot_commandscript();
}
