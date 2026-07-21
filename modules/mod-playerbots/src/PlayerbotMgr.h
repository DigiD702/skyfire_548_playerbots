/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*
* Playerbots module - central configuration/state manager.
*
* Phase 1: socketless bot WorldSessions + synchronous login.
* Phase 2: random-bot pool sourced from dedicated bot accounts, managed by a
*          throttled update loop (bot AI still arrives in Phase 3, see PORTING.md).
*/

#ifndef _SF_PLAYERBOT_MGR_H
#define _SF_PLAYERBOT_MGR_H

#include "Define.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class WorldSession;
class Player;
class PlayerbotAI;

class PlayerbotMgr
{
public:
    static PlayerbotMgr* instance();

    // (Re)reads module settings from the worldserver configuration.
    void LoadConfig();

    // Called every world tick (from the module's WorldScript::OnUpdate).
    void Update(uint32 diff);

    // Ticks a single bot's AI (from PlayerScript::OnUpdate).
    void UpdateBotAI(Player* bot, uint32 diff);

    bool IsEnabled() const { return _enabled; }
    bool IsRandomBotsEnabled() const { return _randomBotsEnabled; }
    uint32 GetMaxRandomBots() const { return _maxRandomBots; }

    // Logs the given character into the world as a manually-added bot.
    bool AddBot(uint64 characterGuid, std::string* errorOut = nullptr);
    // Logs the given bot character out of the world and frees its session.
    bool RemoveBot(uint64 characterGuid);
    bool IsBot(uint64 characterGuid) const { return _bots.find(characterGuid) != _bots.end(); }

    // Logs out and frees every managed bot (used on world shutdown).
    void LogoutAllBots();

    // Forces the candidate bot-character pool to be reloaded on the next tick.
    void ReloadCandidates() { _candidatesLoaded = false; }

    bool IsAutoCreateOnStartup() const { return _autoCreateOnStartup; }

    // Provisions bot accounts (prefix + n) and fills them with characters using
    // the configured faction/role ratios and start level. Incremental: existing
    // accounts and characters are reused. Returns the number of characters
    // created; a human-readable summary is appended to *report when provided.
    uint32 CreateBotPopulation(std::string* report = nullptr);

    uint32 GetActiveBotCount() const { return uint32(_bots.size()); }
    uint32 GetRandomBotCount() const { return uint32(_randomBots.size()); }
    uint32 GetCandidateCount() const { return uint32(_candidates.size()); }
    void GetBotGuids(std::vector<uint64>& out) const;

private:
    PlayerbotMgr() = default;
    ~PlayerbotMgr() = default;
    PlayerbotMgr(PlayerbotMgr const&) = delete;
    PlayerbotMgr& operator=(PlayerbotMgr const&) = delete;

    // Shared bot spawn path. isRandom marks the bot as pool-managed.
    bool SpawnBot(uint64 characterGuid, bool isRandom, std::string* errorOut);
    void LoadCandidates();
    void TrySpawnRandomBot();
    void CleanupDeadBots();
    void DestroyBotAI(uint64 characterGuid);

    // Auto-creation helpers.
    uint32 PopulateAccount(uint32 accountId);               // create missing characters on one account
    bool CreateOneCharacter(uint32 accountId);              // roll & create a single character

    bool _enabled = false;
    bool _randomBotsEnabled = false;
    uint32 _maxRandomBots = 0;
    std::string _accountPrefix = "RNDBOT";
    uint32 _loginIntervalMs = 2000;

    // Auto-creation settings.
    bool _autoCreateOnStartup = false;
    uint32 _autoAccountCount = 0;
    std::string _autoPassword = "password";
    uint32 _autoCharsPerAccount = 1;
    uint32 _autoAlliancePct = 50;
    uint32 _autoTankPct = 20;
    uint32 _autoHealerPct = 20;
    uint32 _autoLevel = 1;

    uint32 _loginTimer = 0;
    bool _candidatesLoaded = false;
    std::vector<uint64> _candidates;                        // eligible character GUIDs

    std::unordered_map<uint64 /*characterGuid*/, WorldSession*> _bots;
    std::unordered_map<uint64 /*characterGuid*/, PlayerbotAI*> _ai;
    std::unordered_set<uint64 /*characterGuid*/> _randomBots; // subset managed by the pool
};

#define sPlayerbotMgr PlayerbotMgr::instance()

#endif // _SF_PLAYERBOT_MGR_H
