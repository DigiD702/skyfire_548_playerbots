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
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotMgr.h"
#include "RBAC.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

// Routes per-tick AI updates and chat orders to the PlayerbotMgr for
// bot-controlled players.
class playerbot_player_script : public PlayerScript
{
public:
    playerbot_player_script() : PlayerScript("playerbot_player_script") { }

    void OnUpdate(Player* player, uint32 diff) override
    {
        if (!player || !player->GetSession())
            return;

        // Socket bots and self-bots both have an attached PlayerbotAI.
        if (player->GetSession()->IsBot() || sPlayerbotMgr->HasBotAI(player->GetGUID()))
            sPlayerbotMgr->UpdateBotAI(player, diff);
    }

    // Whisper directed at a bot.
    void OnChat(Player* player, ChatMsg /*type*/, Language /*lang*/, std::string& msg, Player* receiver) override
    {
        if (!sPlayerbotMgr->IsEnabled() || !player || !receiver)
            return;
        if (player->GetSession() && player->GetSession()->IsBot())
            return;
        if (!sPlayerbotMgr->IsBot(receiver->GetGUID()))
            return;

        sPlayerbotMgr->HandleBotWhisper(player, receiver, msg);
    }

    // Party / raid chat: every bot in the group hears the order.
    void OnChat(Player* player, ChatMsg /*type*/, Language /*lang*/, std::string& msg, Group* group) override
    {
        if (!sPlayerbotMgr->IsEnabled() || !player || !group)
            return;
        if (player->GetSession() && player->GetSession()->IsBot())
            return;

        sPlayerbotMgr->HandleBotGroupChat(player, group, msg);
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
            { "init",   rbac::RBAC_PERM_COMMAND_GM, true, &HandlePlayerbotInitCommand,   "", },
            { "self",   rbac::RBAC_PERM_COMMAND_GM, false, &HandlePlayerbotSelfCommand,  "", },
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

    // .playerbots self [on|off]
    // Attach cast-only AI to your logged-in character (you keep WASD movement).
    static bool HandlePlayerbotSelfCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
        {
            handler->SendSysMessage("This command must be used in-game.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sPlayerbotMgr->IsEnabled())
        {
            handler->SendSysMessage("Playerbots module is disabled (set Playerbots.Enable = 1).");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string arg = args ? args : "";
        std::transform(arg.begin(), arg.end(), arg.begin(),
            [](unsigned char c) { return char(std::tolower(c)); });

        std::string report;
        if (arg == "off" || arg == "detach" || arg == "disable")
        {
            if (!sPlayerbotMgr->DetachSelfBot(player))
            {
                handler->SendSysMessage("Self-bot AI is not attached.");
                handler->SetSentErrorMessage(true);
                return false;
            }
            handler->SendSysMessage("Self-bot AI detached. You control combat again.");
            return true;
        }

        if (arg == "on" || arg == "attach" || arg == "enable")
        {
            if (!sPlayerbotMgr->AttachSelfBot(player, &report))
            {
                handler->PSendSysMessage("%s", report.c_str());
                handler->SetSentErrorMessage(true);
                return false;
            }
            handler->SendSysMessage("Self-bot AI attached. You move; the AI casts in combat.");
            return true;
        }

        if (!sPlayerbotMgr->ToggleSelfBot(player, &report))
        {
            handler->PSendSysMessage("%s", report.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        handler->PSendSysMessage("%s", report.c_str());
        return true;
    }

    // Parses a role token into the InitializeBot override value
    // (-1 keep, 0 tank, 1 healer, 2 damage). Returns false on an unknown token.
    static bool ParseRoleToken(std::string token, int& roleOut)
    {
        std::transform(token.begin(), token.end(), token.begin(),
            [](unsigned char c) { return char(std::tolower(c)); });

        if (token == "tank")
            roleOut = 0;
        else if (token == "healer" || token == "heal" || token == "heals")
            roleOut = 1;
        else if (token == "dps" || token == "damage" || token == "dd")
            roleOut = 2;
        else
            return false;
        return true;
    }

    // .playerbots init [<charname>|all|self] [tank|healer|dps]
    // Re-applies specialization/spells and gear. With no name, initializes you
    // and any bots in your group.
    static bool HandlePlayerbotInitCommand(ChatHandler* handler, char const* args)
    {
        std::string first;
        std::string second;
        if (args)
        {
            std::istringstream stream(args);
            stream >> first >> second;
        }

        int roleOverride = -1;
        bool haveRole = false;
        if (!second.empty())
        {
            if (!ParseRoleToken(second, roleOverride))
            {
                handler->PSendSysMessage("Unknown role '%s'. Use tank, healer, or dps.", second.c_str());
                handler->SetSentErrorMessage(true);
                return false;
            }
            haveRole = true;
        }
        else if (!first.empty() && ParseRoleToken(first, roleOverride))
        {
            haveRole = true;
            first.clear();
        }

        Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;

        if (first == "all")
        {
            uint32 count = sPlayerbotMgr->InitializeAllBots(haveRole ? roleOverride : -1);
            handler->PSendSysMessage("Initialized %u active bot(s)%s.", count,
                haveRole ? " with the requested role" : "");
            return true;
        }

        if (first == "self" || (!first.empty() && master && first == master->GetName()))
        {
            if (!master)
            {
                handler->SendSysMessage("This command must be used in-game.");
                handler->SetSentErrorMessage(true);
                return false;
            }
            sPlayerbotMgr->InitializeBot(master, roleOverride);
            handler->PSendSysMessage("Initialized yourself%s.",
                haveRole ? " with the requested role" : "");
            return true;
        }

        if (!first.empty())
        {
            std::string name = first;
            if (!normalizePlayerName(name))
            {
                handler->SendSysMessage("Invalid character name.");
                handler->SetSentErrorMessage(true);
                return false;
            }

            uint64 guid = sObjectMgr->GetPlayerGUIDByName(name);
            Player* bot = guid ? ObjectAccessor::FindPlayer(guid) : nullptr;
            if (!bot || (!sPlayerbotMgr->IsBot(bot->GetGUID()) && !sPlayerbotMgr->IsSelfBot(bot->GetGUID())))
            {
                handler->PSendSysMessage("'%s' is not an active bot.", name.c_str());
                handler->SetSentErrorMessage(true);
                return false;
            }

            sPlayerbotMgr->InitializeBot(bot, roleOverride);
            handler->PSendSysMessage("Initialized bot '%s'%s.", name.c_str(),
                haveRole ? " with the requested role" : "");
            return true;
        }

        if (!master)
        {
            handler->SendSysMessage("Usage: .playerbots init [<charname>|all|self] [tank|healer|dps].");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sPlayerbotMgr->InitializeBot(master, roleOverride);
        uint32 count = 1;

        if (Group* group = master->GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (member && member != master && sPlayerbotMgr->IsBot(member->GetGUID()))
                {
                    sPlayerbotMgr->InitializeBot(member, roleOverride);
                    ++count;
                }
            }
        }

        handler->PSendSysMessage("Initialized yourself and %u grouped bot(s)%s.",
            count - 1, haveRole ? " with the requested role" : "");
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
