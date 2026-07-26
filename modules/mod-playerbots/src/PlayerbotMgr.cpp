/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "PlayerbotMgr.h"
#include "PlayerbotAI.h"
#include "rotations/BotRotation.h"
#include "AccountMgr.h"
#include "BotPreferredMounts.h"
#include "BotTeleportMaps.h"
#include "BotVendorHubs.h"
#include "Chat.h"
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
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
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
    _loginIntervalMs = uint32(sConfigMgr->GetIntDefault("Playerbots.RandomBots.LoginInterval", 250));
    if (_loginIntervalMs < 1)
        _loginIntervalMs = 1;
    _maxPendingLogins = uint32(sConfigMgr->GetIntDefault("Playerbots.RandomBots.MaxPendingLogins", 8));
    if (_maxPendingLogins < 1)
        _maxPendingLogins = 1;
    if (_maxPendingLogins > 64)
        _maxPendingLogins = 64;
    _loginsPerTick = uint32(sConfigMgr->GetIntDefault("Playerbots.RandomBots.LoginsPerTick", 1));
    if (_loginsPerTick < 1)
        _loginsPerTick = 1;
    if (_loginsPerTick > 10)
        _loginsPerTick = 10;

    _joinLfg = sConfigMgr->GetBoolDefault("Playerbots.RandomBotJoinLfg", false);
    _joinLfgMaxBots = uint32(sConfigMgr->GetIntDefault("Playerbots.RandomBotJoinLfg.MaxBots", 10));
    _joinLfgLevelRange = uint32(sConfigMgr->GetIntDefault("Playerbots.RandomBotJoinLfg.LevelRange", 5));
    if (_joinLfgMaxBots > 40)
        _joinLfgMaxBots = 40;
    if (_joinLfgLevelRange > 20)
        _joinLfgLevelRange = 20;

    _autoCreateOnStartup = sConfigMgr->GetBoolDefault("Playerbots.AutoCreate.OnStartup", false);
    _deleteRandomBotAccounts = sConfigMgr->GetBoolDefault("Playerbots.DeleteRandomBotAccounts", false);
    _autoAccountCount = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.AccountCount", 0));
    _autoPassword = sConfigMgr->GetStringDefault("Playerbots.AutoCreate.AccountPassword", "password");
    _autoCharsPerAccount = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.CharactersPerAccount", 1));
    _autoAlliancePct = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.AlliancePct", 50));
    _autoTankPct = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.TankPct", 20));
    _autoHealerPct = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.HealerPct", 20));

    // Legacy AutoCreate.Level seeds Min/Max when those keys are omitted.
    uint32 const legacyLevel = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.Level", 1));
    _autoMinLevel = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.MinLevel", int32(legacyLevel)));
    _autoMaxLevel = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.MaxLevel", int32(legacyLevel)));
    _autoInitOnCreate = sConfigMgr->GetBoolDefault("Playerbots.AutoCreate.InitOnCreate", false);
    _autoTeleportOnInit = sConfigMgr->GetBoolDefault("Playerbots.AutoCreate.TeleportOnInit", true);
    _initPerTick = uint32(sConfigMgr->GetIntDefault("Playerbots.Init.PerTick", 5));
    if (_initPerTick < 1)
        _initPerTick = 1;
    if (_initPerTick > 50)
        _initPerTick = 50;

    _gearMinQuality = sConfigMgr->GetIntDefault("Playerbots.Gear.MinQuality", int(ITEM_QUALITY_UNCOMMON));
    _gearMaxQuality = sConfigMgr->GetIntDefault("Playerbots.Gear.MaxQuality", int(ITEM_QUALITY_EPIC));

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
    _vendorSellGreens = sConfigMgr->GetBoolDefault("Playerbots.Vendor.SellGreens", true);
    _vendorHubMaxDistance = float(sConfigMgr->GetFloatDefault("Playerbots.Vendor.HubMaxDistance", 500.0f));
    _questAutoPickReward = sConfigMgr->GetBoolDefault("Playerbots.Quest.AutoPickReward", true);

    // Clamp to sane ranges so bad config can't create invalid characters.
    if (_autoAlliancePct > 100) _autoAlliancePct = 100;
    if (_autoTankPct > 100) _autoTankPct = 100;
    if (_autoHealerPct > 100) _autoHealerPct = 100;
    if (_autoTankPct + _autoHealerPct > 100) _autoHealerPct = 100 - _autoTankPct;
    if (_autoCharsPerAccount > 11) _autoCharsPerAccount = 11;   // realm cap
    if (_autoMinLevel < 1) _autoMinLevel = 1;
    if (_autoMaxLevel < 1) _autoMaxLevel = 1;
    if (_autoMaxLevel < _autoMinLevel)
        std::swap(_autoMinLevel, _autoMaxLevel);
    {
        uint32 const maxPlayerLevel = uint32(sWorld->getIntConfig(WorldIntConfigs::CONFIG_MAX_PLAYER_LEVEL));
        if (_autoMinLevel > maxPlayerLevel) _autoMinLevel = maxPlayerLevel;
        if (_autoMaxLevel > maxPlayerLevel) _autoMaxLevel = maxPlayerLevel;
    }
    if (_gearMinQuality < int(ITEM_QUALITY_POOR)) _gearMinQuality = int(ITEM_QUALITY_POOR);
    if (_gearMaxQuality > int(ITEM_QUALITY_LEGENDARY)) _gearMaxQuality = int(ITEM_QUALITY_LEGENDARY);
    if (_gearMinQuality > _gearMaxQuality)
        std::swap(_gearMinQuality, _gearMaxQuality);
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
        SF_LOG_INFO("modules", "[mod-playerbots] Enabled (random bots: %s, max: %u, account prefix: '%s', LFG join: %s, levels %u-%u, gear quality %d-%d).",
            _randomBotsEnabled ? "on" : "off", _maxRandomBots, _accountPrefix.c_str(),
            _joinLfg ? "on" : "off", _autoMinLevel, _autoMaxLevel, _gearMinQuality, _gearMaxQuality);
    else
        SF_LOG_INFO("modules", "[mod-playerbots] Disabled via configuration.");
}

void PlayerbotMgr::Update(uint32 diff)
{
    if (!_enabled)
        return;

    // Drop bots whose player has left the world for any reason.
    CleanupDeadBots();

    // Finish ready async logins (at most LoginsPerTick LoadFromDB per tick).
    ProcessPendingLogins();

    // Mass gear init must run on the world thread, a few bots per tick.
    // This is NOT the random-bot login rate — see RandomBots.LoginsPerTick.
    ProcessInitQueue();

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

    // Start at most one new async login per interval while under caps.
    _loginTimer += diff;
    if (_loginTimer < _loginIntervalMs)
        return;
    _loginTimer = 0;

    uint32 const onlineOrPending = uint32(_randomBots.size() + _pendingLogins.size());
    if (onlineOrPending < _maxRandomBots && _pendingLogins.size() < _maxPendingLogins)
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

    // Cache accountId with guid so BeginSpawnBot skips GetPlayerAccountIdByGUID.
    QueryResult characters = CharacterDatabase.PQuery(
        "SELECT guid, account FROM characters WHERE account IN (%s)", accountIds.str().c_str());
    if (!characters)
    {
        SF_LOG_INFO("modules", "[mod-playerbots] Bot accounts have no characters (prefix '%s').", _accountPrefix.c_str());
        return;
    }

    do
    {
        Field* fields = characters->Fetch();
        BotCandidate c;
        c.guid = MAKE_NEW_GUID(fields[0].GetUInt32(), 0, HIGHGUID_PLAYER);
        c.accountId = fields[1].GetUInt32();
        _candidates.push_back(c);
    } while (characters->NextRow());

    SF_LOG_INFO("modules", "[mod-playerbots] Loaded %u candidate bot character(s) from prefix '%s'.",
        uint32(_candidates.size()), _accountPrefix.c_str());
}

void PlayerbotMgr::TrySpawnRandomBot()
{
    for (BotCandidate const& candidate : _candidates)
    {
        uint64 const guid = candidate.guid;
        if (_bots.find(guid) != _bots.end())
            continue;
        if (_pendingLoginGuids.find(guid) != _pendingLoginGuids.end())
            continue;
        if (ObjectAccessor::FindPlayer(guid))
            continue;

        std::string error;
        if (BeginSpawnBot(guid, true, candidate.accountId, &error))
        {
            SF_LOG_INFO("modules", "[mod-playerbots] Random bot login started (pending %u, online %u/%u).",
                uint32(_pendingLogins.size()), uint32(_randomBots.size()), _maxRandomBots);
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
        // Only drop bots whose Player is gone. !IsInWorld() alone is wrong —
        // LFG/group teleports briefly take the player out of the world, and deleting
        // the session there permanently despawns the bot (who-list empty, "vanished
        // after uninvite").
        if (!session || !player)
        {
            uint64 guid = it->first;
            it = _bots.erase(it);
            _randomBots.erase(guid);
            DestroyBotAI(guid);
            delete session;
            continue;
        }

        // Socketless bots never ACK worldport / near-teleport. Finish those so
        // they re-enter the world and show up in /who again.
        if (session->IsBot() && player->IsBeingTeleported())
            session->FinalizeBotTeleport();

        ++it;
    }
}

// Party LFG (master invites bots, then queues) is handled by PlayerbotAI::HandleLfg.
// Never call JoinLfg on solo bots — that creates separate queue entries that match
// into bot-only groups and pull them out of the open world.
void PlayerbotMgr::UpdateLfgAutoJoin(uint32 diff)
{
    if (_bots.empty())
        return;

    _lfgJoinTimer += diff;
    if (_lfgJoinTimer < 2000)
        return;
    _lfgJoinTimer = 0;

    // Kick any bot that is solo-queued (no real party). Leave grouped bots alone —
    // they are filling a master's party queue via role-check / proposal answers.
    for (auto const& pair : _bots)
    {
        WorldSession* session = pair.second;
        Player* bot = session ? session->GetPlayer() : nullptr;
        if (!bot || !bot->IsInWorld())
            continue;

        Group* group = bot->GetGroup();
        if (!group)
        {
            lfg::LfgState const botState = sLFGMgr->GetState(bot->GetGUID());
            if (botState == lfg::LFG_STATE_NONE || botState == lfg::LFG_STATE_DUNGEON
                || botState == lfg::LFG_STATE_FINISHED_DUNGEON || botState == lfg::LFG_STATE_BOOT)
                continue;

            sLFGMgr->LeaveLfg(bot->GetGUID());
            SF_LOG_DEBUG("modules", "[mod-playerbots] Left solo LFG queue for bot '%s' (stay in world).",
                bot->GetName().c_str());
            continue;
        }

        // Bot-only LFG dungeon groups: eject so they return to the open world.
        if (!group->isLFGGroup())
            continue;

        bool hasRealPlayer = false;
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->GetSession() && !member->GetSession()->IsBot())
            {
                hasRealPlayer = true;
                break;
            }
        }
        if (hasRealPlayer)
            continue;

        sLFGMgr->LeaveLfg(bot->GetGUID());
        if (bot->GetMap() && bot->GetMap()->IsDungeon())
        {
            sLFGMgr->TeleportPlayer(bot, true);
            if (bot->GetSession() && bot->GetSession()->IsBot())
                bot->GetSession()->FinalizeBotTeleport();
        }
        bot->RemoveFromGroup(GROUP_REMOVEMETHOD_LEAVE);
        SF_LOG_DEBUG("modules", "[mod-playerbots] Ejected bot '%s' from bot-only LFG group.",
            bot->GetName().c_str());
    }

    // Legacy RandomBotJoinLfg JoinLfg fill removed: it queued bots independently and
    // they formed bot-only LFG groups. Invite bots to your party, then queue.
    (void)_joinLfg;
    (void)_joinLfgMaxBots;
    (void)_joinLfgLevelRange;
}

bool PlayerbotMgr::AddBot(uint64 characterGuid, std::string* errorOut)
{
    return SpawnBot(characterGuid, false, errorOut);
}

bool PlayerbotMgr::SpawnBot(uint64 characterGuid, bool isRandom, std::string* errorOut,
    uint32 accountIdOverride)
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

    if (_pendingLoginGuids.find(characterGuid) != _pendingLoginGuids.end())
        return fail("That character is already logging in.");

    // Refuse if the character is already in the world (real player or otherwise);
    // a second Player with the same GUID would collide in the object accessor.
    if (ObjectAccessor::FindPlayer(characterGuid))
        return fail("That character is already online.");

    uint32 accountId = accountIdOverride;
    if (!accountId)
        accountId = sObjectMgr->GetPlayerAccountIdByGUID(characterGuid);
    if (!accountId)
        return fail("Could not resolve the character's account.");

    uint8 expansion = uint8(sWorld->getIntConfig(WorldIntConfigs::CONFIG_EXPANSION));

    // Bots always run at player security regardless of the owning account's level.
    WorldSession* botSession = new WorldSession(accountId, nullptr, AccountTypes::SEC_PLAYER, expansion,
        0, LOCALE_enUS, 0, false, false);
    botSession->SetBot(true);
    uint32 const botRealm = ResolveBotVirtualRealm();
    botSession->SetVirtualRealmID(botRealm);

    if (!botSession->LoginBotCharacter(characterGuid))
    {
        delete botSession;
        return fail("Failed to load the character into the world.");
    }

    PendingBotLogin pending;
    pending.session = botSession;
    pending.characterGuid = characterGuid;
    pending.isRandom = isRandom;
    pending.botRealm = botRealm;
    FinishSpawnBot(pending);
    if (_bots.find(characterGuid) == _bots.end())
        return fail("Failed to load the character into the world.");

    return true;
}

uint32 PlayerbotMgr::ResolveBotVirtualRealm()
{
    if (_botVirtualRealm && _botVirtualRealm != uint32(-1))
        return _botVirtualRealm;

    {
        SF_SHARED_GUARD readGuard(*HashMapHolder<Player>::GetLock());
        for (auto const& kv : sObjectAccessor->GetPlayers())
        {
            Player* p = kv.second;
            if (!p || !p->GetSession() || p->GetSession()->IsBot())
                continue;
            uint32 r = p->GetSession()->GetVirtualRealmID();
            if (r && r != uint32(-1))
            {
                _botVirtualRealm = r;
                return _botVirtualRealm;
            }
        }
    }

    _botVirtualRealm = (realmID && realmID != uint32(-1)) ? realmID : 1;
    return _botVirtualRealm;
}

bool PlayerbotMgr::BeginSpawnBot(uint64 characterGuid, bool isRandom, uint32 accountId,
    std::string* errorOut)
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
    if (_pendingLoginGuids.find(characterGuid) != _pendingLoginGuids.end())
        return fail("That character is already logging in.");
    if (ObjectAccessor::FindPlayer(characterGuid))
        return fail("That character is already online.");
    if (!accountId)
        accountId = sObjectMgr->GetPlayerAccountIdByGUID(characterGuid);
    if (!accountId)
        return fail("Could not resolve the character's account.");

    uint8 expansion = uint8(sWorld->getIntConfig(WorldIntConfigs::CONFIG_EXPANSION));
    WorldSession* botSession = new WorldSession(accountId, nullptr, AccountTypes::SEC_PLAYER, expansion,
        0, LOCALE_enUS, 0, false, false);
    botSession->SetBot(true);
    uint32 const botRealm = ResolveBotVirtualRealm();
    botSession->SetVirtualRealmID(botRealm);

    if (!botSession->BeginBotCharacterLogin(characterGuid))
    {
        delete botSession;
        return fail("Failed to start character login.");
    }

    PendingBotLogin pending;
    pending.session = botSession;
    pending.characterGuid = characterGuid;
    pending.isRandom = isRandom;
    pending.botRealm = botRealm;
    pending.startedMs = getMSTime();
    _pendingLogins.push_back(pending);
    _pendingLoginGuids.insert(characterGuid);
    return true;
}

void PlayerbotMgr::ProcessPendingLogins()
{
    if (_pendingLogins.empty())
        return;

    uint32 completed = 0;
    for (auto it = _pendingLogins.begin(); it != _pendingLogins.end();)
    {
        PendingBotLogin& pending = *it;
        if (!pending.session)
        {
            _pendingLoginGuids.erase(pending.characterGuid);
            it = _pendingLogins.erase(it);
            continue;
        }

        // Abandon stalled logins so a wedged DB query cannot fill the pipeline.
        uint32 const ageMs = GetMSTimeDiffToNow(pending.startedMs);
        if (!pending.discard && ageMs > 30000)
        {
            SF_LOG_ERROR("modules",
                "[mod-playerbots] Async bot login timed out (GUID %u) after %u ms.",
                GUID_LOPART(pending.characterGuid), ageMs);
            pending.discard = true;
        }

        // Limit world-thread LoadFromDB work per tick (discard polls are cheap).
        if (!pending.discard && completed >= _loginsPerTick)
        {
            ++it;
            continue;
        }

        WorldSession::BotLoginPollStatus const status =
            pending.session->PollBotCharacterLogin(pending.discard);

        if (status == WorldSession::BotLoginPollStatus::Pending)
        {
            ++it;
            continue;
        }

        if (status == WorldSession::BotLoginPollStatus::Success && !pending.discard)
        {
            FinishSpawnBot(pending);
            if (_bots.find(pending.characterGuid) != _bots.end())
            {
                ++completed;
                SF_LOG_INFO("modules", "[mod-playerbots] Random bot pool: %u/%u online (%u login pending).",
                    uint32(_randomBots.size()), _maxRandomBots,
                    uint32(_pendingLogins.size() > 0 ? _pendingLogins.size() - 1 : 0));
            }
        }
        else
        {
            AbortPendingLogin(pending);
        }

        _pendingLoginGuids.erase(pending.characterGuid);
        it = _pendingLogins.erase(it);
    }
}

void PlayerbotMgr::AbortPendingLogin(PendingBotLogin& pending)
{
    if (!pending.session)
        return;
    delete pending.session;
    pending.session = nullptr;
}

void PlayerbotMgr::FinishSpawnBot(PendingBotLogin& pending)
{
    WorldSession* botSession = pending.session;
    uint64 const characterGuid = pending.characterGuid;
    bool const isRandom = pending.isRandom;
    uint32 const botRealm = pending.botRealm;
    if (!botSession)
        return;

    // Bots have no client to ACK login teleports (instance eject / homebind).
    if (Player* loggedIn = botSession->GetPlayer())
    {
        if (loggedIn->IsBeingTeleported())
            botSession->FinalizeBotTeleport();
        botSession->SetVirtualRealmID(botRealm);
        loggedIn->SetUInt32Value(PLAYER_FIELD_VIRTUAL_PLAYER_REALM, botRealm);
    }

    if (!botSession->GetPlayer() || !botSession->GetPlayer()->IsInWorld())
    {
        AbortPendingLogin(pending);
        return;
    }

    _bots[characterGuid] = botSession;
    if (isRandom)
        _randomBots.insert(characterGuid);

    if (Player* bot = botSession->GetPlayer())
    {
        // Deferred create: gear/spells on first world login when still bare.
        // Already-init'd bots skip this — login is LoadFromDB + AI only.
        if (BotNeedsGearInit(bot))
        {
            uint32 specId = bot->GetTalentSpecialization(bot->GetActiveSpec());
            InitializeBot(bot, -1, specId, -1, false, _autoTeleportOnInit);
        }

        _ai[characterGuid] = new PlayerbotAI(bot);
        if (isRandom)
        {
            if (PlayerbotAI* ai = _ai[characterGuid])
            {
                ai->GetStrategyEngine().ChangeStrategy("+quests,+grind,-follow", BotState::NonCombat);
                ai->GetStrategyEngine().Add("grind", BotState::Combat);
                ai->SyncFlagsFromStrategies();
            }
        }
        char const* raceName = GetRaceName(bot->getRace(), LOCALE_enUS);
        char const* className = GetClassName(bot->getClass(), LOCALE_enUS);
        SF_LOG_INFO("modules",
            "[mod-playerbots] Bot '%s' entered the world (GUID %u, level %u %s %s%s).",
            bot->GetName().c_str(), GUID_LOPART(characterGuid), bot->getLevel(),
            raceName ? raceName : "?", className ? className : "?",
            isRandom ? ", random" : "");
    }

    // Ownership transferred to _bots; clear so Abort won't delete.
    pending.session = nullptr;
}

void PlayerbotMgr::UpdateBotAI(Player* bot, uint32 diff)
{
    if (!bot)
        return;

    auto it = _ai.find(bot->GetGUID());
    if (it != _ai.end())
        it->second->UpdateAI(diff);
}

void PlayerbotMgr::SyncNearbyBotVisibility(Player* viewer, uint32 diff)
{
    if (!_enabled || !viewer || !viewer->IsInWorld() || !viewer->GetSession())
        return;
    if (viewer->GetSession()->IsBot())
        return;

    uint32& timer = _viewerVisTimers[viewer->GetGUID()];
    timer += diff;
    if (timer < 500)
        return;
    timer = 0;

    Map* map = viewer->GetMap();
    if (!map)
        return;

    float const range = map->GetVisibilityRange();
    uint32 const viewerRealm = viewer->GetSession()->GetVirtualRealmID();
    for (auto const& pair : _bots)
    {
        Player* bot = pair.second ? pair.second->GetPlayer() : nullptr;
        if (!bot || !bot->IsInWorld() || bot->GetMap() != map)
            continue;

        // Align bot session VR with real players (name query / who). Never write
        // PLAYER_FIELD_VIRTUAL_PLAYER_REALM here — update-field spam blanked /who.
        if (viewerRealm && viewerRealm != uint32(-1)
            && pair.second->GetVirtualRealmID() != viewerRealm)
            pair.second->SetVirtualRealmID(viewerRealm);

        if (viewer->GetDistance(bot) > range)
            continue;

        if (PlayerbotAI* ai = GetBotAI(bot->GetGUID()))
            ai->SyncPhaseWithMaster(viewer);
    }
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
    for (PendingBotLogin& pending : _pendingLogins)
    {
        if (pending.session)
            pending.session->PollBotCharacterLogin(true);
        AbortPendingLogin(pending);
    }
    _pendingLogins.clear();
    _pendingLoginGuids.clear();

    for (auto& pair : _ai)
        delete pair.second;
    _ai.clear();

    for (auto& pair : _bots)
        delete pair.second;

    _bots.clear();
    _randomBots.clear();
    _selfBots.clear();
}

bool PlayerbotMgr::CanDeleteBotAccounts(std::string* errorOut) const
{
    auto fail = [&](char const* msg) -> bool
    {
        if (errorOut)
            *errorOut = msg;
        return false;
    };

    if (_accountPrefix.empty())
        return fail("Playerbots.RandomBots.AccountPrefix is empty; refusing wipe.");
    if (_accountPrefix.size() < 3)
        return fail("AccountPrefix is shorter than 3 characters; refusing wipe.");

    // Guard against accidentally wiping the whole account table.
    std::string upper = _accountPrefix;
    std::transform(upper.begin(), upper.end(), upper.begin(),
        [](unsigned char c) { return char(std::toupper(c)); });
    if (upper == "ADMIN" || upper == "GM" || upper == "PLAYER" || upper == "ACCOUNT")
        return fail("AccountPrefix looks unsafe; refusing wipe.");

    return true;
}

uint32 PlayerbotMgr::DeleteBotAccounts(std::string* report)
{
    std::string error;
    if (!CanDeleteBotAccounts(&error))
    {
        SF_LOG_ERROR("modules", "[mod-playerbots] %s", error.c_str());
        if (report)
            *report = error;
        return 0;
    }

    // Drop live bot sessions first so DeleteAccount does not race logouts.
    LogoutAllBots();

    QueryResult accounts = LoginDatabase.PQuery(
        "SELECT id, username FROM account WHERE username LIKE '%s%%'", _accountPrefix.c_str());
    if (!accounts)
    {
        std::string msg = "No bot accounts found for prefix '" + _accountPrefix + "'.";
        SF_LOG_INFO("modules", "[mod-playerbots] %s", msg.c_str());
        if (report)
            *report = msg;
        _candidates.clear();
        _candidatesLoaded = false;
        return 0;
    }

    uint32 deleted = 0;
    do
    {
        uint32 const accountId = (*accounts)[0].GetUInt32();
        std::string const username = (*accounts)[1].GetString();
        AccountOpResult res = AccountMgr::DeleteAccount(accountId);
        if (res == AccountOpResult::AOR_OK)
        {
            ++deleted;
            SF_LOG_INFO("modules", "[mod-playerbots] Deleted bot account '%s' (id %u).",
                username.c_str(), accountId);
        }
        else
        {
            SF_LOG_ERROR("modules", "[mod-playerbots] Failed to delete bot account '%s' (id %u, error %u).",
                username.c_str(), accountId, uint32(res));
        }
    } while (accounts->NextRow());

    _candidates.clear();
    _candidatesLoaded = false;

    std::ostringstream ss;
    ss << "Deleted " << deleted << " bot account(s) with prefix '" << _accountPrefix
       << "'. Set Playerbots.DeleteRandomBotAccounts = 0 before the next restart, "
       << "then run .playerbots create (or enable AutoCreate.OnStartup).";
    SF_LOG_INFO("modules", "[mod-playerbots] %s", ss.str().c_str());
    if (report)
        *report = ss.str();
    return deleted;
}

void PlayerbotMgr::DestroyBotAI(uint64 characterGuid)
{
    auto it = _ai.find(characterGuid);
    if (it == _ai.end())
        return;

    delete it->second;
    _ai.erase(it);
}

PlayerbotAI* PlayerbotMgr::GetBotAI(uint64 characterGuid) const
{
    auto it = _ai.find(characterGuid);
    return it != _ai.end() ? it->second : nullptr;
}

PlayerbotAI* PlayerbotMgr::GetBotAI(Player* bot) const
{
    return bot ? GetBotAI(bot->GetGUID()) : nullptr;
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
    // minQuality/maxQuality are ITEM_QUALITY_* bounds from conf or init override.
    // Results are cached — mass init of similar level/class bots hits memory.
    std::unordered_map<std::string, std::vector<uint32>> g_itemQueryCache;

    std::vector<uint32> QueryItemEntries(Player* bot, std::string const& where, uint32 primaryStat,
        uint32 exclude, int minQuality, int maxQuality)
    {
        uint32 const level    = bot->getLevel();
        uint32 const classBit = 1u << (bot->getClass() - 1);
        uint32 const raceBit  = 1u << (bot->getRace() - 1);
        uint32 const minReq   = level > 10 ? (level - 10) : 1;
        int qualityCap = std::max(0, std::min(maxQuality, int(ITEM_QUALITY_LEGENDARY)));
        int qualityMin = std::max(0, std::min(minQuality, qualityCap));

        std::ostringstream key;
        // v3: broader QA/template bans + rare-low-ilvl reject + min ItemLevel floor.
        key << "v3|" << where << '|' << level << '|' << classBit << '|' << raceBit << '|'
            << minReq << '|' << qualityMin << '|' << qualityCap << '|' << primaryStat
            << '|' << exclude;
        std::string const cacheKey = key.str();
        if (auto it = g_itemQueryCache.find(cacheKey); it != g_itemQueryCache.end())
            return it->second;

        std::ostringstream q;
        q << "SELECT entry FROM item_template WHERE " << where
          << " AND RequiredLevel <= " << level
          << " AND RequiredLevel >= " << minReq
          << " AND Quality BETWEEN " << qualityMin << " AND " << qualityCap
          << " AND duration = 0 AND startquest = 0"
          // Designer / QA / art / monster templates (RequiredLevel 1, junk names).
          << " AND name NOT LIKE 'QA%'"
          << " AND name NOT LIKE '%Test%'"
          << " AND name NOT LIKE 'ZG %'"
          << " AND name NOT LIKE '%Raid D0%'"
          << " AND name NOT LIKE '% D0%'"
          << " AND name NOT LIKE 'Monster -%'"
          << " AND name NOT LIKE 'Art Template%'"
          << " AND name NOT LIKE 'Artwork %'"
          << " AND (Flags & " << uint32(ITEM_PROTO_FLAG_DEPRECATED) << ") = 0"
          // Rare+ with tiny ilvl is almost always a leftover template (ZG/Icecrown stubs).
          << " AND NOT (Quality >= " << int(ITEM_QUALITY_RARE)
          << " AND ItemLevel < 10 AND RequiredLevel <= 10)"
          // Soft ilvl ceiling so low-req raid gear cannot dominate the pool.
          << " AND ItemLevel <= " << (level * 8u + 40u)
          // Prefer real level-scaled gear over ilvl-1 stubs.
          << " AND ItemLevel >= " << (level > 5 ? (level > 10 ? level - 5 : 3) : 1)
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

        // Prefer items the bot actually leveled into; only then pick the strongest.
        q << " ORDER BY RequiredLevel DESC, ItemLevel DESC LIMIT 25";

        std::vector<uint32> entries;
        if (QueryResult result = WorldDatabase.Query(q.str().c_str()))
        {
            do
            {
                entries.push_back(result->Fetch()[0].GetUInt32());
            } while (result->NextRow());
        }
        g_itemQueryCache.emplace(cacheKey, entries);
        return entries;
    }

    // Class trainer spell lists — built once per class (full creature scan is costly).
    std::unordered_map<uint8, std::vector<TrainerSpell const*>> g_trainerSpellCache;

    std::vector<TrainerSpell const*> const& GetClassTrainerSpells(uint8 cls)
    {
        auto it = g_trainerSpellCache.find(cls);
        if (it != g_trainerSpellCache.end())
            return it->second;

        std::vector<TrainerSpell const*> trainerSpells;
        std::unordered_set<uint32> seen;
        CreatureTemplateContainer const* templates = sObjectMgr->GetCreatureTemplates();
        if (templates)
        {
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
        }
        auto inserted = g_trainerSpellCache.emplace(cls, std::move(trainerSpells));
        return inserted.first->second;
    }

    // Level gate used by SkillLineAbility / SpecializationSpells. Prefer
    // SpellLevel, fall back to BaseLevel (some MoP rows store the gate there).
    uint32 SpellLearnLevel(SpellInfo const* info)
    {
        if (!info)
            return 0;
        if (info->SpellLevel)
            return info->SpellLevel;
        return info->BaseLevel;
    }

    // Hardcoded class skill lines — GetSkillIdByClass() can fail when ChrClasses
    // locale names do not match SkillLine names.
    uint32 ClassSkillId(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:      return SKILL_GENERAL_WARRIOR;
            case CLASS_PALADIN:      return SKILL_GENERAL_PALADIN;
            case CLASS_HUNTER:       return SKILL_GENERAL_HUNTER;
            case CLASS_ROGUE:        return SKILL_GENERAL_ROGUE;
            case CLASS_PRIEST:       return SKILL_GENERAL_PRIEST;
            case CLASS_DEATH_KNIGHT: return SKILL_GENERAL_DEATH_KNIGHT;
            case CLASS_SHAMAN:       return SKILL_GENERAL_SHAMAN;
            case CLASS_MAGE:         return SKILL_GENERAL_MAGE;
            case CLASS_WARLOCK:      return SKILL_GENERAL_WARLOCK;
            case CLASS_MONK:         return SKILL_GENERAL_MONK;
            case CLASS_DRUID:        return SKILL_GENERAL_DRUID;
            default:                 return GetSkillIdByClass(cls);
        }
    }

    // Independent learn so spells persist in character_spell (dependent=true is
    // skipped on SaveToDB — that left bots able to cast for one session only).
    void TryLearnSpell(Player* bot, uint32 spellId)
    {
        if (!bot || !spellId)
            return;

        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!info || !SpellMgr::IsSpellValid(info, bot, false))
            return;
        if (GetTalentSpellCost(spellId) > 0)
            return;

        uint32 const needLevel = SpellLearnLevel(info);
        if (needLevel && needLevel > bot->getLevel())
            return;

        // Already known as a saved independent spell — nothing to do.
        // If only known as dependent, remove and re-add independent so it saves.
        if (bot->HasSpell(spellId))
        {
            PlayerSpellMap::const_iterator itr = bot->GetSpellMap().find(spellId);
            if (itr != bot->GetSpellMap().end() && itr->second && !itr->second->dependent
                && itr->second->state != PLAYERSPELL_REMOVED)
                return;
            bot->removeSpell(spellId, false, false);
        }

        if (!bot->IsSpellFitByClassAndRace(spellId))
            return;

        bot->learnSpell(spellId, false);
    }

    // Teach every class-trainer spell the bot qualifies for at its current level
    // (multi-pass so spell-chain prerequisites can unlock in order).
    void LearnClassTrainerSpells(Player* bot)
    {
        if (!bot)
            return;

        std::vector<TrainerSpell const*> const& trainerSpells = GetClassTrainerSpells(bot->getClass());

        for (int pass = 0; pass < 12; ++pass)
        {
            bool learnedAny = false;
            for (TrainerSpell const* ts : trainerSpells)
            {
                if (!ts)
                    continue;

                // If learnedSpell[] was left empty by a LEARN_SPELL edge case,
                // GetTrainerSpellState reports GRAY (already known) incorrectly.
                // Fall back to teaching ts->spell when the bot lacks it and meets
                // the level requirement.
                TrainerSpellState const state = bot->GetTrainerSpellState(ts);
                if (state == TRAINER_SPELL_GREEN)
                {
                    if (ts->IsCastable())
                        bot->CastSpell(bot, ts->spell, true);
                    else
                        bot->learnSpell(ts->spell, false);
                    learnedAny = true;
                    continue;
                }

                if (state != TRAINER_SPELL_GRAY)
                    continue;

                bool anyLearnedSlot = false;
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                {
                    if (ts->learnedSpell[i])
                    {
                        anyLearnedSlot = true;
                        break;
                    }
                }
                if (anyLearnedSlot)
                    continue;
                if (!sSpellMgr->GetSpellInfo(ts->spell))
                    continue;
                if (bot->HasSpell(ts->spell))
                    continue;
                if (ts->reqLevel && bot->getLevel() < ts->reqLevel)
                    continue;
                if (!bot->IsSpellFitByClassAndRace(ts->spell))
                    continue;

                bot->learnSpell(ts->spell, false);
                learnedAny = true;
            }
            if (!learnedAny)
                break;
        }

        // Convert trainer spells known only as dependent (unsaved) into
        // independent spells, and pick up anything the GREEN pass missed.
        for (TrainerSpell const* ts : trainerSpells)
        {
            if (!ts || !ts->spell)
                continue;
            if (ts->reqLevel && bot->getLevel() < ts->reqLevel)
                continue;
            uint32 spellId = ts->spell;
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            {
                if (ts->learnedSpell[i])
                {
                    spellId = ts->learnedSpell[i];
                    break;
                }
            }
            TryLearnSpell(bot, spellId);
        }
    }

    // Teach class/spec spells the core GetSpellsForLevels path often misses.
    void LearnLevelClassSpells(Player* bot, uint32 specId)
    {
        if (!bot)
            return;

        uint8 const level = bot->getLevel();
        uint32 const raceMask = bot->getRaceMask();
        uint32 const classSkill = ClassSkillId(bot->getClass());

        // 1) Stock helper (class skill + specialization when mapping works).
        std::list<uint32> const stock = GetSpellsForLevels(
            bot->getClass(), raceMask, specId, 0, level);
        for (uint32 spellId : stock)
            TryLearnSpell(bot, spellId);

        // 2) Direct scan of THIS class's skill line only (e.g. skill 804 = Priest).
        if (classSkill)
        {
            for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
            {
                SkillLineAbilityEntry const* ability = sSkillLineAbilityStore.LookupEntry(i);
                if (!ability || !ability->spellId)
                    continue;
                if (ability->skillId != classSkill)
                    continue;
                if (ability->learnOnGetSkill != ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL)
                    continue;
                if (ability->racemask && !(ability->racemask & raceMask))
                    continue;
                if (!ability->classmask || !(ability->classmask & bot->getClassMask()))
                    continue;

                TryLearnSpell(bot, ability->spellId);
            }
        }

        // 3) Specialization spells (Mind Flay, Shadowform, etc.) by level.
        if (specId)
        {
            if (std::vector<uint32> const* specSpells = GetSpecializationSpells(specId))
            {
                for (uint32 spellId : *specSpells)
                    TryLearnSpell(bot, spellId);
            }
        }

        // 4) Race+class create spells (Smite, racial languages for this combo).
        bot->learnDefaultSpells();

        // 5) Critical baseline casts that create/trainer paths still miss on some
        // races (e.g. Smite 585) — without these, shadow bots only land SW:P.
        if (bot->getClass() == CLASS_PRIEST)
        {
            TryLearnSpell(bot, 585);   // Smite
            TryLearnSpell(bot, 589);   // Shadow Word: Pain
            TryLearnSpell(bot, 588);   // Inner Fire
            TryLearnSpell(bot, 8092);  // Mind Blast
            if (specId == SPEC_PRIEST_SHADOW)
                TryLearnSpell(bot, 15407); // Mind Flay
        }
        if (bot->getClass() == CLASS_HUNTER)
        {
            TryLearnSpell(bot, 75);    // Auto Shot
            TryLearnSpell(bot, 56641); // Steady Shot
            TryLearnSpell(bot, 3044);  // Arcane Shot
            TryLearnSpell(bot, 1978);  // Serpent Sting
            TryLearnSpell(bot, 13165); // Aspect of the Hawk
        }
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

    // Equips a strong item matching the WHERE clause that the bot can actually
    // use. Among the top ilvl candidates, pick randomly so bots do not all wear
    // the identical BiS-looking template. Prefers primary-stat items, then any.
    // The exclude entry keeps paired slots (rings/trinkets) distinct.
    uint32 EquipBestForSlot(Player* bot, std::string const& where, uint32 primaryStat, uint32 exclude,
        int minQuality, int maxQuality)
    {
        uint32 const passStats[2] = { primaryStat, 0 };
        int const passes = primaryStat ? 2 : 1;
        constexpr size_t kRandomPool = 8;

        for (int p = 0; p < passes; ++p)
        {
            std::vector<uint32> entries = QueryItemEntries(bot, where, passStats[p], exclude,
                minQuality, maxQuality);
            if (entries.empty())
                continue;

            size_t const pool = std::min(entries.size(), kRandomPool);
            for (size_t i = 0; i + 1 < pool; ++i)
            {
                size_t const j = i + size_t(std::rand() % int(pool - i));
                std::swap(entries[i], entries[j]);
            }

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
    void GearWeapons(Player* bot, BotRole role, uint32 specId, int minQuality, int maxQuality)
    {
        uint8 const cls = bot->getClass();
        uint32 const primary = PrimaryStatForClassRole(cls, role, specId);

        auto equip = [&](std::string const& where) -> bool
        {
            return EquipBestForSlot(bot, where, primary, 0, minQuality, maxQuality) != 0;
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
                    uint32 const mh = EquipBestForSlot(bot, dagger, primary, 0, minQuality, maxQuality);
                    uint32 oh = 0;
                    if (mh)
                        oh = EquipBestForSlot(bot, dagger, primary, mh, minQuality, maxQuality);

                    // Last resort: any 1H if the dagger pool is empty, but still
                    // force Dual Wield so an off-hand slot can fill.
                    if (!mh)
                        EquipBestForSlot(bot, oneH + " AND subclass IN (0,4,7,13,15)", primary, 0,
                            minQuality, maxQuality);
                    if (!oh)
                        EquipBestForSlot(bot, oneH + " AND subclass IN (0,4,7,13,15)", primary,
                            bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND)
                                ? bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND)->GetEntry()
                                : 0,
                            minQuality, maxQuality);
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

    // Fills every gear slot with level/class/spec-appropriate items.
    void GearBot(Player* bot, BotRole role, uint32 specId, int minQuality, int maxQuality)
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
            return EquipBestForSlot(bot, where, primary, exclude, minQuality, maxQuality);
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

        GearWeapons(bot, role, specId, minQuality, maxQuality);
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
    std::vector<PendingBotInit> pending;

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

        charsCreated += PopulateAccount(accountId, &pending);
    }

    // One flush for the whole create batch (was Wait() per character).
    if (charsCreated)
        CharacterDatabase.Wait();

    uint32 inited = 0;
    if (_autoInitOnCreate && !pending.empty())
    {
        SF_LOG_INFO("modules",
            "[mod-playerbots] Auto-init on create: gearing %u new character(s)...",
            uint32(pending.size()));
        for (PendingBotInit const& p : pending)
        {
            InitCreatedBot(p);
            ++inited;
            if ((inited % 25u) == 0u)
                SF_LOG_INFO("modules", "[mod-playerbots] Auto-init progress: %u / %u.",
                    inited, uint32(pending.size()));
        }
    }

    // Newly created characters are candidates; refresh the pool on the next tick.
    _candidatesLoaded = false;

    SF_LOG_INFO("modules",
        "[mod-playerbots] Auto-create complete: %u new account(s), %u new character(s)%s.",
        accountsCreated, charsCreated,
        _autoInitOnCreate ? "" : " (init deferred to first login)");
    if (_autoInitOnCreate && inited)
        SF_LOG_INFO("modules", "[mod-playerbots] Auto-init finished: %u character(s).", inited);

    if (report)
    {
        std::ostringstream ss;
        ss << "Auto-create complete: " << accountsCreated << " new account(s), "
           << charsCreated << " new character(s)";
        if (_autoInitOnCreate)
            ss << ", " << inited << " inited";
        else
            ss << " (gear on first login)";
        ss << ".";
        *report = ss.str();
    }

    return charsCreated;
}

void PlayerbotMgr::InitializeBot(Player* bot, int roleOverride, uint32 specOverride,
    int maxItemQuality, bool relevel, bool teleport, bool teleportFromCommand)
{
    if (!bot || !bot->IsInWorld())
        return;

    if (relevel)
    {
        uint32 const maxPlayerLevel = sWorld->getIntConfig(WorldIntConfigs::CONFIG_MAX_PLAYER_LEVEL);
        uint32 minL = std::min(_autoMinLevel, maxPlayerLevel);
        uint32 maxL = std::min(_autoMaxLevel, maxPlayerLevel);
        if (maxL < minL)
            std::swap(minL, maxL);
        uint8 const level = (maxL > minL)
            ? uint8(minL + uint32(std::rand() % int(maxL - minL + 1)))
            : uint8(minL);
        if (bot->getLevel() != level)
        {
            SF_LOG_INFO("modules",
                "[mod-playerbots]   '%s': relevelling %u -> %u (conf %u-%u)...",
                bot->GetName().c_str(), bot->getLevel(), uint32(level), minL, maxL);
            bot->GiveLevel(level);
            bot->SetUInt32Value(PLAYER_FIELD_XP, 0);
        }
    }

    SF_LOG_INFO("modules", "[mod-playerbots] Initializing bot '%s' (level %u %s %s)%s%s...",
        bot->GetName().c_str(), bot->getLevel(),
        SafeRaceName(bot->getRace()), SafeClassName(bot->getClass()),
        relevel ? " [relevel]" : "",
        teleport ? (teleportFromCommand ? " [tele]" : " [tele-auto]") : "");

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
            bot->LearnSpecialization(specId);

        BotRotation::ApplyRecommendedTalents(bot);
        BotRotation::ApplyRecommendedGlyphs(bot);
    }
    else if (roleOverride >= 0)
    {
        role = static_cast<BotRole>(roleOverride);
    }

    // Armor/weapon skills for this level, then trainer spells, riding/mounts,
    // then gear. Re-running init after a delevel simply skips plate/mail and
    // higher riding tiers the bot is no longer high enough for.
    LearnArmorProficiencies(bot);
    LearnClassTrainerSpells(bot);
    LearnLevelClassSpells(bot, specId);
    LearnRidingAndMounts(bot);

    // Gear the bot for the (possibly newly assigned) role/spec at its level.
    int const qualityCap = maxItemQuality < 0
        ? _gearMaxQuality
        : std::max(0, std::min(maxItemQuality, int(ITEM_QUALITY_LEGENDARY)));
    int qualityFloor = _gearMinQuality;
    if (maxItemQuality >= 0)
        qualityFloor = std::min(qualityFloor, qualityCap);
    if (qualityFloor > qualityCap)
        qualityFloor = qualityCap;

    GearBot(bot, role, specId, qualityFloor, qualityCap);
    GiveRestConsumables(bot);

    // After relevel + gear so the destination matches the bot's final level.
    // Levels 1–5: auto skips; player `tele` resets to homebind/spawn.
    if (teleport)
        BotTeleportMaps::TeleportForLevel(bot, teleportFromCommand);

    bot->SaveToDB();

    if (auto it = _ai.find(bot->GetGUID()); it != _ai.end() && it->second)
    {
        it->second->ResetStrategiesToRoleDefaults();
        it->second->AfterInitRelocate(teleport);
    }

    SF_LOG_INFO("modules",
        "[mod-playerbots] Initialized bot '%s': level %u %s %s, spec %s (%s), gear %s-%s%s.",
        bot->GetName().c_str(), bot->getLevel(),
        SafeRaceName(bot->getRace()), SafeClassName(bot->getClass()),
        SpecName(specId), RoleName(role), QualityName(qualityFloor), QualityName(qualityCap),
        teleport ? ", teleported" : "");
}

uint32 PlayerbotMgr::EnqueueInitializeAllBots(int roleOverride, uint32 specOverride,
    int maxItemQuality, bool relevel, bool teleport, uint64 notifyPlayerGuid)
{
    uint32 added = 0;
    for (auto const& pair : _bots)
    {
        WorldSession* session = pair.second;
        Player* bot = session ? session->GetPlayer() : nullptr;
        if (!bot || !bot->IsInWorld())
            continue;
        if (specOverride)
        {
            ChrSpecializationEntry const* entry = sChrSpecializationStore.LookupEntry(specOverride);
            if (!entry || entry->classId != bot->getClass())
                continue;
        }
        // Queued from `.playerbots init` — tele token is an explicit command.
        UpsertInitQueueJob(bot->GetGUID(), roleOverride, specOverride, maxItemQuality,
            relevel, teleport, teleport);
        ++added;
    }

    if (added)
    {
        if (!_initQueueBatchTotal)
            _initQueueBatchDone = 0;
        _initQueueBatchTotal = uint32(_initQueue.size());
        if (notifyPlayerGuid)
            _initQueueNotifyGuid = notifyPlayerGuid;
        SF_LOG_INFO("modules",
            "[mod-playerbots] Init queue: %u bot(s) queued (%u/tick).",
            added, _initPerTick);
    }
    return added;
}

bool PlayerbotMgr::EnqueueInitializeBot(uint64 characterGuid, int roleOverride,
    uint32 specOverride, int maxItemQuality, bool relevel, bool teleport,
    uint64 notifyPlayerGuid)
{
    Player* bot = ObjectAccessor::FindPlayer(characterGuid);
    if (!bot || !bot->IsInWorld())
        return false;
    if (!IsBot(characterGuid) && !IsSelfBot(characterGuid))
        return false;
    if (specOverride)
    {
        ChrSpecializationEntry const* entry = sChrSpecializationStore.LookupEntry(specOverride);
        if (!entry || entry->classId != bot->getClass())
            return false;
    }

    UpsertInitQueueJob(characterGuid, roleOverride, specOverride, maxItemQuality,
        relevel, teleport, teleport);
    if (!_initQueueBatchTotal)
        _initQueueBatchDone = 0;
    _initQueueBatchTotal = uint32(_initQueue.size());
    if (notifyPlayerGuid)
        _initQueueNotifyGuid = notifyPlayerGuid;
    return true;
}

void PlayerbotMgr::UpsertInitQueueJob(uint64 guid, int roleOverride, uint32 specOverride,
    int maxItemQuality, bool relevel, bool teleport, bool teleportFromCommand)
{
    for (QueuedBotInit& job : _initQueue)
    {
        if (job.guid != guid)
            continue;
        job.roleOverride = roleOverride;
        job.specOverride = specOverride;
        job.maxItemQuality = maxItemQuality;
        job.relevel = relevel;
        job.teleport = teleport;
        job.teleportFromCommand = teleportFromCommand;
        return;
    }

    QueuedBotInit job;
    job.guid = guid;
    job.roleOverride = roleOverride;
    job.specOverride = specOverride;
    job.maxItemQuality = maxItemQuality;
    job.relevel = relevel;
    job.teleport = teleport;
    job.teleportFromCommand = teleportFromCommand;
    _initQueue.push_back(job);
}

void PlayerbotMgr::ProcessInitQueue()
{
    if (_initQueue.empty())
        return;

    uint32 processed = 0;
    while (!_initQueue.empty() && processed < _initPerTick)
    {
        QueuedBotInit job = _initQueue.front();
        _initQueue.pop_front();
        ++processed;
        ++_initQueueBatchDone;

        Player* bot = ObjectAccessor::FindPlayer(job.guid);
        if (!bot || !bot->IsInWorld())
            continue;
        if (!IsBot(job.guid) && !IsSelfBot(job.guid))
            continue;
        if (job.specOverride)
        {
            ChrSpecializationEntry const* entry = sChrSpecializationStore.LookupEntry(job.specOverride);
            if (!entry || entry->classId != bot->getClass())
                continue;
        }

        InitializeBot(bot, job.roleOverride, job.specOverride, job.maxItemQuality,
            job.relevel, job.teleport, job.teleportFromCommand);
    }

    if ((_initQueueBatchDone % 25u) == 0u || _initQueue.empty())
        SF_LOG_INFO("modules", "[mod-playerbots] Init queue progress: %u done, %u remaining.",
            _initQueueBatchDone, uint32(_initQueue.size()));

    if (!_initQueue.empty())
        return;

    uint32 const done = _initQueueBatchDone;
    uint64 const notify = _initQueueNotifyGuid;
    _initQueueBatchTotal = 0;
    _initQueueBatchDone = 0;
    _initQueueNotifyGuid = 0;

    SF_LOG_INFO("modules", "[mod-playerbots] Init queue finished (%u bot(s)).", done);
    if (notify)
    {
        if (Player* player = ObjectAccessor::FindPlayer(notify))
            if (WorldSession* session = player->GetSession())
                ChatHandler(session).PSendSysMessage(
                    "Playerbots init queue finished (%u bot(s)).", done);
    }
}

uint32 PlayerbotMgr::PopulateAccount(uint32 accountId, std::vector<PendingBotInit>* pending)
{
    uint32 existing = AccountMgr::GetCharactersCount(accountId);
    uint32 created = 0;

    for (uint32 n = existing; n < _autoCharsPerAccount; ++n)
        if (CreateOneCharacter(accountId, pending))
            ++created;

    return created;
}

bool PlayerbotMgr::BotNeedsGearInit(Player* bot) const
{
    if (!bot)
        return false;
    // Ungeared after deferred create — chest and main-hand both empty.
    return !bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_CHEST)
        && !bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
}

void PlayerbotMgr::InitCreatedBot(PendingBotInit const& pending)
{
    std::string spawnError;
    if (!SpawnBot(pending.guid, false, &spawnError, pending.accountId))
    {
        SF_LOG_ERROR("modules", "[mod-playerbots] Created GUID %u but could not auto-init: %s",
            GUID_LOPART(pending.guid), spawnError.c_str());
        return;
    }
    if (auto it = _bots.find(pending.guid); it != _bots.end())
    {
        if (Player* bot = it->second ? it->second->GetPlayer() : nullptr)
            InitializeBot(bot, pending.role, pending.specId, -1, false, _autoTeleportOnInit);
    }
    RemoveBot(pending.guid);
}

bool PlayerbotMgr::CreateOneCharacter(uint32 accountId, std::vector<PendingBotInit>* pending)
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
    uint32 minL = std::min(_autoMinLevel, maxLevel);
    uint32 maxL = std::min(_autoMaxLevel, maxLevel);
    if (maxL < minL)
        std::swap(minL, maxL);
    uint8 level = uint8(minL);
    if (maxL > minL)
        level = uint8(minL + uint32(std::rand() % int(maxL - minL + 1)));

    WorldSession* sess = new WorldSession(accountId, nullptr, AccountTypes::SEC_PLAYER, expansion,
        0, LOCALE_enUS, 0, false, false);
    sess->SetBot(true);
    // Match real characters (global realmID is set at world startup from RealmID
    // conf or auth.realmlist). Never persist the unset sentinel 0xFFFFFFFF.
    uint32 botRealm = realmID;
    if (!botRealm || botRealm == uint32(-1))
        botRealm = 1;
    sess->SetVirtualRealmID(botRealm);

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

    if (pending)
    {
        PendingBotInit p;
        p.guid = MAKE_NEW_GUID(guid, 0, HIGHGUID_PLAYER);
        p.accountId = accountId;
        p.role = int(role);
        p.specId = specId;
        pending->push_back(p);
    }

    return true;
}
