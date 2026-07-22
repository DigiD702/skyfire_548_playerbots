/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*
* Playerbots module - core script hooks and admin commands.
*/

#include "Chat.h"
#include "DBCStores.h"
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
#include "SharedDefines.h"
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

    // Parses a role or specialization token for .playerbots init.
    // Roles: tank / healer / dps. Specs: elemental, enhancement, feral, moonkin, etc.
    static bool ParseRoleOrSpecToken(std::string token, int& roleOut, uint32& specOut)
    {
        std::transform(token.begin(), token.end(), token.begin(),
            [](unsigned char c) { return char(std::tolower(c)); });

        roleOut = -1;
        specOut = 0;

        if (token == "tank")
        {
            roleOut = 0;
            return true;
        }
        if (token == "healer" || token == "heal" || token == "heals" || token == "resto" || token == "restoration")
        {
            // "resto" alone is ambiguous across classes - treat as healer role
            // so SpecIdForRole picks the class's resto tab. Explicit class specs
            // below still win when the token is unique (e.g. mistweaver).
            if (token == "resto" || token == "restoration")
            {
                // Prefer role mapping; per-class resto specs use the same word.
                roleOut = 1;
                return true;
            }
            roleOut = 1;
            return true;
        }
        if (token == "dps" || token == "damage" || token == "dd")
        {
            roleOut = 2;
            return true;
        }

        // Spec names (ChrSpecialization ids). Hybrids need these so DPS is not
        // stuck on the default tab (e.g. shaman ele vs enh, druid balance vs feral).
        if (token == "elemental" || token == "ele")
            specOut = SPEC_SHAMAN_ELEMENTAL;
        else if (token == "enhancement" || token == "enh" || token == "enhance")
            specOut = SPEC_SHAMAN_ENHANCEMENT;
        else if (token == "balance" || token == "moonkin" || token == "boomkin" || token == "boomie")
            specOut = SPEC_DRUID_BALANCE;
        else if (token == "feral")
            specOut = SPEC_DRUID_FERAL;
        else if (token == "guardian")
            specOut = SPEC_DRUID_GUARDIAN;
        else if (token == "retribution" || token == "ret")
            specOut = SPEC_PALADIN_RETRIBUTION;
        else if (token == "protection" || token == "prot")
        {
            // Ambiguous across warrior/pala/monk - leave as tank role.
            roleOut = 0;
            return true;
        }
        else if (token == "holy")
        {
            // Ambiguous (pala/priest); use healer role so SpecIdForRole is class-aware.
            roleOut = 1;
            return true;
        }
        else if (token == "shadow")
            specOut = SPEC_PRIEST_SHADOW;
        else if (token == "discipline" || token == "disc")
            specOut = SPEC_PRIEST_DISCIPLINE;
        else if (token == "windwalker" || token == "ww")
            specOut = SPEC_MONK_WINDWALKER;
        else if (token == "brewmaster" || token == "brm")
            specOut = SPEC_MONK_BREWMASTER;
        else if (token == "mistweaver" || token == "mw")
            specOut = SPEC_MONK_MISTWEAVER;
        else if (token == "beastmastery" || token == "beastmaster" || token == "bm")
            specOut = SPEC_HUNTER_BEAST_MASTERY;
        else if (token == "marksmanship" || token == "marks" || token == "mm")
            specOut = SPEC_HUNTER_MARKSMANSHIP;
        else if (token == "survival" || token == "surv")
            specOut = SPEC_HUNTER_SURVIVAL;
        else if (token == "affliction" || token == "aff")
            specOut = SPEC_WARLOCK_AFFLICTION;
        else if (token == "demonology" || token == "demo")
            specOut = SPEC_WARLOCK_DEMONOLOGY;
        else if (token == "destruction" || token == "destro")
            specOut = SPEC_WARLOCK_DESTRUCTION;
        else if (token == "arcane")
            specOut = SPEC_MAGE_ARCANE;
        else if (token == "fire")
            specOut = SPEC_MAGE_FIRE;
        else if (token == "frost")
            specOut = SPEC_MAGE_FROST;
        else if (token == "arms")
            specOut = SPEC_WARRIOR_ARMS;
        else if (token == "fury")
            specOut = SPEC_WARRIOR_FURY;
        else if (token == "assassination" || token == "mut")
            specOut = SPEC_ROGUE_ASSASSINATION;
        else if (token == "combat")
            specOut = SPEC_ROGUE_COMBAT;
        else if (token == "subtlety" || token == "sub")
            specOut = SPEC_ROGUE_SUBTLETY;
        else if (token == "blood")
            specOut = SPEC_DEATH_KNIGHT_BLOOD;
        else if (token == "unholy")
            specOut = SPEC_DEATH_KNIGHT_UNHOLY;
        else
            return false;

        return specOut != 0;
    }

    static bool SpecMatchesClass(uint32 specId, uint8 cls)
    {
        ChrSpecializationEntry const* entry = sChrSpecializationStore.LookupEntry(specId);
        return entry && entry->classId == cls;
    }

    // .playerbots init [<charname>|all|self] [tank|healer|dps|<spec>]
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
        uint32 specOverride = 0;
        bool haveChoice = false;
        auto applyToken = [&](std::string const& token) -> bool
        {
            int role = -1;
            uint32 spec = 0;
            if (!ParseRoleOrSpecToken(token, role, spec))
            {
                handler->PSendSysMessage(
                    "Unknown role/spec '%s'. Use tank, healer, dps, or a spec name "
                    "(e.g. elemental, enhancement, feral, moonkin, ret, shadow).",
                    token.c_str());
                handler->SetSentErrorMessage(true);
                return false;
            }
            roleOverride = role;
            specOverride = spec;
            haveChoice = true;
            return true;
        };

        if (!second.empty())
        {
            if (!applyToken(second))
                return false;
        }
        else if (!first.empty())
        {
            int role = -1;
            uint32 spec = 0;
            if (ParseRoleOrSpecToken(first, role, spec))
            {
                roleOverride = role;
                specOverride = spec;
                haveChoice = true;
                first.clear();
            }
        }

        Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        char const* choiceNote = haveChoice
            ? (specOverride ? " with the requested spec" : " with the requested role")
            : "";

        if (first == "all")
        {
            uint32 count = sPlayerbotMgr->InitializeAllBots(roleOverride, specOverride);
            handler->PSendSysMessage("Initialized %u active bot(s)%s.", count, choiceNote);
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
            if (specOverride && !SpecMatchesClass(specOverride, master->getClass()))
            {
                handler->SendSysMessage("That specialization does not match your class.");
                handler->SetSentErrorMessage(true);
                return false;
            }
            sPlayerbotMgr->InitializeBot(master, roleOverride, specOverride);
            handler->PSendSysMessage("Initialized yourself%s.", choiceNote);
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
            if (specOverride && !SpecMatchesClass(specOverride, bot->getClass()))
            {
                handler->PSendSysMessage("That specialization does not match %s's class.", name.c_str());
                handler->SetSentErrorMessage(true);
                return false;
            }

            sPlayerbotMgr->InitializeBot(bot, roleOverride, specOverride);
            handler->PSendSysMessage("Initialized bot '%s'%s.", name.c_str(), choiceNote);
            return true;
        }

        if (!master)
        {
            handler->SendSysMessage(
                "Usage: .playerbots init [<charname>|all|self] [tank|healer|dps|<spec>].");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (specOverride && !SpecMatchesClass(specOverride, master->getClass()))
        {
            handler->SendSysMessage("That specialization does not match your class.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sPlayerbotMgr->InitializeBot(master, roleOverride, specOverride);
        uint32 count = 1;

        if (Group* group = master->GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (member && member != master && sPlayerbotMgr->IsBot(member->GetGUID()))
                {
                    if (specOverride && !SpecMatchesClass(specOverride, member->getClass()))
                        continue;
                    sPlayerbotMgr->InitializeBot(member, roleOverride, specOverride);
                    ++count;
                }
            }
        }

        handler->PSendSysMessage("Initialized yourself and %u grouped bot(s)%s.",
            count - 1, choiceNote);
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
