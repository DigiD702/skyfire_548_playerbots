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
class Group;

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

    // Routes a chat order to one bot (whisper) or every bot in a group (party/raid).
    void HandleBotWhisper(Player* from, Player* bot, std::string const& msg);
    void HandleBotGroupChat(Player* from, Group* group, std::string const& msg);

    bool IsEnabled() const { return _enabled; }
    bool IsRandomBotsEnabled() const { return _randomBotsEnabled; }
    uint32 GetMaxRandomBots() const { return _maxRandomBots; }

    // Logs the given character into the world as a manually-added bot.
    bool AddBot(uint64 characterGuid, std::string* errorOut = nullptr);
    // Logs the given bot character out of the world and frees its session.
    bool RemoveBot(uint64 characterGuid);
    bool IsBot(uint64 characterGuid) const { return _bots.find(characterGuid) != _bots.end(); }
    // True if this GUID has an attached PlayerbotAI (socket bot or self-bot).
    bool HasBotAI(uint64 characterGuid) const { return _ai.find(characterGuid) != _ai.end(); }
    bool IsSelfBot(uint64 characterGuid) const { return _selfBots.find(characterGuid) != _selfBots.end(); }

    // Attach/detach AI to a real logged-in player (client keeps movement).
    bool AttachSelfBot(Player* player, std::string* errorOut = nullptr);
    bool DetachSelfBot(Player* player);
    bool ToggleSelfBot(Player* player, std::string* report = nullptr);

    // Logs out and frees every managed bot (used on world shutdown).
    void LogoutAllBots();

    // Forces the candidate bot-character pool to be reloaded on the next tick.
    void ReloadCandidates() { _candidatesLoaded = false; }

    bool IsAutoCreateOnStartup() const { return _autoCreateOnStartup; }

    float GetRestHealthPct() const { return _restHealthPct; }
    float GetRestManaPct() const { return _restManaPct; }
    float GetSaveManaThreshold() const { return _saveManaThreshold; }
    uint32 GetWaitForAttackSeconds() const { return _waitForAttackSeconds; }

    // Provisions bot accounts (prefix + n) and fills them with characters using
    // the configured faction/role ratios and start level. Incremental: existing
    // accounts and characters are reused. Returns the number of characters
    // created; a human-readable summary is appended to *report when provided.
    uint32 CreateBotPopulation(std::string* report = nullptr);

    // Re-applies derived state (specialization/spells, gear) to a single active
    // bot. Safe to call repeatedly.
    // roleOverride: -1 keep current role mapping, otherwise 0 = tank, 1 = healer,
    // 2 = damage (default DPS tab for the class).
    // specOverride: if non-zero, force that ChrSpecialization id (takes priority).
    // maxItemQuality: ITEM_QUALITY_* cap for gear rolls (default epic).
    void InitializeBot(Player* bot, int roleOverride = -1, uint32 specOverride = 0,
        int maxItemQuality = 4);
    // Initializes every active bot; returns the number processed.
    uint32 InitializeAllBots(int roleOverride = -1, uint32 specOverride = 0,
        int maxItemQuality = 4);

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
    // When a real player is solo-queued for LFG, queue level-matched bots too.
    void UpdateLfgAutoJoin(uint32 diff);

    // Auto-creation helpers.
    uint32 PopulateAccount(uint32 accountId);               // create missing characters on one account
    bool CreateOneCharacter(uint32 accountId);              // roll & create a single character

    bool _enabled = false;
    bool _randomBotsEnabled = false;
    uint32 _maxRandomBots = 0;
    std::string _accountPrefix = "RNDBOT";
    uint32 _loginIntervalMs = 2000;

    // Auto-join LFG when a real player queues (AC-style fill).
    bool _joinLfg = false;
    uint32 _joinLfgMaxBots = 10;
    uint32 _joinLfgLevelRange = 5;   // |botLevel - playerLevel| must be <= this
    uint32 _lfgJoinTimer = 0;

    // Auto-creation settings.
    bool _autoCreateOnStartup = false;
    uint32 _autoAccountCount = 0;
    std::string _autoPassword = "password";
    uint32 _autoCharsPerAccount = 1;
    uint32 _autoAlliancePct = 50;
    uint32 _autoTankPct = 20;
    uint32 _autoHealerPct = 20;
    uint32 _autoLevel = 1;

    // Rest / save-mana numeric thresholds only (enable/disable is co/nc runtime).
    float _restHealthPct = 50.0f;
    float _restManaPct = 50.0f;
    float _saveManaThreshold = 60.0f;
    uint32 _waitForAttackSeconds = 5;

    uint32 _loginTimer = 0;
    bool _candidatesLoaded = false;
    std::vector<uint64> _candidates;                        // eligible character GUIDs

    std::unordered_map<uint64 /*characterGuid*/, WorldSession*> _bots;
    std::unordered_map<uint64 /*characterGuid*/, PlayerbotAI*> _ai;
    std::unordered_set<uint64 /*characterGuid*/> _randomBots; // subset managed by the pool
    std::unordered_set<uint64 /*characterGuid*/> _selfBots;   // real players with cast-only AI
};

#define sPlayerbotMgr PlayerbotMgr::instance()

#endif // _SF_PLAYERBOT_MGR_H
