/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "PlayerbotMgr.h"
#include "PlayerbotAI.h"
#include "rotations/BotRotation.h"
#include "AccountMgr.h"
#include "Config.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "GroupReference.h"
#include "Item.h"
#include "ItemPrototype.h"
#include "LFGMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Unit.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <vector>

PlayerbotMgr* PlayerbotMgr::instance()
{
    static PlayerbotMgr instance;
    return &instance;
}

namespace
{
    // Module .conf files are not loaded by the core automatically; resolve the
    // playerbots.conf next to the main worldserver.conf and merge it in.
    std::string ResolveModuleConfigPath()
    {
        std::string mainConfig = sConfigMgr->GetFilename();
        std::string::size_type slash = mainConfig.find_last_of("/\\");
        std::string directory = (slash != std::string::npos) ? mainConfig.substr(0, slash + 1) : "";
        return directory + "playerbots.conf";
    }
}

void PlayerbotMgr::LoadConfig()
{
    // Merge the module configuration into the shared config store. LoadMore is
    // additive, so this only adds/overrides the Playerbots.* keys.
    std::string configPath = ResolveModuleConfigPath();
    if (!sConfigMgr->LoadMore(configPath.c_str()))
        SF_LOG_ERROR("modules", "[mod-playerbots] Could not read config file '%s'; using defaults (module disabled).",
            configPath.c_str());

    _enabled = sConfigMgr->GetBoolDefault("Playerbots.Enable", false);
    _randomBotsEnabled = sConfigMgr->GetBoolDefault("Playerbots.RandomBots.Enable", false);
    _maxRandomBots = uint32(sConfigMgr->GetIntDefault("Playerbots.RandomBots.MaxBots", 0));
    _accountPrefix = sConfigMgr->GetStringDefault("Playerbots.RandomBots.AccountPrefix", "RNDBOT");
    _loginIntervalMs = uint32(sConfigMgr->GetIntDefault("Playerbots.RandomBots.LoginInterval", 2000));

    _joinLfg = sConfigMgr->GetBoolDefault("Playerbots.RandomBotJoinLfg", false);
    _joinLfgMaxBots = uint32(sConfigMgr->GetIntDefault("Playerbots.RandomBotJoinLfg.MaxBots", 10));
    _joinLfgLevelRange = uint32(sConfigMgr->GetIntDefault("Playerbots.RandomBotJoinLfg.LevelRange", 5));
    if (_joinLfgMaxBots > 40)
        _joinLfgMaxBots = 40;
    if (_joinLfgLevelRange > 20)
        _joinLfgLevelRange = 20;

    _autoCreateOnStartup = sConfigMgr->GetBoolDefault("Playerbots.AutoCreate.OnStartup", false);
    _autoAccountCount = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.AccountCount", 0));
    _autoPassword = sConfigMgr->GetStringDefault("Playerbots.AutoCreate.AccountPassword", "password");
    _autoCharsPerAccount = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.CharactersPerAccount", 1));
    _autoAlliancePct = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.AlliancePct", 50));
    _autoTankPct = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.TankPct", 20));
    _autoHealerPct = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.HealerPct", 20));
    _autoLevel = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.Level", 1));

    // Optional numeric thresholds (strategies themselves are runtime co/nc only).
    _restHealthPct = float(sConfigMgr->GetFloatDefault("Playerbots.Rest.HealthPct", 50.0f));
    _restManaPct = float(sConfigMgr->GetFloatDefault("Playerbots.Rest.ManaPct", 50.0f));
    _almostFullHealthPct = float(sConfigMgr->GetFloatDefault("Playerbots.Rest.AlmostFullHealth", 85.0f));
    _mediumManaPct = float(sConfigMgr->GetFloatDefault("Playerbots.Rest.MediumMana", 40.0f));
    _threatThrottlePct = float(sConfigMgr->GetFloatDefault("Playerbots.Threat.ThrottlePct", 80.0f));
    _fleeDistance = float(sConfigMgr->GetFloatDefault("Playerbots.Flee.Distance", 20.0f));
    _fleeEnabled = sConfigMgr->GetBoolDefault("Playerbots.Flee.Enabled", true);
    _saveManaThreshold = float(sConfigMgr->GetFloatDefault("Playerbots.SaveMana.Threshold", 60.0f));
    _waitForAttackSeconds = uint32(sConfigMgr->GetIntDefault("Playerbots.WaitForAttack.Seconds", 5));

    // Clamp to sane ranges so bad config can't create invalid characters.
    if (_autoAlliancePct > 100) _autoAlliancePct = 100;
    if (_autoTankPct > 100) _autoTankPct = 100;
    if (_autoHealerPct > 100) _autoHealerPct = 100;
    if (_autoTankPct + _autoHealerPct > 100) _autoHealerPct = 100 - _autoTankPct;
    if (_autoCharsPerAccount > 11) _autoCharsPerAccount = 11;   // realm cap
    if (_autoLevel < 1) _autoLevel = 1;
    if (_restHealthPct < 1.0f) _restHealthPct = 1.0f;
    if (_restHealthPct > 100.0f) _restHealthPct = 100.0f;
    if (_restManaPct < 1.0f) _restManaPct = 1.0f;
    if (_restManaPct > 100.0f) _restManaPct = 100.0f;
    if (_almostFullHealthPct < 1.0f) _almostFullHealthPct = 1.0f;
    if (_almostFullHealthPct > 100.0f) _almostFullHealthPct = 100.0f;
    if (_mediumManaPct < 1.0f) _mediumManaPct = 1.0f;
    if (_mediumManaPct > 100.0f) _mediumManaPct = 100.0f;
    if (_threatThrottlePct < 10.0f) _threatThrottlePct = 10.0f;
    if (_threatThrottlePct > 100.0f) _threatThrottlePct = 100.0f;
    if (_fleeDistance < 5.0f) _fleeDistance = 5.0f;
    if (_fleeDistance > 40.0f) _fleeDistance = 40.0f;
    if (_saveManaThreshold < 1.0f) _saveManaThreshold = 1.0f;
    if (_saveManaThreshold > 100.0f) _saveManaThreshold = 100.0f;
    if (_waitForAttackSeconds > 30) _waitForAttackSeconds = 30;

    // Force a candidate reload so prefix changes take effect on .reload config.
    _candidatesLoaded = false;

    if (_enabled)
        SF_LOG_INFO("modules", "[mod-playerbots] Enabled (random bots: %s, max: %u, account prefix: '%s', LFG join: %s).",
            _randomBotsEnabled ? "on" : "off", _maxRandomBots, _accountPrefix.c_str(),
            _joinLfg ? "on" : "off");
    else
        SF_LOG_INFO("modules", "[mod-playerbots] Disabled via configuration.");
}

void PlayerbotMgr::Update(uint32 diff)
{
    if (!_enabled)
        return;

    // Drop bots whose player has left the world for any reason.
    CleanupDeadBots();

    // LFG role checks + proposal accepts must run on the world thread. Calling
    // UpdateProposal from Map::Update (via PlayerbotAI::UpdateAI) races the LFG
    // mgr and can crash in MakeNewGroup / RemoveFromQueue (premade party enter).
    for (auto const& pair : _ai)
    {
        PlayerbotAI* ai = pair.second;
        if (!ai || ai->IsClientControlled())
            continue;
        Player* bot = ai->GetBot();
        if (!bot || !bot->IsInWorld())
            continue;
        ai->HandleLfg();
    }

    // AC-style: when a real player solo-queues LFG, fill with level-matched bots.
    UpdateLfgAutoJoin(diff);

    if (!_randomBotsEnabled)
        return;

    if (!_candidatesLoaded)
        LoadCandidates();

    // Trim excess random bots if the cap was lowered.
    while (_randomBots.size() > _maxRandomBots)
        RemoveBot(*_randomBots.begin());

    // Spawn at most one random bot per interval to spread the login load.
    _loginTimer += diff;
    if (_loginTimer < _loginIntervalMs)
        return;
    _loginTimer = 0;

    if (_randomBots.size() < _maxRandomBots)
        TrySpawnRandomBot();
}

void PlayerbotMgr::LoadCandidates()
{
    _candidates.clear();
    _candidatesLoaded = true;

    if (_accountPrefix.empty())
    {
        SF_LOG_ERROR("modules", "[mod-playerbots] RandomBots.AccountPrefix is empty; no candidates loaded.");
        return;
    }

    // account is stored in the auth database, characters in the character database,
    // so the lookup is done in two steps rather than a cross-database join.
    QueryResult accounts = LoginDatabase.PQuery(
        "SELECT id FROM account WHERE username LIKE '%s%%'", _accountPrefix.c_str());
    if (!accounts)
    {
        SF_LOG_INFO("modules", "[mod-playerbots] No bot accounts found for prefix '%s'.", _accountPrefix.c_str());
        return;
    }

    std::ostringstream accountIds;
    bool first = true;
    do
    {
        if (!first)
            accountIds << ',';
        accountIds << (*accounts)[0].GetUInt32();
        first = false;
    } while (accounts->NextRow());

    QueryResult characters = CharacterDatabase.PQuery(
        "SELECT guid FROM characters WHERE account IN (%s)", accountIds.str().c_str());
    if (!characters)
    {
        SF_LOG_INFO("modules", "[mod-playerbots] Bot accounts have no characters (prefix '%s').", _accountPrefix.c_str());
        return;
    }

    do
    {
        uint32 lowGuid = (*characters)[0].GetUInt32();
        _candidates.push_back(MAKE_NEW_GUID(lowGuid, 0, HIGHGUID_PLAYER));
    } while (characters->NextRow());

    SF_LOG_INFO("modules", "[mod-playerbots] Loaded %u candidate bot character(s) from prefix '%s'.",
        uint32(_candidates.size()), _accountPrefix.c_str());
}

void PlayerbotMgr::TrySpawnRandomBot()
{
    for (uint64 guid : _candidates)
    {
        if (_bots.find(guid) != _bots.end())            // already a bot
            continue;
        if (ObjectAccessor::FindPlayer(guid))           // online as a real player
            continue;

        std::string error;
        if (SpawnBot(guid, true, &error))
        {
            SF_LOG_INFO("modules", "[mod-playerbots] Random bot pool: %u/%u online.",
                uint32(_randomBots.size()), _maxRandomBots);
            return;
        }

        SF_LOG_DEBUG("modules", "[mod-playerbots] Skipping candidate GUID %u: %s", GUID_LOPART(guid), error.c_str());
    }
}

void PlayerbotMgr::CleanupDeadBots()
{
    for (auto it = _bots.begin(); it != _bots.end();)
    {
        WorldSession* session = it->second;
        Player* player = session ? session->GetPlayer() : nullptr;
        if (!player || !player->IsInWorld())
        {
            uint64 guid = it->first;
            it = _bots.erase(it);
            _randomBots.erase(guid);
            DestroyBotAI(guid);
            delete session;
        }
        else
            ++it;
    }
}

// When a real player is solo-queued in the dungeon finder, queue online bots that
// fit the same dungeon level bracket (and within LevelRange of the player).
void PlayerbotMgr::UpdateLfgAutoJoin(uint32 diff)
{
    if (!_joinLfg || _bots.empty())
        return;

    _lfgJoinTimer += diff;
    if (_lfgJoinTimer < 2000)
        return;
    _lfgJoinTimer = 0;

    struct MasterQueue
    {
        Player* player = nullptr;
        lfg::LfgDungeonSet dungeons;
    };

    std::vector<MasterQueue> masters;
    HashMapHolder<Player>::MapType const& players = ObjectAccessor::GetPlayers();
    for (auto const& pair : players)
    {
        Player* player = pair.second;
        if (!player || !player->IsInWorld() || !player->GetSession())
            continue;
        if (player->GetSession()->IsBot() || IsBot(player->GetGUID()))
            continue;
        // Party queues already use HandleLfg role/proposal responses on grouped bots.
        if (player->GetGroup())
            continue;
        if (sLFGMgr->GetState(player->GetGUID()) != lfg::LFG_STATE_QUEUED)
            continue;

        lfg::LfgDungeonSet const& selected = sLFGMgr->GetSelectedDungeons(player->GetGUID());
        if (selected.empty())
            continue;

        MasterQueue mq;
        mq.player = player;
        mq.dungeons = selected;
        masters.push_back(mq);
    }

    if (masters.empty())
        return;

    for (MasterQueue const& mq : masters)
    {
        Player* master = mq.player;
        uint8 const masterLevel = master->getLevel();
        uint32 queued = 0;

        for (auto const& pair : _bots)
        {
            if (queued >= _joinLfgMaxBots)
                break;

            WorldSession* session = pair.second;
            Player* bot = session ? session->GetPlayer() : nullptr;
            if (!bot || !bot->IsInWorld() || !bot->IsAlive())
                continue;
            if (bot->GetGroup())
                continue;
            if (bot->GetTeam() != master->GetTeam())
                continue;

            int const levelDiff = std::abs(int(bot->getLevel()) - int(masterLevel));
            if (uint32(levelDiff) > _joinLfgLevelRange)
                continue;

            lfg::LfgState const botState = sLFGMgr->GetState(bot->GetGUID());
            if (botState == lfg::LFG_STATE_QUEUED
                || botState == lfg::LFG_STATE_ROLECHECK
                || botState == lfg::LFG_STATE_PROPOSAL
                || botState == lfg::LFG_STATE_DUNGEON
                || botState == lfg::LFG_STATE_BOOT)
                continue;

            // Keep only dungeons the bot's level is eligible for.
            lfg::LfgDungeonSet botDungeons;
            for (uint32 dungeonRef : mq.dungeons)
            {
                uint32 const dungeonId = dungeonRef & 0x00FFFFFF;
                LFGDungeonEntry const* entry = sLFGDungeonStore.LookupEntry(dungeonId);
                if (!entry)
                    continue;
                if (bot->getLevel() < entry->m_MinLevel || bot->getLevel() > entry->m_MaxLevel)
                    continue;
                botDungeons.insert(dungeonId);
            }

            if (botDungeons.empty())
                continue;

            uint8 roles = lfg::PLAYER_ROLE_DAMAGE;
            auto aiIt = _ai.find(bot->GetGUID());
            if (aiIt != _ai.end() && aiIt->second)
                roles = aiIt->second->ComputeLfgRole();

            sLFGMgr->JoinLfg(bot, roles, botDungeons, "");
            ++queued;
        }

        if (queued)
            SF_LOG_DEBUG("modules", "[mod-playerbots] LFG auto-join: queued %u bot(s) for %s (level %u).",
                queued, master->GetName().c_str(), masterLevel);
    }
}

bool PlayerbotMgr::AddBot(uint64 characterGuid, std::string* errorOut)
{
    return SpawnBot(characterGuid, false, errorOut);
}

bool PlayerbotMgr::SpawnBot(uint64 characterGuid, bool isRandom, std::string* errorOut)
{
    auto fail = [errorOut](char const* reason) -> bool
    {
        if (errorOut)
            *errorOut = reason;
        return false;
    };

    if (!_enabled)
        return fail("Playerbots module is disabled (set Playerbots.Enable = 1).");

    if (!characterGuid)
        return fail("Invalid character.");

    if (IsBot(characterGuid))
        return fail("That character is already an active bot.");

    // Refuse if the character is already in the world (real player or otherwise);
    // a second Player with the same GUID would collide in the object accessor.
    if (ObjectAccessor::FindPlayer(characterGuid))
        return fail("That character is already online.");

    uint32 accountId = sObjectMgr->GetPlayerAccountIdByGUID(characterGuid);
    if (!accountId)
        return fail("Could not resolve the character's account.");

    uint8 expansion = uint8(sWorld->getIntConfig(WorldIntConfigs::CONFIG_EXPANSION));

    // Bots always run at player security regardless of the owning account's level.
    WorldSession* botSession = new WorldSession(accountId, nullptr, AccountTypes::SEC_PLAYER, expansion,
        0, LOCALE_enUS, 0, false, false);
    botSession->SetBot(true);
    // Match the realm the real characters use so name queries and /who resolve
    // the bot correctly (a socketless session otherwise reports realm 0).
    botSession->SetVirtualRealmID(realmID);

    if (!botSession->LoginBotCharacter(characterGuid))
    {
        delete botSession;
        return fail("Failed to load the character into the world.");
    }

    _bots[characterGuid] = botSession;
    if (isRandom)
        _randomBots.insert(characterGuid);

    if (Player* bot = botSession->GetPlayer())
    {
        _ai[characterGuid] = new PlayerbotAI(bot);
        char const* raceName = GetRaceName(bot->getRace(), LOCALE_enUS);
        char const* className = GetClassName(bot->getClass(), LOCALE_enUS);
        SF_LOG_INFO("modules",
            "[mod-playerbots] Bot '%s' entered the world (GUID %u, level %u %s %s%s).",
            bot->GetName().c_str(), GUID_LOPART(characterGuid), bot->getLevel(),
            raceName ? raceName : "?", className ? className : "?",
            isRandom ? ", random" : "");
    }

    return true;
}

void PlayerbotMgr::UpdateBotAI(Player* bot, uint32 diff)
{
    if (!bot)
        return;

    auto it = _ai.find(bot->GetGUID());
    if (it != _ai.end())
        it->second->UpdateAI(diff);
}

void PlayerbotMgr::HandleBotWhisper(Player* from, Player* bot, std::string const& msg)
{
    if (!_enabled || !from || !bot)
        return;
    if (from->GetSession() && from->GetSession()->IsBot())
        return;

    auto it = _ai.find(bot->GetGUID());
    if (it == _ai.end() || !it->second)
        return;

    it->second->HandleChatCommand(from, msg);
}

void PlayerbotMgr::HandleBotGroupChat(Player* from, Group* group, std::string const& msg)
{
    if (!_enabled || !from || !group)
        return;
    if (from->GetSession() && from->GetSession()->IsBot())
        return;

    // Optional "@tank "/ "@dps "/ "@heal "/ "@ranged " prefix filters who hears the order.
    std::string text = msg;
    while (!text.empty() && text[0] == ' ')
        text.erase(text.begin());

    std::string filter;
    if (!text.empty() && text[0] == '@')
    {
        std::string::size_type space = text.find(' ');
        if (space != std::string::npos)
        {
            filter = text.substr(1, space - 1);
            text = text.substr(space + 1);
            while (!text.empty() && text[0] == ' ')
                text.erase(text.begin());
            std::transform(filter.begin(), filter.end(), filter.begin(),
                [](unsigned char c) { return char(std::tolower(c)); });
            if (filter == "healer")
                filter = "heal";
        }
    }

    if (text.empty())
        return;

    // Strategy queries/changes always want a whisper reply from each bot.
    bool const strategyCmd = (text.size() >= 2
        && (text.compare(0, 2, "co") == 0 || text.compare(0, 2, "nc") == 0)
        && (text.size() == 2 || text[2] == ' ' || text[2] == '?'));

    // "@tank attack" strips to filter=tank + text=attack. Map that to tank-only
    // engage and make everyone else hold assist so melee DPS don't pile in.
    bool const tankOnlyPull = (filter == "tank" && (text == "attack" || text == "tank attack"))
        || (filter.empty() && text == "tank attack");
    bool const dpsOnlyPull = (filter == "dps" && (text == "attack" || text == "dps attack"))
        || (filter.empty() && text == "dps attack");

    auto deliver = [&](Player* member)
    {
        if (!member)
            return;
        auto it = _ai.find(member->GetGUID());
        if (it == _ai.end() || !it->second)
            return;

        if (tankOnlyPull)
        {
            if (it->second->MatchesRoleFilter("tank"))
                it->second->HandleChatCommand(from, "tank attack", false);
            else
                it->second->SetHoldAssist(true);
            return;
        }
        if (dpsOnlyPull)
        {
            if (it->second->MatchesRoleFilter("dps"))
                it->second->HandleChatCommand(from, "dps attack", false);
            return;
        }

        if (!filter.empty() && !it->second->MatchesRoleFilter(filter))
            return;
        // Party orders are silent except co/nc (bots whisper their strategy state).
        it->second->HandleChatCommand(from, text, strategyCmd);
    };

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        // Socket bots and self-bots (including the issuer for co/nc on yourself).
        if (!member || !HasBotAI(member->GetGUID()))
            continue;
        deliver(member);
    }
}

bool PlayerbotMgr::AttachSelfBot(Player* player, std::string* errorOut)
{
    auto fail = [&](char const* msg) -> bool
    {
        if (errorOut)
            *errorOut = msg;
        return false;
    };

    if (!_enabled)
        return fail("Playerbots module is disabled.");
    if (!player || !player->IsInWorld())
        return fail("You must be in the world.");
    if (IsBot(player->GetGUID()))
        return fail("This character is already a full bot session.");
    if (IsSelfBot(player->GetGUID()))
        return fail("Self-bot AI is already attached.");

    uint64 guid = player->GetGUID();
    DestroyBotAI(guid); // safety if a stale AI pointer lingered
    _ai[guid] = new PlayerbotAI(player, true);
    _selfBots.insert(guid);
    return true;
}

bool PlayerbotMgr::DetachSelfBot(Player* player)
{
    if (!player)
        return false;

    uint64 guid = player->GetGUID();
    if (!IsSelfBot(guid))
        return false;

    _selfBots.erase(guid);
    DestroyBotAI(guid);
    return true;
}

bool PlayerbotMgr::ToggleSelfBot(Player* player, std::string* report)
{
    if (!player)
        return false;

    if (IsSelfBot(player->GetGUID()))
    {
        DetachSelfBot(player);
        if (report)
            *report = "Self-bot AI detached. You control combat again.";
        return true;
    }

    std::string error;
    if (!AttachSelfBot(player, &error))
    {
        if (report)
            *report = error;
        return false;
    }

    if (report)
        *report = "Self-bot AI attached. You move; the AI casts in combat. Use .playerbots self again to detach.";
    return true;
}

bool PlayerbotMgr::RemoveBot(uint64 characterGuid)
{
    auto it = _bots.find(characterGuid);
    if (it == _bots.end())
        return false;

    WorldSession* botSession = it->second;
    _bots.erase(it);
    _randomBots.erase(characterGuid);
    DestroyBotAI(characterGuid);

    // ~WorldSession logs the player out (save) if still attached and cleans up.
    delete botSession;
    return true;
}

void PlayerbotMgr::LogoutAllBots()
{
    for (auto& pair : _ai)
        delete pair.second;
    _ai.clear();

    for (auto& pair : _bots)
        delete pair.second;

    _bots.clear();
    _randomBots.clear();
    _selfBots.clear();
}

void PlayerbotMgr::DestroyBotAI(uint64 characterGuid)
{
    auto it = _ai.find(characterGuid);
    if (it == _ai.end())
        return;

    delete it->second;
    _ai.erase(it);
}

void PlayerbotMgr::GetBotGuids(std::vector<uint64>& out) const
{
    out.reserve(_bots.size());
    for (auto const& pair : _bots)
        out.push_back(pair.first);
}

// ---------------------------------------------------------------------------
// Auto-creation
// ---------------------------------------------------------------------------

namespace
{
    enum BotRole { BOT_ROLE_TANK, BOT_ROLE_HEALER, BOT_ROLE_DPS };

    char const* RoleName(BotRole role)
    {
        switch (role)
        {
            case BOT_ROLE_TANK:   return "tank";
            case BOT_ROLE_HEALER: return "healer";
            default:              return "dps";
        }
    }

    char const* SpecName(uint32 specId)
    {
        switch (specId)
        {
            case SPEC_MAGE_ARCANE:            return "Arcane";
            case SPEC_MAGE_FIRE:              return "Fire";
            case SPEC_MAGE_FROST:             return "Frost";
            case SPEC_PALADIN_HOLY:           return "Holy";
            case SPEC_PALADIN_PROTECTION:     return "Protection";
            case SPEC_PALADIN_RETRIBUTION:    return "Retribution";
            case SPEC_WARRIOR_ARMS:           return "Arms";
            case SPEC_WARRIOR_FURY:           return "Fury";
            case SPEC_WARRIOR_PROTECTION:     return "Protection";
            case SPEC_DRUID_BALANCE:          return "Balance";
            case SPEC_DRUID_FERAL:            return "Feral";
            case SPEC_DRUID_GUARDIAN:         return "Guardian";
            case SPEC_DRUID_RESTORATION:      return "Restoration";
            case SPEC_DEATH_KNIGHT_BLOOD:     return "Blood";
            case SPEC_DEATH_KNIGHT_FROST:     return "Frost";
            case SPEC_DEATH_KNIGHT_UNHOLY:    return "Unholy";
            case SPEC_HUNTER_BEAST_MASTERY:   return "Beast Mastery";
            case SPEC_HUNTER_MARKSMANSHIP:    return "Marksmanship";
            case SPEC_HUNTER_SURVIVAL:        return "Survival";
            case SPEC_PRIEST_DISCIPLINE:      return "Discipline";
            case SPEC_PRIEST_HOLY:            return "Holy";
            case SPEC_PRIEST_SHADOW:          return "Shadow";
            case SPEC_ROGUE_ASSASSINATION:    return "Assassination";
            case SPEC_ROGUE_COMBAT:           return "Combat";
            case SPEC_ROGUE_SUBTLETY:         return "Subtlety";
            case SPEC_SHAMAN_ELEMENTAL:       return "Elemental";
            case SPEC_SHAMAN_ENHANCEMENT:     return "Enhancement";
            case SPEC_SHAMAN_RESTORATION:     return "Restoration";
            case SPEC_WARLOCK_AFFLICTION:     return "Affliction";
            case SPEC_WARLOCK_DEMONOLOGY:     return "Demonology";
            case SPEC_WARLOCK_DESTRUCTION:    return "Destruction";
            case SPEC_MONK_BREWMASTER:        return "Brewmaster";
            case SPEC_MONK_WINDWALKER:        return "Windwalker";
            case SPEC_MONK_MISTWEAVER:        return "Mistweaver";
            default:                         return "None";
        }
    }

    char const* QualityName(int quality)
    {
        switch (quality)
        {
            case ITEM_QUALITY_POOR:      return "poor";
            case ITEM_QUALITY_NORMAL:    return "common";
            case ITEM_QUALITY_UNCOMMON:  return "uncommon";
            case ITEM_QUALITY_RARE:      return "rare";
            case ITEM_QUALITY_EPIC:      return "epic";
            case ITEM_QUALITY_LEGENDARY: return "legendary";
            default:                     return "unknown";
        }
    }

    char const* SafeRaceName(uint8 race)
    {
        char const* name = GetRaceName(race, LOCALE_enUS);
        return name ? name : "?";
    }

    char const* SafeClassName(uint8 cls)
    {
        char const* name = GetClassName(cls, LOCALE_enUS);
        return name ? name : "?";
    }

    // Playable races per faction. Neutral Pandaren are excluded so offline
    // creation never has to resolve the neutral starting faction.
    uint8 const AllianceRaces[] = { RACE_HUMAN, RACE_DWARF, RACE_NIGHTELF, RACE_GNOME, RACE_DRAENEI, RACE_WORGEN };
    uint8 const HordeRaces[]    = { RACE_ORC, RACE_UNDEAD_PLAYER, RACE_TAUREN, RACE_TROLL, RACE_BLOODELF, RACE_GOBLIN };

    // Classes able to fill each role (Death Knight excluded: it has a special
    // starting experience and level requirement that offline creation skips).
    uint8 const TankClasses[]   = { CLASS_WARRIOR, CLASS_PALADIN, CLASS_DRUID, CLASS_MONK };
    uint8 const HealerClasses[] = { CLASS_PALADIN, CLASS_PRIEST, CLASS_SHAMAN, CLASS_DRUID, CLASS_MONK };
    uint8 const DpsClasses[]    = { CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE, CLASS_PRIEST,
                                    CLASS_SHAMAN, CLASS_MAGE, CLASS_WARLOCK, CLASS_MONK, CLASS_DRUID };

    uint32 RollPct() { return uint32(std::rand() % 100); }   // 0..99

    template <size_t N>
    uint8 PickRandom(uint8 const (&arr)[N]) { return arr[std::rand() % N]; }

    uint8 PickClassForRole(BotRole role)
    {
        switch (role)
        {
            case BOT_ROLE_TANK:   return PickRandom(TankClasses);
            case BOT_ROLE_HEALER: return PickRandom(HealerClasses);
            default:              return PickRandom(DpsClasses);
        }
    }

    // Specialization tab index (matches the in-game spec order) for a class in a
    // given role. Classes that cannot fill the role fall back to a damage spec.
    uint8 SpecTabForRole(uint8 cls, BotRole role)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:      return role == BOT_ROLE_TANK ? 2 : 0;                       // Prot / Arms
            case CLASS_PALADIN:      return role == BOT_ROLE_TANK ? 1 : (role == BOT_ROLE_HEALER ? 0 : 2); // Prot / Holy / Ret
            case CLASS_DEATH_KNIGHT: return role == BOT_ROLE_TANK ? 0 : 1;                       // Blood / Frost
            case CLASS_PRIEST:       return role == BOT_ROLE_HEALER ? 0 : 2;                     // Disc / Shadow
            case CLASS_SHAMAN:       return role == BOT_ROLE_HEALER ? 2 : 0;                     // Resto / Elemental
            case CLASS_MONK:         return role == BOT_ROLE_TANK ? 0 : (role == BOT_ROLE_HEALER ? 1 : 2); // Brewmaster / Mistweaver / Windwalker
            case CLASS_DRUID:        return role == BOT_ROLE_TANK ? 2 : (role == BOT_ROLE_HEALER ? 3 : 0); // Guardian / Resto / Balance
            case CLASS_HUNTER:       return 0;                                                   // Beast Mastery
            case CLASS_ROGUE:        return 0;                                                   // Assassination
            case CLASS_MAGE:         return 2;                                                   // Frost
            case CLASS_WARLOCK:      return 0;                                                   // Affliction
            default:                 return 0;
        }
    }

    // ChrSpecialization id for a class/role pair (0 if unavailable).
    uint32 SpecIdForRole(uint8 cls, BotRole role)
    {
        uint32 const* specs = GetClassSpecializations(cls);
        if (!specs)
            return 0;
        return specs[SpecTabForRole(cls, role)];
    }

    // Returns a race that forms a valid race/class pair for the faction, or 0.
    uint8 PickRaceForClassFaction(uint8 cls, bool alliance)
    {
        std::vector<uint8> races(alliance ? std::begin(AllianceRaces) : std::begin(HordeRaces),
                                 alliance ? std::end(AllianceRaces) : std::end(HordeRaces));

        // Fisher-Yates shuffle so the chosen race is not biased by list order.
        for (size_t i = races.size(); i > 1; --i)
            std::swap(races[i - 1], races[std::rand() % i]);

        for (uint8 r : races)
            if (sObjectMgr->GetPlayerInfo(r, cls))
                return r;
        return 0;
    }

    std::string GenerateBotName()
    {
        static char const consonants[] = "bcdfghjklmnprstvw";
        static char const vowels[]     = "aeiou";

        uint32 len = 4 + (std::rand() % 5);                  // 4..8 characters
        std::string name;
        bool cons = true;
        for (uint32 i = 0; i < len; ++i)
        {
            if (cons)
                name += consonants[std::rand() % (sizeof(consonants) - 1)];
            else
                name += vowels[std::rand() % (sizeof(vowels) - 1)];
            cons = !cons;
        }
        name[0] = char(std::toupper(static_cast<unsigned char>(name[0])));
        return name;
    }

    // -----------------------------------------------------------------------
    // Gear selection
    // -----------------------------------------------------------------------

    // Heaviest armor type the class can wear. Armor slots query the whole range
    // 1..armorType and prefer the heaviest available, so low-level plate/mail
    // classes automatically fall back to lighter armor until upgrades exist.
    uint8 ArmorTypeForClass(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT: return ITEM_SUBCLASS_ARMOR_PLATE;
            case CLASS_HUNTER:
            case CLASS_SHAMAN:       return ITEM_SUBCLASS_ARMOR_MAIL;
            case CLASS_ROGUE:
            case CLASS_DRUID:
            case CLASS_MONK:         return ITEM_SUBCLASS_ARMOR_LEATHER;
            default:                 return ITEM_SUBCLASS_ARMOR_CLOTH; // priest/mage/warlock
        }
    }

    // Primary stat the spec wants, used to bias item picks toward useful gear.
    uint32 PrimaryStatForClassRole(uint8 cls, BotRole role, uint32 specId = 0)
    {
        // Spec-aware overrides for hybrids with multiple DPS trees.
        switch (specId)
        {
            case SPEC_SHAMAN_ENHANCEMENT:
            case SPEC_DRUID_FERAL:
            case SPEC_DRUID_GUARDIAN:
            case SPEC_HUNTER_BEAST_MASTERY:
            case SPEC_HUNTER_MARKSMANSHIP:
            case SPEC_HUNTER_SURVIVAL:
            case SPEC_ROGUE_ASSASSINATION:
            case SPEC_ROGUE_COMBAT:
            case SPEC_ROGUE_SUBTLETY:
            case SPEC_MONK_WINDWALKER:
            case SPEC_MONK_BREWMASTER:
                return ITEM_MOD_AGILITY;
            case SPEC_SHAMAN_ELEMENTAL:
            case SPEC_SHAMAN_RESTORATION:
            case SPEC_DRUID_BALANCE:
            case SPEC_DRUID_RESTORATION:
            case SPEC_MONK_MISTWEAVER:
                return ITEM_MOD_INTELLECT;
            default:
                break;
        }

        switch (cls)
        {
            case CLASS_WARRIOR:
            case CLASS_DEATH_KNIGHT: return ITEM_MOD_STRENGTH;
            case CLASS_PALADIN:      return role == BOT_ROLE_HEALER ? ITEM_MOD_INTELLECT : ITEM_MOD_STRENGTH;
            case CLASS_HUNTER:
            case CLASS_ROGUE:        return ITEM_MOD_AGILITY;
            case CLASS_MONK:         return role == BOT_ROLE_HEALER ? ITEM_MOD_INTELLECT : ITEM_MOD_AGILITY;
            case CLASS_DRUID:        return role == BOT_ROLE_TANK ? ITEM_MOD_AGILITY
                                         : (role == BOT_ROLE_DPS ? ITEM_MOD_INTELLECT : ITEM_MOD_INTELLECT);
            case CLASS_SHAMAN:       return role == BOT_ROLE_DPS ? ITEM_MOD_INTELLECT : ITEM_MOD_INTELLECT;
            default:                 return ITEM_MOD_INTELLECT; // priest/mage/warlock
        }
    }

    // Maps a specialization back to a coarse role, but only for classes that can
    // actually fill the tank/healer role; everything else counts as damage.
    BotRole RoleFromSpec(uint8 cls, uint32 specId)
    {
        if (specId)
        {
            for (uint8 c : TankClasses)
                if (c == cls && specId == SpecIdForRole(cls, BOT_ROLE_TANK))
                    return BOT_ROLE_TANK;
            for (uint8 c : HealerClasses)
                if (c == cls && specId == SpecIdForRole(cls, BOT_ROLE_HEALER))
                    return BOT_ROLE_HEALER;
        }
        return BOT_ROLE_DPS;
    }

    // Returns candidate item entries (highest item level first) matching the
    // WHERE clause for this bot. When primaryStat is set the search is
    // restricted to items carrying that stat. More than one row is returned so
    // callers can skip items the bot cannot actually equip yet (e.g. a low-level
    // plate class that only has armor proficiency for mail/leather).
    //
    // Filter by RequiredLevel near the bot's level (not ItemLevel <= level+25):
    // MoP ilvl is hundreds at 90, so an ItemLevel cap wrongly forces ~TBC gear.
    // maxQuality caps ITEM_QUALITY_* (default epic) so testers can request rare/etc.
    std::vector<uint32> QueryItemEntries(Player* bot, std::string const& where, uint32 primaryStat,
        uint32 exclude, int maxQuality)
    {
        uint32 const level    = bot->getLevel();
        uint32 const classBit = 1u << (bot->getClass() - 1);
        uint32 const raceBit  = 1u << (bot->getRace() - 1);
        uint32 const minReq   = level > 10 ? (level - 10) : 1;
        int const qualityCap  = std::max(0, std::min(maxQuality, int(ITEM_QUALITY_LEGENDARY)));
        int const qualityMin  = qualityCap >= int(ITEM_QUALITY_UNCOMMON) ? int(ITEM_QUALITY_UNCOMMON) : qualityCap;

        std::ostringstream q;
        q << "SELECT entry FROM item_template WHERE " << where
          << " AND RequiredLevel <= " << level
          << " AND RequiredLevel >= " << minReq
          << " AND Quality BETWEEN " << qualityMin << " AND " << qualityCap
          << " AND duration = 0 AND startquest = 0"
          << " AND (AllowableClass = -1 OR (AllowableClass & " << classBit << "))"
          << " AND (AllowableRace = -1 OR (AllowableRace & " << raceBit << "))";

        if (exclude)
            q << " AND entry <> " << exclude;

        if (primaryStat)
            q << " AND (stat_type1=" << primaryStat << " OR stat_type2=" << primaryStat
              << " OR stat_type3=" << primaryStat << " OR stat_type4=" << primaryStat
              << " OR stat_type5=" << primaryStat << " OR stat_type6=" << primaryStat
              << " OR stat_type7=" << primaryStat << " OR stat_type8=" << primaryStat
              << " OR stat_type9=" << primaryStat << " OR stat_type10=" << primaryStat << ")";

        q << " ORDER BY ItemLevel DESC, RequiredLevel DESC LIMIT 25";

        std::vector<uint32> entries;
        if (QueryResult result = WorldDatabase.Query(q.str().c_str()))
        {
            do
            {
                entries.push_back(result->Fetch()[0].GetUInt32());
            } while (result->NextRow());
        }
        return entries;
    }

    // Teach every class-trainer spell the bot qualifies for at its current level
    // (multi-pass so spell-chain prerequisites can unlock in order).
    void LearnClassTrainerSpells(Player* bot)
    {
        if (!bot)
            return;

        uint8 const cls = bot->getClass();
        CreatureTemplateContainer const* templates = sObjectMgr->GetCreatureTemplates();
        if (!templates)
            return;

        std::unordered_set<uint32> seen;
        std::vector<TrainerSpell const*> trainerSpells;

        for (CreatureTemplateContainer::const_iterator itr = templates->begin();
            itr != templates->end(); ++itr)
        {
            CreatureTemplate const& creature = itr->second;
            if (creature.trainer_type != TRAINER_TYPE_CLASS)
                continue;
            if (creature.trainer_class != cls)
                continue;
            if (!(creature.npcflag & UNIT_NPC_FLAG_TRAINER))
                continue;

            TrainerSpellData const* data = sObjectMgr->GetNpcTrainerSpells(creature.Entry);
            if (!data)
                continue;

            for (TrainerSpellMap::const_iterator sp = data->spellList.begin();
                sp != data->spellList.end(); ++sp)
            {
                if (!seen.insert(sp->first).second)
                    continue;
                trainerSpells.push_back(&sp->second);
            }
        }

        for (int pass = 0; pass < 12; ++pass)
        {
            bool learnedAny = false;
            for (TrainerSpell const* ts : trainerSpells)
            {
                if (!ts)
                    continue;
                // GREEN = can learn now. Skip GRAY (known) and RED (reqs).
                // GREEN_DISABLED is primary-profession capped - skip those.
                if (bot->GetTrainerSpellState(ts) != TRAINER_SPELL_GREEN)
                    continue;

                if (ts->IsCastable())
                    bot->CastSpell(bot, ts->spell, true);
                else
                    bot->learnSpell(ts->spell, false);
                learnedAny = true;
            }
            if (!learnedAny)
                break;
        }
    }

    // Catch SkillLineAbility / specialization spells GiveLevel would grant.
    void LearnLevelClassSpells(Player* bot, uint32 specId)
    {
        if (!bot)
            return;

        std::list<uint32> const spells = GetSpellsForLevels(
            bot->getClass(), bot->getRaceMask(), specId, 0, bot->getLevel());
        for (uint32 spellId : spells)
            if (spellId && !bot->HasSpell(spellId))
                bot->learnSpell(spellId, true);
    }

    uint32 PickRandomSpell(uint32 const* list, size_t count)
    {
        if (!list || !count)
            return 0;
        return list[static_cast<size_t>(std::rand()) % count];
    }

    void LearnMountIfMissing(Player* bot, uint32 const* primary, size_t primaryCount,
        uint32 const* fallback, size_t fallbackCount)
    {
        if (!bot)
            return;

        auto hasAny = [&](uint32 const* list, size_t count) -> bool
        {
            for (size_t i = 0; i < count; ++i)
                if (list[i] && bot->HasSpell(list[i]))
                    return true;
            return false;
        };

        if (hasAny(primary, primaryCount) || hasAny(fallback, fallbackCount))
            return;

        auto tryLearn = [&](uint32 const* list, size_t count) -> bool
        {
            if (!list || !count)
                return false;
            for (int attempt = 0; attempt < 8; ++attempt)
            {
                uint32 const mount = PickRandomSpell(list, count);
                if (mount && bot->IsSpellFitByClassAndRace(mount))
                {
                    bot->learnSpell(mount, false);
                    return true;
                }
            }
            for (size_t i = 0; i < count; ++i)
            {
                if (list[i] && bot->IsSpellFitByClassAndRace(list[i]))
                {
                    bot->learnSpell(list[i], false);
                    return true;
                }
            }
            return false;
        };

        if (!tryLearn(primary, primaryCount))
            tryLearn(fallback, fallbackCount);
    }

    // Riding skills + one random mount per unlocked tier (level-gated).
    // Tiers: normal ground (20), swift ground (40), normal flying (60), epic flying (70).
    void LearnRidingAndMounts(Player* bot)
    {
        if (!bot)
            return;

        uint8 const level = bot->getLevel();
        auto learnRiding = [&](uint32 spellId)
        {
            if (spellId && !bot->HasSpell(spellId))
                bot->learnSpell(spellId, false);
        };

        // Faction mount pools (spell IDs taught by the mount items).
        static uint32 const allianceGround[] = { 458, 470, 472, 6648, 6777, 6898, 6899 };
        static uint32 const hordeGround[]    = { 580, 6653, 6654, 8395, 10796, 17462, 18989, 34795 };
        static uint32 const neutralGround[]  = { 43899, 49379 };

        static uint32 const allianceSwiftGround[] = { 23227, 23228, 23229, 23238, 23239, 23240 };
        static uint32 const hordeSwiftGround[]    = { 23250, 23251, 23252, 23241, 23242, 23243, 35025, 35027 };
        static uint32 const neutralSwiftGround[]  = { 42777 };

        static uint32 const allianceFlying[] = { 32235, 32239, 32240 };
        static uint32 const hordeFlying[]    = { 32243, 32244, 32245 };
        static uint32 const neutralFlying[]  = { 59569, 59570 };

        static uint32 const allianceEpicFlying[] = { 32242, 32289, 32290, 32292 };
        static uint32 const hordeEpicFlying[]    = { 32246, 32295, 32296, 32297 };
        static uint32 const neutralEpicFlying[]  = { 60025 };

        bool const alliance = bot->GetTeam() == ALLIANCE;

        if (level >= 20)
        {
            learnRiding(33388); // Apprentice Riding
            if (alliance)
                LearnMountIfMissing(bot, allianceGround, sizeof(allianceGround) / sizeof(allianceGround[0]),
                    neutralGround, sizeof(neutralGround) / sizeof(neutralGround[0]));
            else
                LearnMountIfMissing(bot, hordeGround, sizeof(hordeGround) / sizeof(hordeGround[0]),
                    neutralGround, sizeof(neutralGround) / sizeof(neutralGround[0]));
        }

        if (level >= 40)
        {
            learnRiding(33391); // Journeyman Riding
            if (alliance)
                LearnMountIfMissing(bot, allianceSwiftGround, sizeof(allianceSwiftGround) / sizeof(allianceSwiftGround[0]),
                    neutralSwiftGround, sizeof(neutralSwiftGround) / sizeof(neutralSwiftGround[0]));
            else
                LearnMountIfMissing(bot, hordeSwiftGround, sizeof(hordeSwiftGround) / sizeof(hordeSwiftGround[0]),
                    neutralSwiftGround, sizeof(neutralSwiftGround) / sizeof(neutralSwiftGround[0]));
        }

        if (level >= 60)
        {
            learnRiding(34090); // Expert Riding
            learnRiding(90267); // Flight Master's License
            if (alliance)
                LearnMountIfMissing(bot, allianceFlying, sizeof(allianceFlying) / sizeof(allianceFlying[0]),
                    neutralFlying, sizeof(neutralFlying) / sizeof(neutralFlying[0]));
            else
                LearnMountIfMissing(bot, hordeFlying, sizeof(hordeFlying) / sizeof(hordeFlying[0]),
                    neutralFlying, sizeof(neutralFlying) / sizeof(neutralFlying[0]));
        }

        if (level >= 68)
            learnRiding(54197); // Cold Weather Flying

        if (level >= 70)
        {
            learnRiding(34091); // Artisan Riding
            if (alliance)
                LearnMountIfMissing(bot, allianceEpicFlying, sizeof(allianceEpicFlying) / sizeof(allianceEpicFlying[0]),
                    neutralEpicFlying, sizeof(neutralEpicFlying) / sizeof(neutralEpicFlying[0]));
            else
                LearnMountIfMissing(bot, hordeEpicFlying, sizeof(hordeEpicFlying) / sizeof(hordeEpicFlying[0]),
                    neutralEpicFlying, sizeof(neutralEpicFlying) / sizeof(neutralEpicFlying[0]));
        }

        if (level >= 80)
            learnRiding(90265); // Master Riding
    }

    // Learn cloth/leather/mail/plate proficiency spells the class is allowed to
    // have at its current level (mirrors trainer unlocks / GiveLevel plate).
    // Called on every init so deleveling then re-init does not teach plate early,
    // while a level-90 warrior that never visited a trainer still learns it.
    void LearnArmorProficiencies(Player* bot)
    {
        if (!bot)
            return;

        uint8 const level = bot->getLevel();
        uint8 const cls = bot->getClass();

        auto learn = [&](uint32 spellId)
        {
            if (spellId && !bot->HasSpell(spellId))
                bot->learnSpell(spellId, true);
        };

        // Cloth (9078) is baseline for cloth wearers; leather classes also use it
        // under the armor. Learning again is a no-op when already known.
        switch (cls)
        {
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                learn(9078); // Cloth
                break;
            case CLASS_ROGUE:
            case CLASS_DRUID:
            case CLASS_MONK:
                learn(9078);
                learn(9077); // Leather
                break;
            case CLASS_HUNTER:
            case CLASS_SHAMAN:
                learn(9078);
                learn(9077);
                if (level >= 40)
                    learn(8737); // Mail
                break;
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
                learn(9078);
                learn(9077);
                learn(119811); // Mail (MoP)
                if (level >= 40)
                    learn(750); // Plate Mail
                break;
            case CLASS_DEATH_KNIGHT:
                // DKs begin at 55 with plate available.
                learn(9078);
                learn(9077);
                learn(119811);
                learn(750);
                break;
            default:
                break;
        }

        // Keep weapon skills usable for the equipped level band.
        bot->UpdateSkillsToMaxSkillsForLevel();
    }

    // Equips a fresh copy of the item into whichever slot the core picks.
    bool EquipItemEntry(Player* bot, uint32 entry)
    {
        if (!entry)
            return false;

        uint16 dest = 0;
        if (bot->CanEquipNewItem(NULL_SLOT, dest, entry, false) != EQUIP_ERR_OK)
            return false;

        return bot->EquipNewItem(dest, entry, true) != nullptr;
    }

    // Equips the best item matching the WHERE clause that the bot can actually
    // use, walking the candidate list so a slot is only left empty when nothing
    // is equippable. Prefers items with the spec's primary stat, then any. The
    // exclude entry keeps paired slots (rings/trinkets) distinct. Returns the
    // equipped entry, or 0.
    uint32 EquipBestForSlot(Player* bot, std::string const& where, uint32 primaryStat, uint32 exclude,
        int maxQuality)
    {
        uint32 const passStats[2] = { primaryStat, 0 };
        int const passes = primaryStat ? 2 : 1;

        for (int p = 0; p < passes; ++p)
        {
            std::vector<uint32> const entries = QueryItemEntries(bot, where, passStats[p], exclude, maxQuality);
            for (uint32 entry : entries)
                if (EquipItemEntry(bot, entry))
                    return entry;
        }
        return 0;
    }

    // Removes everything currently equipped except the cosmetic shirt/tabard so
    // a re-gear starts from a clean set of slots.
    void ClearEquipment(Player* bot)
    {
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            if (slot == EQUIPMENT_SLOT_BODY || slot == EQUIPMENT_SLOT_TABARD)
                continue;
            if (bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                bot->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
        }
    }

    // Picks class/role-appropriate weapons (and shield/offhand/ranged).
    void GearWeapons(Player* bot, BotRole role, uint32 specId, int maxQuality)
    {
        uint8 const cls = bot->getClass();
        uint32 const primary = PrimaryStatForClassRole(cls, role, specId);

        auto equip = [&](std::string const& where) -> bool
        {
            return EquipBestForSlot(bot, where, primary, 0, maxQuality) != 0;
        };

        std::string const oneH   = "class=2 AND InventoryType IN (13,21)";
        std::string const twoH   = "class=2 AND InventoryType=17";
        std::string const shield = "class=4 AND subclass=6 AND InventoryType=14";
        std::string const held   = "class=4 AND InventoryType=23";
        std::string const ranged = "class=2 AND InventoryType IN (15,26) AND subclass IN (2,3,18)";

        switch (cls)
        {
            case CLASS_HUNTER:
                equip(ranged);
                break;
            case CLASS_WARRIOR:
                if (role == BOT_ROLE_TANK)
                {
                    if (!equip(oneH + " AND subclass IN (0,4,7)"))
                        equip(twoH + " AND subclass IN (1,5,6,8)");
                    equip(shield);
                }
                else if (!equip(twoH + " AND subclass IN (1,5,6,8)"))
                {
                    equip(oneH + " AND subclass IN (0,4,7)");
                    equip(shield);
                }
                break;
            case CLASS_PALADIN:
                if (role == BOT_ROLE_DPS)
                {
                    if (!equip(twoH + " AND subclass IN (1,5,6,8)"))
                    {
                        equip(oneH + " AND subclass IN (0,4,7)");
                        equip(shield);
                    }
                }
                else
                {
                    equip(oneH + " AND subclass IN (0,4,7)");
                    equip(shield);
                }
                break;
            case CLASS_DEATH_KNIGHT:
                if (!equip(twoH + " AND subclass IN (1,5,6,8)"))
                    equip(oneH + " AND subclass IN (0,4,7)");
                break;
            case CLASS_ROGUE:
                // Assassination (Mutilate) requires Dual Wield + daggers in both hands.
                if (specId == SPEC_ROGUE_ASSASSINATION)
                {
                    if (!bot->HasSpell(674))
                        bot->learnSpell(674, true); // Dual Wield
                    bot->SetCanDualWield(true);

                    std::string const dagger =
                        "class=2 AND subclass=15 AND InventoryType IN (13,21,22)";
                    uint32 const mh = EquipBestForSlot(bot, dagger, primary, 0, maxQuality);
                    uint32 oh = 0;
                    if (mh)
                        oh = EquipBestForSlot(bot, dagger, primary, mh, maxQuality);

                    // Last resort: any 1H if the dagger pool is empty, but still
                    // force Dual Wield so an off-hand slot can fill.
                    if (!mh)
                        EquipBestForSlot(bot, oneH + " AND subclass IN (0,4,7,13,15)", primary, 0, maxQuality);
                    if (!oh)
                        EquipBestForSlot(bot, oneH + " AND subclass IN (0,4,7,13,15)", primary,
                            bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND)
                                ? bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND)->GetEntry()
                                : 0,
                            maxQuality);
                }
                else
                {
                    if (!bot->HasSpell(674))
                        bot->learnSpell(674, true);
                    bot->SetCanDualWield(true);
                    equip(oneH + " AND subclass IN (0,4,7,13,15)");
                    equip(oneH + " AND subclass IN (0,4,7,13,15)"); // off-hand
                }
                break;
            case CLASS_SHAMAN:
                if (specId == SPEC_SHAMAN_ENHANCEMENT)
                {
                    // Enhancement: dual-wield 1H.
                    equip(oneH + " AND subclass IN (0,4,13,15)");
                    equip(oneH + " AND subclass IN (0,4,13,15)");
                }
                else
                {
                    equip(oneH + " AND subclass IN (0,4,13,15)");
                    equip(shield);
                }
                break;
            case CLASS_MONK:
                if (!equip(twoH + " AND subclass IN (6,10)"))
                {
                    equip(oneH + " AND subclass IN (0,4,7,13)");
                    equip(held);
                }
                break;
            case CLASS_DRUID:
                if (!equip(twoH + " AND subclass IN (6,10)"))
                {
                    equip(oneH + " AND subclass IN (4,13,15)");
                    equip(held);
                }
                break;
            case CLASS_PRIEST:
                if (!equip(twoH + " AND subclass=10"))
                {
                    equip(oneH + " AND subclass IN (4,15)");
                    equip(held);
                }
                break;
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                if (!equip(twoH + " AND subclass=10"))
                {
                    equip(oneH + " AND subclass IN (7,15)");
                    equip(held);
                }
                break;
            default:
                break;
        }
    }

    // Fills every gear slot with the best level/class/spec-appropriate items.
    void GearBot(Player* bot, BotRole role, uint32 specId, int maxQuality)
    {
        uint8 const cls = bot->getClass();
        uint32 const primary = PrimaryStatForClassRole(cls, role, specId);
        std::string const armorRange =
            "class=4 AND subclass BETWEEN 1 AND " + std::to_string(uint32(ArmorTypeForClass(cls)));

        ClearEquipment(bot);

        // Best equippable item for a slot, optionally excluding an already-picked
        // entry (used to place two distinct rings/trinkets).
        auto equipOne = [&](std::string const& where, uint32 exclude) -> uint32
        {
            return EquipBestForSlot(bot, where, primary, exclude, maxQuality);
        };

        equipOne(armorRange + " AND InventoryType=1", 0);        // head
        equipOne(armorRange + " AND InventoryType=3", 0);        // shoulders
        equipOne(armorRange + " AND InventoryType IN (5,20)", 0);// chest
        equipOne(armorRange + " AND InventoryType=6", 0);        // waist
        equipOne(armorRange + " AND InventoryType=7", 0);        // legs
        equipOne(armorRange + " AND InventoryType=8", 0);        // feet
        equipOne(armorRange + " AND InventoryType=9", 0);        // wrists
        equipOne(armorRange + " AND InventoryType=10", 0);       // hands

        equipOne("class=4 AND InventoryType=2", 0);              // neck
        equipOne("class=4 AND InventoryType=16", 0);             // cloak

        uint32 ring1 = equipOne("class=4 AND InventoryType=11", 0);
        equipOne("class=4 AND InventoryType=11", ring1);
        uint32 trinket1 = equipOne("class=4 AND InventoryType=12", 0);
        equipOne("class=4 AND InventoryType=12", trinket1);

        GearWeapons(bot, role, specId, maxQuality);
    }

    // Rest is spell-based (Refreshment 128701) — no bag food/drink.
    void GiveRestConsumables(Player* /*bot*/)
    {
    }
}

uint32 PlayerbotMgr::CreateBotPopulation(std::string* report)
{
    if (_autoAccountCount == 0)
    {
        if (report)
            *report = "Playerbots.AutoCreate.AccountCount is 0; nothing to create.";
        return 0;
    }

    uint8 expansion = uint8(sWorld->getIntConfig(WorldIntConfigs::CONFIG_EXPANSION));

    uint32 accountsCreated = 0;
    uint32 charsCreated = 0;

    for (uint32 i = 1; i <= _autoAccountCount; ++i)
    {
        std::string username = _accountPrefix + std::to_string(i);

        uint32 accountId = AccountMgr::GetId(username);
        if (!accountId)
        {
            AccountOpResult res = sAccountMgr->CreateAccount(username, _autoPassword, "");
            if (res != AccountOpResult::AOR_OK)
            {
                SF_LOG_ERROR("modules", "[mod-playerbots] Failed to create bot account '%s' (error %u).",
                    username.c_str(), uint32(res));
                continue;
            }

            accountId = AccountMgr::GetId(username);
            if (!accountId)
                continue;

            ++accountsCreated;

            // Unlock every race/class by matching the realm's expansion.
            PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_EXPANSION);
            stmt->setUInt8(0, expansion);
            stmt->setUInt32(1, accountId);
            LoginDatabase.Execute(stmt);
        }

        charsCreated += PopulateAccount(accountId);
    }

    // Newly created characters are candidates; refresh the pool on the next tick.
    _candidatesLoaded = false;

    SF_LOG_INFO("modules", "[mod-playerbots] Auto-create complete: %u new account(s), %u new character(s).",
        accountsCreated, charsCreated);

    if (report)
    {
        std::ostringstream ss;
        ss << "Auto-create complete: " << accountsCreated << " new account(s), "
           << charsCreated << " new character(s).";
        *report = ss.str();
    }

    return charsCreated;
}

void PlayerbotMgr::InitializeBot(Player* bot, int roleOverride, uint32 specOverride, int maxItemQuality)
{
    if (!bot || !bot->IsInWorld())
        return;

    SF_LOG_INFO("modules", "[mod-playerbots] Initializing bot '%s' (level %u %s %s)...",
        bot->GetName().c_str(), bot->getLevel(),
        SafeRaceName(bot->getRace()), SafeClassName(bot->getClass()));

    // Specialization + spells. Explicit specOverride wins; otherwise a role
    // override picks the default tab for that role; otherwise keep current.
    // Below level 10 specs are unavailable.
    BotRole role = BOT_ROLE_DPS;
    uint32 specId = 0;
    if (bot->getLevel() >= 10)
    {
        if (specOverride)
        {
            if (ChrSpecializationEntry const* entry = sChrSpecializationStore.LookupEntry(specOverride))
            {
                if (entry->classId == bot->getClass())
                    specId = specOverride;
            }
            if (!specId)
            {
                // Unknown or wrong-class spec id; keep existing state.
                specId = bot->GetTalentSpecialization(bot->GetActiveSpec());
                if (!specId)
                    specId = SpecIdForRole(bot->getClass(), BOT_ROLE_DPS);
                role = RoleFromSpec(bot->getClass(), specId);
            }
            else
                role = RoleFromSpec(bot->getClass(), specId);
        }
        else if (roleOverride >= 0)
        {
            role = static_cast<BotRole>(roleOverride);
            specId = SpecIdForRole(bot->getClass(), role);
        }
        else
        {
            specId = bot->GetTalentSpecialization(bot->GetActiveSpec());
            if (!specId)
                specId = SpecIdForRole(bot->getClass(), BOT_ROLE_DPS);
            role = RoleFromSpec(bot->getClass(), specId);
        }

        if (specId)
        {
            SF_LOG_INFO("modules", "[mod-playerbots]   '%s': learning specialization %s (%s)...",
                bot->GetName().c_str(), SpecName(specId), RoleName(role));
            bot->LearnSpecialization(specId);
        }

        SF_LOG_INFO("modules", "[mod-playerbots]   '%s': initializing talents...",
            bot->GetName().c_str());
        BotRotation::ApplyRecommendedTalents(bot);

        SF_LOG_INFO("modules", "[mod-playerbots]   '%s': initializing glyphs...",
            bot->GetName().c_str());
        BotRotation::ApplyRecommendedGlyphs(bot);
    }
    else if (roleOverride >= 0)
    {
        role = static_cast<BotRole>(roleOverride);
        SF_LOG_INFO("modules", "[mod-playerbots]   '%s': below level 10; skipping specialization (role %s).",
            bot->GetName().c_str(), RoleName(role));
    }

    // Armor/weapon skills for this level, then trainer spells, riding/mounts,
    // then gear. Re-running init after a delevel simply skips plate/mail and
    // higher riding tiers the bot is no longer high enough for.
    SF_LOG_INFO("modules", "[mod-playerbots]   '%s': initializing skills and spells...",
        bot->GetName().c_str());
    LearnArmorProficiencies(bot);
    LearnClassTrainerSpells(bot);
    LearnLevelClassSpells(bot, specId);

    SF_LOG_INFO("modules", "[mod-playerbots]   '%s': initializing riding and mounts...",
        bot->GetName().c_str());
    LearnRidingAndMounts(bot);

    // Gear the bot for the (possibly newly assigned) role/spec at its level.
    int const qualityCap = std::max(0, std::min(maxItemQuality, int(ITEM_QUALITY_LEGENDARY)));
    SF_LOG_INFO("modules", "[mod-playerbots]   '%s': initializing equipment (max quality %s)...",
        bot->GetName().c_str(), QualityName(qualityCap));
    GearBot(bot, role, specId, qualityCap);
    GiveRestConsumables(bot);

    bot->SaveToDB();

    if (auto it = _ai.find(bot->GetGUID()); it != _ai.end() && it->second)
        it->second->ResetStrategiesToRoleDefaults();

    SF_LOG_INFO("modules",
        "[mod-playerbots] Initialized bot '%s': level %u %s %s, spec %s (%s), gear <= %s.",
        bot->GetName().c_str(), bot->getLevel(),
        SafeRaceName(bot->getRace()), SafeClassName(bot->getClass()),
        SpecName(specId), RoleName(role), QualityName(qualityCap));
}

uint32 PlayerbotMgr::InitializeAllBots(int roleOverride, uint32 specOverride, int maxItemQuality)
{
    uint32 count = 0;
    for (auto const& pair : _bots)
    {
        WorldSession* session = pair.second;
        Player* bot = session ? session->GetPlayer() : nullptr;
        if (bot && bot->IsInWorld())
        {
            // Skip bots whose class cannot use an explicit spec override.
            if (specOverride)
            {
                ChrSpecializationEntry const* entry = sChrSpecializationStore.LookupEntry(specOverride);
                if (!entry || entry->classId != bot->getClass())
                    continue;
            }
            InitializeBot(bot, roleOverride, specOverride, maxItemQuality);
            ++count;
        }
    }
    return count;
}

uint32 PlayerbotMgr::PopulateAccount(uint32 accountId)
{
    uint32 existing = AccountMgr::GetCharactersCount(accountId);
    uint32 created = 0;

    for (uint32 n = existing; n < _autoCharsPerAccount; ++n)
        if (CreateOneCharacter(accountId))
            ++created;

    return created;
}

bool PlayerbotMgr::CreateOneCharacter(uint32 accountId)
{
    bool alliance = RollPct() < _autoAlliancePct;

    uint32 roll = RollPct();
    BotRole role;
    if (roll < _autoTankPct)
        role = BOT_ROLE_TANK;
    else if (roll < _autoTankPct + _autoHealerPct)
        role = BOT_ROLE_HEALER;
    else
        role = BOT_ROLE_DPS;

    uint8 cls = 0;
    uint8 race = 0;
    for (int attempt = 0; attempt < 20 && !race; ++attempt)
    {
        cls = PickClassForRole(role);
        race = PickRaceForClassFaction(cls, alliance);
    }

    if (!race || !cls)
    {
        SF_LOG_ERROR("modules", "[mod-playerbots] No valid race/class combo for account %u.", accountId);
        return false;
    }

    uint8 gender = uint8(std::rand() % 2);

    std::string name;
    for (int attempt = 0; attempt < 50; ++attempt)
    {
        std::string candidate = GenerateBotName();
        if (ObjectMgr::CheckPlayerName(candidate, true) != ResponseCodes::CHAR_NAME_SUCCESS)
            continue;
        if (sObjectMgr->GetPlayerGUIDByName(candidate))
            continue;
        name = candidate;
        break;
    }

    if (name.empty())
    {
        SF_LOG_ERROR("modules", "[mod-playerbots] Could not generate a unique name for account %u.", accountId);
        return false;
    }

    uint8 expansion = uint8(sWorld->getIntConfig(WorldIntConfigs::CONFIG_EXPANSION));
    uint32 maxLevel = sWorld->getIntConfig(WorldIntConfigs::CONFIG_MAX_PLAYER_LEVEL);
    uint8 level = uint8(std::min<uint32>(_autoLevel, maxLevel));

    WorldSession* sess = new WorldSession(accountId, nullptr, AccountTypes::SEC_PLAYER, expansion,
        0, LOCALE_enUS, 0, false, false);
    sess->SetBot(true);
    // Save the character on the same realm as real characters so its name
    // resolves for other clients (name query / who list use the realm id).
    sess->SetVirtualRealmID(realmID);

    uint32 specId = SpecIdForRole(cls, role);
    uint32 guid = sess->CreateBotCharacter(name, race, cls, gender, 0, 0, 0, 0, 0, level, specId);

    delete sess;

    if (!guid)
    {
        SF_LOG_ERROR("modules", "[mod-playerbots] CreateBotCharacter failed (account %u, race %u, class %u).",
            accountId, race, cls);
        return false;
    }

    SF_LOG_INFO("modules", "[mod-playerbots] Created bot '%s' (GUID %u, %s, race %u, class %u, level %u) on account %u.",
        name.c_str(), guid, alliance ? "Alliance" : "Horde", race, cls, level, accountId);
    return true;
}
