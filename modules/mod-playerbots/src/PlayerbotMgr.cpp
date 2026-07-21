/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "PlayerbotMgr.h"
#include "PlayerbotAI.h"
#include "AccountMgr.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <sstream>
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

    _autoCreateOnStartup = sConfigMgr->GetBoolDefault("Playerbots.AutoCreate.OnStartup", false);
    _autoAccountCount = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.AccountCount", 0));
    _autoPassword = sConfigMgr->GetStringDefault("Playerbots.AutoCreate.AccountPassword", "password");
    _autoCharsPerAccount = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.CharactersPerAccount", 1));
    _autoAlliancePct = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.AlliancePct", 50));
    _autoTankPct = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.TankPct", 20));
    _autoHealerPct = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.HealerPct", 20));
    _autoLevel = uint32(sConfigMgr->GetIntDefault("Playerbots.AutoCreate.Level", 1));

    // Clamp to sane ranges so bad config can't create invalid characters.
    if (_autoAlliancePct > 100) _autoAlliancePct = 100;
    if (_autoTankPct > 100) _autoTankPct = 100;
    if (_autoHealerPct > 100) _autoHealerPct = 100;
    if (_autoTankPct + _autoHealerPct > 100) _autoHealerPct = 100 - _autoTankPct;
    if (_autoCharsPerAccount > 11) _autoCharsPerAccount = 11;   // realm cap
    if (_autoLevel < 1) _autoLevel = 1;

    // Force a candidate reload so prefix changes take effect on .reload config.
    _candidatesLoaded = false;

    if (_enabled)
        SF_LOG_INFO("modules", "[mod-playerbots] Enabled (random bots: %s, max: %u, account prefix: '%s').",
            _randomBotsEnabled ? "on" : "off", _maxRandomBots, _accountPrefix.c_str());
    else
        SF_LOG_INFO("modules", "[mod-playerbots] Disabled via configuration.");
}

void PlayerbotMgr::Update(uint32 diff)
{
    if (!_enabled)
        return;

    // Drop bots whose player has left the world for any reason.
    CleanupDeadBots();

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
            SF_LOG_INFO("modules", "[mod-playerbots] Random bot logged in (GUID %u). Random bots: %u/%u.",
                GUID_LOPART(guid), uint32(_randomBots.size()), _maxRandomBots);
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
        _ai[characterGuid] = new PlayerbotAI(bot);

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

    uint32 guid = sess->CreateBotCharacter(name, race, cls, gender, 0, 0, 0, 0, 0, level);

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
