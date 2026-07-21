/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "Containers.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Group.h"
#include "GroupMgr.h"
#include "LFGMgr.h"
#include "LFGQueue.h"
#include "Log.h"
#include "ObjectDefines.h"
#include "World.h"

#include <algorithm>
#include <map>

namespace lfg
{
    namespace
    {
        uint8 const LFG_COMBAT_ROLE_MASK = PLAYER_ROLE_TANK | PLAYER_ROLE_HEALER | PLAYER_ROLE_DAMAGE;

        bool QueueContainsGuid(LfgGuidList const& queue, uint64 guid)
        {
            return std::find(queue.begin(), queue.end(), guid) != queue.end();
        }

        bool CompatibleKeyContainsGuid(std::string const& key, uint64 guid)
        {
            std::ostringstream out;
            out << guid;
            std::string guidString = out.str();

            std::string::size_type tokenStart = 0;
            while (tokenStart <= key.length())
            {
                std::string::size_type tokenEnd = key.find('|', tokenStart);
                std::string::size_type tokenLength = tokenEnd == std::string::npos ? std::string::npos : tokenEnd - tokenStart;
                if (key.compare(tokenStart, tokenLength, guidString) == 0)
                    return true;

                if (tokenEnd == std::string::npos)
                    break;

                tokenStart = tokenEnd + 1;
            }

            return false;
        }

        void ConsumeRoleSlot(uint8 role, uint8& tanks, uint8& healers, uint8& dps)
        {
            role &= ~PLAYER_ROLE_LEADER;

            if ((role & PLAYER_ROLE_TANK) && tanks)
                --tanks;
            else if ((role & PLAYER_ROLE_HEALER) && healers)
                --healers;
            else if ((role & PLAYER_ROLE_DAMAGE) && dps)
                --dps;
        }

        bool IsScenarioDifficulty(uint32 difficulty)
        {
            return difficulty == DIFFICULTY_SCE_NORMAL || difficulty == DIFFICULTY_SCE_HEROIC;
        }

        bool IsScenarioDungeon(uint32 dungeonId)
        {
            LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(dungeonId);
            if (!dungeon)
                return false;

            if (IsScenarioDifficulty(dungeon->m_DifficultyID))
                return true;

            MapEntry const* map = sMapStore.LookupEntry(dungeon->m_ContinentID);
            return map && map->IsScenario();
        }

        bool IsRaidFinderDungeon(uint32 dungeonId)
        {
            return sLFGMgr->IsRaidFinderDungeon(dungeonId);
        }

        bool HasRaidFinderDungeon(LfgDungeonSet const& dungeons)
        {
            for (LfgDungeonSet::const_iterator it = dungeons.begin(); it != dungeons.end(); ++it)
                if (IsRaidFinderDungeon(*it))
                    return true;

            return false;
        }

        uint8 GetDungeonGroupSize(uint32 dungeonId)
        {
            if (IsRaidFinderDungeon(dungeonId))
                return LFG_RF_GROUP_SIZE;

            LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(dungeonId);
            if (!dungeon || !IsScenarioDungeon(dungeonId))
                return MAXGROUPSIZE;

            if (MapDifficulty const* difficulty = GetMapDifficultyData(dungeon->m_ContinentID, DifficultyID(dungeon->m_DifficultyID)))
                if (difficulty->maxPlayers)
                    return uint8(std::min<uint32>(difficulty->maxPlayers, MAXGROUPSIZE));

            return 3;
        }

        bool HasScenarioDungeon(LfgDungeonSet const& dungeons)
        {
            for (LfgDungeonSet::const_iterator it = dungeons.begin(); it != dungeons.end(); ++it)
                if (IsScenarioDungeon(*it))
                    return true;

            return false;
        }

        uint8 GetQueueGroupSize(LfgDungeonSet const& dungeons)
        {
            uint8 groupSize = MAXGROUPSIZE;
            for (LfgDungeonSet::const_iterator it = dungeons.begin(); it != dungeons.end(); ++it)
                groupSize = std::min<uint8>(groupSize, GetDungeonGroupSize(*it));

            return groupSize ? groupSize : MAXGROUPSIZE;
        }

        bool CheckQueueRoles(LfgRolesMap& roles, LfgDungeonSet const& dungeons)
        {
            if (HasRaidFinderDungeon(dungeons))
                return LFGMgr::CheckRaidFinderRoles(roles, LFG_RF_TANKS_NEEDED, LFG_RF_HEALERS_NEEDED, LFG_RF_DPS_NEEDED);

            if (HasScenarioDungeon(dungeons))
                return LFGMgr::CheckDpsOnlyRoles(roles, GetQueueGroupSize(dungeons));

            return LFGMgr::CheckGroupRoles(roles);
        }

        void CalculateRoleShortage(LfgRolesMap const& roles, LfgDungeonSet const& dungeons, uint8& tanks, uint8& healers, uint8& dps)
        {
            if (HasRaidFinderDungeon(dungeons))
            {
                tanks = LFG_RF_TANKS_NEEDED;
                healers = LFG_RF_HEALERS_NEEDED;
                dps = LFG_RF_DPS_NEEDED;

                LfgRolesMap assignedRoles = roles;
                if (LFGMgr::CheckRaidFinderRoles(assignedRoles, tanks, healers, dps))
                    for (LfgRolesMap::const_iterator it = assignedRoles.begin(); it != assignedRoles.end(); ++it)
                        ConsumeRoleSlot(it->second, tanks, healers, dps);
                else
                    for (LfgRolesMap::const_iterator it = roles.begin(); it != roles.end(); ++it)
                        ConsumeRoleSlot(it->second, tanks, healers, dps);
                return;
            }

            if (HasScenarioDungeon(dungeons))
            {
                uint8 groupSize = GetQueueGroupSize(dungeons);
                tanks = 0;
                healers = 0;
                dps = roles.size() >= groupSize ? 0 : uint8(groupSize - roles.size());
                return;
            }

            tanks = LFG_TANKS_NEEDED;
            healers = LFG_HEALERS_NEEDED;
            dps = LFG_DPS_NEEDED;

            LfgRolesMap assignedRoles = roles;
            if (LFGMgr::CheckGroupRoles(assignedRoles))
                for (LfgRolesMap::const_iterator it = assignedRoles.begin(); it != assignedRoles.end(); ++it)
                    ConsumeRoleSlot(it->second, tanks, healers, dps);
            else
                for (LfgRolesMap::const_iterator it = roles.begin(); it != roles.end(); ++it)
                    ConsumeRoleSlot(it->second, tanks, healers, dps);
        }

        int32 SelectWaitTime(uint8 tanks, uint8 healers, uint8 dps, int32 wtTank, int32 wtHealer, int32 wtDps, int32 wtAvg)
        {
            if (tanks)
                return wtTank;

            if (healers)
                return wtHealer;

            if (dps)
                return wtDps;

            return wtAvg;
        }
    }

    /**
       Given a list of guids returns the concatenation using | as delimiter

       @param[in]     check list of guids
       @returns Concatenated string
    */
    std::string ConcatenateGuids(LfgGuidList const& check)
    {
        if (check.empty())
            return "";

        // need the guids in order to avoid duplicates
        LfgGuidSet guids(check.begin(), check.end());

        std::ostringstream o;

        LfgGuidSet::const_iterator it = guids.begin();
        o << (*it);
        for (++it; it != guids.end(); ++it)
            o << '|' << (*it);

        return o.str();
    }

    char const* GetCompatibleString(LfgCompatibility compatibles)
    {
        switch (compatibles)
        {
            case LFG_COMPATIBILITY_PENDING:
                return "Pending";
            case LFG_COMPATIBLES_BAD_STATES:
                return "Compatibles (Bad States)";
            case LFG_COMPATIBLES_MATCH:
                return "Match";
            case LFG_COMPATIBLES_WITH_LESS_PLAYERS:
                return "Compatibles (Not enough players)";
            case LFG_INCOMPATIBLES_HAS_IGNORES:
                return "Has ignores";
            case LFG_INCOMPATIBLES_MULTIPLE_LFG_GROUPS:
                return "Multiple Lfg Groups";
            case LFG_INCOMPATIBLES_NO_DUNGEONS:
                return "Incompatible dungeons";
            case LFG_INCOMPATIBLES_NO_ROLES:
                return "Incompatible roles";
            case LFG_INCOMPATIBLES_TOO_MUCH_PLAYERS:
                return "Too much players";
            case LFG_INCOMPATIBLES_WRONG_GROUP_SIZE:
                return "Wrong group size";
            default:
                return "Unknown";
        }
    }

    void LFGQueue::AddToQueue(uint64 guid, bool reQueue)
    {
        LfgQueueDataContainer::iterator itQueue = QueueDataStore.find(guid);
        if (itQueue == QueueDataStore.end())
        {
            SF_LOG_ERROR("lfg.queue.add", "Queue data not found for [%u]", GUID_LOPART(guid));
            return;
        }

        if (reQueue)
            AddToCurrentQueue(guid);
        else
            AddToNewQueue(guid);
    }

    void LFGQueue::RemoveFromQueue(uint64 guid)
    {
        RemoveFromNewQueue(guid);
        RemoveFromCurrentQueue(guid);
        RemoveFromCompatibles(guid);

        LfgQueueDataContainer::iterator itDelete = QueueDataStore.end();
        for (LfgQueueDataContainer::iterator itr = QueueDataStore.begin(); itr != QueueDataStore.end(); ++itr)
            if (itr->first != guid)
            {
                if (CompatibleKeyContainsGuid(itr->second.bestCompatible, guid))
                {
                    itr->second.bestCompatible.clear();
                    FindBestCompatibleInQueue(itr);
                }
            }
            else
                itDelete = itr;

        if (itDelete != QueueDataStore.end())
            QueueDataStore.erase(itDelete);
    }

    void LFGQueue::AddToNewQueue(uint64 guid)
    {
        if (!QueueContainsGuid(newToQueueStore, guid))
            newToQueueStore.push_back(guid);
    }

    void LFGQueue::RemoveFromNewQueue(uint64 guid)
    {
        newToQueueStore.remove(guid);
    }

    void LFGQueue::AddToCurrentQueue(uint64 guid)
    {
        if (!QueueContainsGuid(currentQueueStore, guid))
            currentQueueStore.push_back(guid);
    }

    void LFGQueue::RemoveFromCurrentQueue(uint64 guid)
    {
        currentQueueStore.remove(guid);
    }

    void LFGQueue::AddQueueData(uint64 guid, time_t joinTime, LfgDungeonSet const& dungeons, LfgRolesMap const& rolesMap)
    {
        QueueDataStore[guid] = LfgQueueData(joinTime, dungeons, rolesMap);
        AddToQueue(guid);
    }

    void LFGQueue::RemoveQueueData(uint64 guid)
    {
        LfgQueueDataContainer::iterator it = QueueDataStore.find(guid);
        if (it != QueueDataStore.end())
            QueueDataStore.erase(it);
    }

    bool LFGQueue::HasQueueData(uint64 guid) const
    {
        return QueueDataStore.find(guid) != QueueDataStore.end();
    }

    void LFGQueue::UpdateWaitTimeAvg(int32 waitTime, uint32 dungeonId)
    {
        LfgWaitTime& wt = waitTimesAvgStore[dungeonId];
        uint32 old_number = wt.number++;
        wt.time = int32((wt.time * old_number + waitTime) / wt.number);
    }

    void LFGQueue::UpdateWaitTimeTank(int32 waitTime, uint32 dungeonId)
    {
        LfgWaitTime& wt = waitTimesTankStore[dungeonId];
        uint32 old_number = wt.number++;
        wt.time = int32((wt.time * old_number + waitTime) / wt.number);
    }

    void LFGQueue::UpdateWaitTimeHealer(int32 waitTime, uint32 dungeonId)
    {
        LfgWaitTime& wt = waitTimesHealerStore[dungeonId];
        uint32 old_number = wt.number++;
        wt.time = int32((wt.time * old_number + waitTime) / wt.number);
    }

    void LFGQueue::UpdateWaitTimeDps(int32 waitTime, uint32 dungeonId)
    {
        LfgWaitTime& wt = waitTimesDpsStore[dungeonId];
        uint32 old_number = wt.number++;
        wt.time = int32((wt.time * old_number + waitTime) / wt.number);
    }

    /**
       Remove from cached compatible dungeons any entry that contains the given guid

       @param[in]     guid Guid to remove from compatible cache
    */
    void LFGQueue::RemoveFromCompatibles(uint64 guid)
    {
        SF_LOG_DEBUG("lfg.queue.data.compatibles.remove", "Removing [%u]", GUID_LOPART(guid));
        for (LfgCompatibleContainer::iterator itNext = CompatibleMapStore.begin(); itNext != CompatibleMapStore.end();)
        {
            LfgCompatibleContainer::iterator it = itNext++;
            if (CompatibleKeyContainsGuid(it->first, guid))
                CompatibleMapStore.erase(it);
        }
    }

    /**
       Stores the compatibility of a list of guids

       @param[in]     key String concatenation of guids (| used as separator)
       @param[in]     compatibles type of compatibility
    */
    void LFGQueue::SetCompatibles(std::string const& key, LfgCompatibility compatibles)
    {
        LfgCompatibilityData& data = CompatibleMapStore[key];
        data.compatibility = compatibles;
    }

    void LFGQueue::SetCompatibilityData(std::string const& key, LfgCompatibilityData const& data)
    {
        CompatibleMapStore[key] = data;
    }

    /**
       Get the compatibility of a group of guids

       @param[in]     key String concatenation of guids (| used as separator)
       @return LfgCompatibility type of compatibility
    */
    LfgCompatibility LFGQueue::GetCompatibles(std::string const& key)
    {
        LfgCompatibleContainer::iterator itr = CompatibleMapStore.find(key);
        if (itr != CompatibleMapStore.end())
            return itr->second.compatibility;

        return LFG_COMPATIBILITY_PENDING;
    }

    LfgCompatibilityData* LFGQueue::GetCompatibilityData(std::string const& key)
    {
        LfgCompatibleContainer::iterator itr = CompatibleMapStore.find(key);
        if (itr != CompatibleMapStore.end())
            return &(itr->second);

        return NULL;
    }

    bool LFGQueue::IsRaidFinderQueueEntry(uint64 guid) const
    {
        LfgQueueDataContainer::const_iterator itQueue = QueueDataStore.find(guid);
        if (itQueue == QueueDataStore.end())
            return false;

        return HasRaidFinderDungeon(itQueue->second.dungeons);
    }

    uint8 LFGQueue::FindGroups()
    {
        uint8 proposals = 0;

        // Raid Finder is handled by its own accumulator; the 5-man combinatorial matcher cannot assemble 25 players
        proposals += FindRaidFinderGroups();

        LfgGuidList firstNew;
        while (!newToQueueStore.empty())
        {
            uint64 frontguid = newToQueueStore.front();
            SF_LOG_DEBUG("lfg.queue.match.check.new", "Checking [%u] newToQueue(%u), currentQueue(%u)", GUID_LOPART(frontguid), uint32(newToQueueStore.size()), uint32(currentQueueStore.size()));
            RemoveFromNewQueue(frontguid);

            // Raid-finder entries never enter the 5-man matcher; keep them queued for the accumulator
            if (IsRaidFinderQueueEntry(frontguid))
            {
                AddToCurrentQueue(frontguid);
                continue;
            }

            firstNew.clear();
            firstNew.push_back(frontguid);

            LfgGuidList temporalList;
            for (LfgGuidList::const_iterator it = currentQueueStore.begin(); it != currentQueueStore.end(); ++it)
                if (!IsRaidFinderQueueEntry(*it))
                    temporalList.push_back(*it);

            LfgCompatibility compatibles = FindNewGroups(firstNew, temporalList);

            if (compatibles == LFG_COMPATIBLES_MATCH)
                ++proposals;
            else
                AddToCurrentQueue(frontguid);                  // Lfg group not found, add this group to the queue.
        }
        return proposals;
    }

    /**
       Accumulate solo/queued raid-finder entries into a single raid proposal.

       Raid Finder cannot use the recursive 5-man matcher (bounded by MAXGROUPSIZE), so instead we
       greedily gather queued entries for each raid-finder dungeon, honoring the 2 tank / 6 heal /
       17 dps caps, and form a proposal once the configured minimum size is reached. Players accept
       the proposal dialog (no auto-accept) before being teleported in.

       @return number of proposals created
    */
    uint8 LFGQueue::FindRaidFinderGroups()
    {
        uint8 proposals = 0;

        uint32 minSize = sWorld->getIntConfig(WorldIntConfigs::CONFIG_LFG_RAIDFINDER_MIN_SIZE);
        if (!minSize)
            minSize = LFG_RF_GROUP_SIZE;
        minSize = std::min<uint32>(minSize, LFG_RF_GROUP_SIZE);

        // Bucket currently-queued raid-finder entries by the specific raid-finder dungeon they can enter
        std::map<uint32, LfgGuidList> candidatesByDungeon;
        for (LfgQueueDataContainer::const_iterator it = QueueDataStore.begin(); it != QueueDataStore.end(); ++it)
        {
            uint64 guid = it->first;
            if (!QueueContainsGuid(newToQueueStore, guid) && !QueueContainsGuid(currentQueueStore, guid))
                continue;

            for (LfgDungeonSet::const_iterator itDungeon = it->second.dungeons.begin(); itDungeon != it->second.dungeons.end(); ++itDungeon)
                if (IsRaidFinderDungeon(*itDungeon))
                    candidatesByDungeon[*itDungeon].push_back(guid);
        }

        LfgGuidSet consumedGuids;                              // guids already placed into a proposal this pass
        for (std::map<uint32, LfgGuidList>::iterator itDungeon = candidatesByDungeon.begin(); itDungeon != candidatesByDungeon.end(); ++itDungeon)
        {
            uint32 dungeonId = itDungeon->first;
            LfgGuidList const& candidates = itDungeon->second;

            LfgGuidList selectedQueues;
            LfgRolesMap selectedRoles;
            std::map<uint64, uint64> playerGroups;             // player guid -> original group guid (0 if solo)
            uint32 selectedCount = 0;

            for (LfgGuidList::const_iterator itGuid = candidates.begin(); itGuid != candidates.end() && selectedCount < minSize; ++itGuid)
            {
                if (consumedGuids.find(*itGuid) != consumedGuids.end())
                    continue;

                LfgQueueDataContainer::const_iterator itData = QueueDataStore.find(*itGuid);
                if (itData == QueueDataStore.end())
                    continue;

                LfgRolesMap const& entryRoles = itData->second.roles;
                if (entryRoles.empty())
                    continue;

                if (selectedCount + entryRoles.size() > LFG_RF_GROUP_SIZE)
                    continue;

                // Trial-fit this entry's players into the current selection honoring the raid role caps
                LfgRolesMap trialRoles = selectedRoles;
                for (LfgRolesMap::const_iterator itRole = entryRoles.begin(); itRole != entryRoles.end(); ++itRole)
                    trialRoles[itRole->first] = itRole->second;

                if (!LFGMgr::CheckRaidFinderRoles(trialRoles, LFG_RF_TANKS_NEEDED, LFG_RF_HEALERS_NEEDED, LFG_RF_DPS_NEEDED))
                    continue;

                selectedRoles.swap(trialRoles);
                selectedQueues.push_back(*itGuid);
                selectedCount += entryRoles.size();

                uint64 groupGuid = IS_GROUP_GUID(*itGuid) ? *itGuid : 0;
                for (LfgRolesMap::const_iterator itRole = entryRoles.begin(); itRole != entryRoles.end(); ++itRole)
                    playerGroups[itRole->first] = groupGuid;
            }

            if (selectedCount < minSize)
                continue;

            // Build the raid proposal. Like normal LFG, each player must accept via the proposal
            // dialog before being teleported in (no silent auto-accept/auto-port).
            LfgProposal proposal;
            proposal.queues = selectedQueues;
            proposal.isNew = true;
            proposal.autoAccept = false;
            proposal.cancelTime = time(NULL) + LFG_TIME_PROPOSAL;
            proposal.state = LFG_PROPOSAL_INITIATING;
            proposal.leader = 0;
            proposal.dungeonId = dungeonId;
            proposal.group = 0;

            bool leader = false;
            for (LfgRolesMap::const_iterator itRole = selectedRoles.begin(); itRole != selectedRoles.end(); ++itRole)
            {
                if (itRole->second & PLAYER_ROLE_LEADER)
                {
                    if (!leader || !proposal.leader || std::rand() % 2)
                        proposal.leader = itRole->first;
                    leader = true;
                }
                else if (!leader && (!proposal.leader || std::rand() % 2))
                    proposal.leader = itRole->first;

                LfgProposalPlayer& data = proposal.players[itRole->first];
                data.role = itRole->second;
                std::map<uint64, uint64>::const_iterator itGroup = playerGroups.find(itRole->first);
                data.group = itGroup != playerGroups.end() ? itGroup->second : 0;
            }

            // Remove selected entries from the working queues so the 5-man matcher never touches them.
            // Queue data is kept until the proposal completes (UpdateProposal validates queue data exists).
            for (LfgGuidList::const_iterator itGuid = selectedQueues.begin(); itGuid != selectedQueues.end(); ++itGuid)
            {
                RemoveFromNewQueue(*itGuid);
                RemoveFromCurrentQueue(*itGuid);
                consumedGuids.insert(*itGuid);
            }

            sLFGMgr->AddProposal(proposal);
            ++proposals;

            SF_LOG_DEBUG("lfg.queue.match.raidfinder", "Raid Finder MATCH! Dungeon %u formed with %u players", dungeonId, selectedCount);
        }

        proposals += FindRaidFinderBackfill(candidatesByDungeon, consumedGuids);

        return proposals;
    }

    /**
       Silently backfill under-strength Raid Finder raids from this queue's candidates.

       For each raid registered as needing players (via LFGMgr), pull queued entries whose roles fit
       within the remaining 2 tank / 6 heal / 17 dps caps and build a join-existing proposal
       (proposal.group = raid guid, isNew = false) that the new players accept before joining.
       Reservations guard against two team queues overfilling the same raid within a single Update tick.

       @param[in]     candidatesByDungeon Queued raid-finder entries bucketed by dungeon id
       @param[in,out] consumedGuids       Guids already placed into a proposal this pass
       @return number of proposals created
    */
    uint8 LFGQueue::FindRaidFinderBackfill(std::map<uint32, LfgGuidList>& candidatesByDungeon, LfgGuidSet& consumedGuids)
    {
        uint8 proposals = 0;

        std::vector<std::pair<uint64, uint32>> backfillRaids = sLFGMgr->GetRaidFinderBackfillGroups();
        for (std::vector<std::pair<uint64, uint32>>::const_iterator itRaid = backfillRaids.begin(); itRaid != backfillRaids.end(); ++itRaid)
        {
            uint64 raidGuid = itRaid->first;
            uint32 dungeonId = itRaid->second;

            std::map<uint32, LfgGuidList>::iterator itCandidates = candidatesByDungeon.find(dungeonId);
            if (itCandidates == candidatesByDungeon.end())
                continue;                                      // no queued players for this raid's dungeon in this queue

            Group* raid = sGroupMgr->GetGroupByGUID(GUID_LOPART(raidGuid));
            if (!raid || !raid->isLFGGroup() || !raid->isRaidGroup() || sLFGMgr->GetState(raidGuid) != LFG_STATE_DUNGEON)
            {
                sLFGMgr->DeregisterRaidFinderBackfill(raidGuid);
                continue;
            }

            // Account for members already committed by pending proposals this tick to avoid overfilling
            uint32 filledCount = raid->GetMembersCount() + sLFGMgr->GetRaidFinderBackfillReserved(raidGuid);
            if (filledCount >= LFG_RF_GROUP_SIZE)
            {
                sLFGMgr->DeregisterRaidFinderBackfill(raidGuid);
                continue;
            }

            // Seed the role map with the raid's current members so caps are honored across the whole raid
            LfgRolesMap selectedRoles;
            for (Group::MemberSlot const& slot : raid->GetMemberSlots())
            {
                uint8 role = slot.roles & (PLAYER_ROLE_TANK | PLAYER_ROLE_HEALER | PLAYER_ROLE_DAMAGE);
                selectedRoles[slot.guid] = role ? role : uint8(PLAYER_ROLE_DAMAGE);
            }

            LfgGuidList const& candidates = itCandidates->second;
            LfgGuidList selectedQueues;
            std::map<uint64, uint64> playerGroups;             // player guid -> original group guid (0 if solo)
            LfgGuidSet addedPlayers;                            // new players pulled into the raid

            for (LfgGuidList::const_iterator itGuid = candidates.begin(); itGuid != candidates.end() && filledCount < LFG_RF_GROUP_SIZE; ++itGuid)
            {
                if (consumedGuids.find(*itGuid) != consumedGuids.end())
                    continue;

                LfgQueueDataContainer::const_iterator itData = QueueDataStore.find(*itGuid);
                if (itData == QueueDataStore.end())
                    continue;

                LfgRolesMap const& entryRoles = itData->second.roles;
                if (entryRoles.empty())
                    continue;

                if (filledCount + entryRoles.size() > LFG_RF_GROUP_SIZE)
                    continue;

                // Trial-fit this entry's players alongside the existing raid honoring the raid role caps
                LfgRolesMap trialRoles = selectedRoles;
                for (LfgRolesMap::const_iterator itRole = entryRoles.begin(); itRole != entryRoles.end(); ++itRole)
                    trialRoles[itRole->first] = itRole->second;

                if (!LFGMgr::CheckRaidFinderRoles(trialRoles, LFG_RF_TANKS_NEEDED, LFG_RF_HEALERS_NEEDED, LFG_RF_DPS_NEEDED))
                    continue;

                selectedRoles.swap(trialRoles);
                selectedQueues.push_back(*itGuid);
                filledCount += entryRoles.size();

                uint64 groupGuid = IS_GROUP_GUID(*itGuid) ? *itGuid : 0;
                for (LfgRolesMap::const_iterator itRole = entryRoles.begin(); itRole != entryRoles.end(); ++itRole)
                {
                    playerGroups[itRole->first] = groupGuid;
                    addedPlayers.insert(itRole->first);
                }
            }

            if (selectedQueues.empty())
                continue;                                      // nothing matched this pass; keep the raid registered

            // Build a join-existing proposal containing only the new players. They must accept the
            // proposal dialog before being pulled into the raid (matches normal LFG behavior).
            LfgProposal proposal;
            proposal.queues = selectedQueues;
            proposal.isNew = false;
            proposal.autoAccept = false;
            proposal.cancelTime = time(NULL) + LFG_TIME_PROPOSAL;
            proposal.state = LFG_PROPOSAL_INITIATING;
            proposal.leader = raid->GetLeaderGUID();
            proposal.dungeonId = dungeonId;
            proposal.group = raidGuid;

            for (LfgRolesMap::const_iterator itRole = selectedRoles.begin(); itRole != selectedRoles.end(); ++itRole)
            {
                if (addedPlayers.find(itRole->first) == addedPlayers.end())
                    continue;                                  // existing raid member; leave untouched

                LfgProposalPlayer& data = proposal.players[itRole->first];
                data.role = itRole->second;
                std::map<uint64, uint64>::const_iterator itGroup = playerGroups.find(itRole->first);
                data.group = itGroup != playerGroups.end() ? itGroup->second : 0;
            }

            for (LfgGuidList::const_iterator itGuid = selectedQueues.begin(); itGuid != selectedQueues.end(); ++itGuid)
            {
                RemoveFromNewQueue(*itGuid);
                RemoveFromCurrentQueue(*itGuid);
                consumedGuids.insert(*itGuid);
            }

            sLFGMgr->ReserveRaidFinderBackfillSlots(raidGuid, uint32(addedPlayers.size()));
            sLFGMgr->AddProposal(proposal);
            ++proposals;

            if (filledCount >= LFG_RF_GROUP_SIZE)
                sLFGMgr->DeregisterRaidFinderBackfill(raidGuid);

            SF_LOG_DEBUG("lfg.queue.match.raidfinder", "Raid Finder BACKFILL! Raid %u dungeon %u pulled %u players (now %u)",
                GUID_LOPART(raidGuid), dungeonId, uint32(addedPlayers.size()), filledCount);
        }

        return proposals;
    }

    /**
       Checks que main queue to try to form a Lfg group. Returns first match found (if any)

       @param[in]     check List of guids trying to match with other groups
       @param[in]     all List of all other guids in main queue to match against
       @return LfgCompatibility type of compatibility between groups
    */
    LfgCompatibility LFGQueue::FindNewGroups(LfgGuidList& check, LfgGuidList& all)
    {
        std::string strGuids = ConcatenateGuids(check);
        LfgCompatibility compatibles = GetCompatibles(strGuids);

        SF_LOG_DEBUG("lfg.queue.match.check", "Guids: (%s): %s - all(%s)", strGuids.c_str(), GetCompatibleString(compatibles), ConcatenateGuids(all).c_str());
        if (compatibles == LFG_COMPATIBILITY_PENDING) // Not previously cached, calculate
            compatibles = CheckCompatibility(check);

        if (compatibles == LFG_COMPATIBLES_BAD_STATES && sLFGMgr->AllQueued(check))
        {
            SF_LOG_DEBUG("lfg.queue.match.check", "Guids: (%s) cached bad states are now valid, rebuilding compatibility", strGuids.c_str());
            SetCompatibles(strGuids, LFG_COMPATIBILITY_PENDING);
            return CheckCompatibility(check);
        }

        if (compatibles != LFG_COMPATIBLES_WITH_LESS_PLAYERS)
            return compatibles;

        // Try to match with queued groups
        while (!all.empty())
        {
            check.push_back(all.front());
            all.pop_front();
            LfgCompatibility subcompatibility = FindNewGroups(check, all);
            if (subcompatibility == LFG_COMPATIBLES_MATCH)
                return LFG_COMPATIBLES_MATCH;
            check.pop_back();
        }
        return compatibles;
    }

    /**
       Check compatibilities between groups. If group is Matched proposal will be created

       @param[in]     check List of guids to check compatibilities
       @return LfgCompatibility type of compatibility
    */
    LfgCompatibility LFGQueue::CheckCompatibility(LfgGuidList check)
    {
        std::string strGuids = ConcatenateGuids(check);
        LfgProposal proposal;
        LfgDungeonSet proposalDungeons;
        LfgGroupsMap proposalGroups;
        LfgRolesMap proposalRoles;

        // Check for correct size
        if (check.size() > MAXGROUPSIZE || check.empty())
        {
            SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s): Size wrong - Not compatibles", strGuids.c_str());
            return LFG_INCOMPATIBLES_WRONG_GROUP_SIZE;
        }

        // Check all-but-new compatiblitity
        if (check.size() > 2)
        {
            uint64 frontGuid = check.front();
            check.pop_front();

            // Check all-but-new compatibilities (New, A, B, C, D) --> check(A, B, C, D)
            LfgCompatibility child_compatibles = CheckCompatibility(check);
            if (child_compatibles < LFG_COMPATIBLES_WITH_LESS_PLAYERS) // Group not compatible
            {
                SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) child %s not compatibles", strGuids.c_str(), ConcatenateGuids(check).c_str());
                SetCompatibles(strGuids, child_compatibles);
                return child_compatibles;
            }
            check.push_front(frontGuid);
        }

        // Check if more than one LFG group and number of players joining
        uint8 numPlayers = 0;
        uint8 numLfgGroups = 0;
        for (LfgGuidList::const_iterator it = check.begin(); it != check.end() && numLfgGroups < 2 && numPlayers <= MAXGROUPSIZE; ++it)
        {
            uint64 guid = (*it);
            LfgQueueDataContainer::iterator itQueue = QueueDataStore.find(guid);
            if (itQueue == QueueDataStore.end())
            {
                SF_LOG_ERROR("lfg.queue.match.compatibility.check", "Guid: [%u] is not queued but listed as queued!", GUID_LOPART(guid));
                RemoveFromQueue(guid);
                return LFG_COMPATIBILITY_PENDING;
            }

            // Store group so we don't need to call Mgr to get it later (if it's player group will be 0 otherwise would have joined as group)
            for (LfgRolesMap::const_iterator it2 = itQueue->second.roles.begin(); it2 != itQueue->second.roles.end(); ++it2)
                proposalGroups[it2->first] = IS_GROUP_GUID(itQueue->first) ? itQueue->first : 0;

            numPlayers += itQueue->second.roles.size();

            if (sLFGMgr->IsLfgGroup(guid))
            {
                if (!numLfgGroups)
                    proposal.group = guid;
                ++numLfgGroups;
            }
        }

        // Group with less than the target dungeon size is always compatible
        if (check.size() == 1)
        {
            LfgQueueDataContainer::iterator itQueue = QueueDataStore.find(check.front());
            uint8 groupSize = GetQueueGroupSize(itQueue->second.dungeons);

            if (numPlayers != groupSize)
            {
                SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) single group. Compatibles", strGuids.c_str());

                LfgCompatibilityData data(LFG_COMPATIBLES_WITH_LESS_PLAYERS);
                data.roles = itQueue->second.roles;
                if (!CheckQueueRoles(data.roles, itQueue->second.dungeons))
                {
                    SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) single group has invalid roles", strGuids.c_str());
                    SetCompatibles(strGuids, LFG_INCOMPATIBLES_NO_ROLES);
                    return LFG_INCOMPATIBLES_NO_ROLES;
                }

                UpdateBestCompatibleInQueue(itQueue, strGuids, data.roles);
                SetCompatibilityData(strGuids, data);
                return LFG_COMPATIBLES_WITH_LESS_PLAYERS;
            }
        }

        if (numLfgGroups > 1)
        {
            SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) More than one Lfggroup (%u)", strGuids.c_str(), numLfgGroups);
            SetCompatibles(strGuids, LFG_INCOMPATIBLES_MULTIPLE_LFG_GROUPS);
            return LFG_INCOMPATIBLES_MULTIPLE_LFG_GROUPS;
        }

        // If it's single group no need to check for duplicate players, ignores, bad roles or bad dungeons as it's been checked before joining
        if (check.size() > 1)
        {
            for (LfgGuidList::const_iterator it = check.begin(); it != check.end(); ++it)
            {
                const LfgRolesMap& roles = QueueDataStore[(*it)].roles;
                for (LfgRolesMap::const_iterator itRoles = roles.begin(); itRoles != roles.end(); ++itRoles)
                {
                    LfgRolesMap::const_iterator itPlayer;
                    for (itPlayer = proposalRoles.begin(); itPlayer != proposalRoles.end(); ++itPlayer)
                    {
                        if (itRoles->first == itPlayer->first)
                        {
                            SF_LOG_ERROR("lfg.queue.match.compatibility.check", "Guids: ERROR! Player multiple times in queue! [%u]", GUID_LOPART(itRoles->first));
                        }
                        else if (sLFGMgr->HasIgnore(itRoles->first, itPlayer->first))
                            break;
                    }
                    if (itPlayer == proposalRoles.end())
                        proposalRoles[itRoles->first] = itRoles->second;
                }
            }

            if (uint8 playersize = numPlayers - proposalRoles.size())
            {
                SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) not compatible, %u players are ignoring each other", strGuids.c_str(), playersize);
                SetCompatibles(strGuids, LFG_INCOMPATIBLES_HAS_IGNORES);
                return LFG_INCOMPATIBLES_HAS_IGNORES;
            }

            LfgGuidList::iterator itguid = check.begin();
            proposalDungeons = QueueDataStore[*itguid].dungeons;
            std::ostringstream o;
            o << ", " << *itguid << ": (" << ConcatenateDungeons(proposalDungeons) << ")";
            for (++itguid; itguid != check.end(); ++itguid)
            {
                LfgDungeonSet temporal;
                LfgDungeonSet& dungeons = QueueDataStore[*itguid].dungeons;
                o << ", " << *itguid << ": (" << ConcatenateDungeons(dungeons) << ")";
                std::set_intersection(proposalDungeons.begin(), proposalDungeons.end(), dungeons.begin(), dungeons.end(), std::inserter(temporal, temporal.begin()));
                proposalDungeons = temporal;
            }

            if (proposalDungeons.empty())
            {
                SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) No compatible dungeons%s", strGuids.c_str(), o.str().c_str());
                SetCompatibles(strGuids, LFG_INCOMPATIBLES_NO_DUNGEONS);
                return LFG_INCOMPATIBLES_NO_DUNGEONS;
            }

            LfgRolesMap debugRoles = proposalRoles;
            if (!CheckQueueRoles(proposalRoles, proposalDungeons))
            {
                std::ostringstream rolesDebug;
                for (LfgRolesMap::const_iterator it = debugRoles.begin(); it != debugRoles.end(); ++it)
                    rolesDebug << ", " << it->first << ": " << GetRolesString(it->second);

                SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) Roles not compatible%s", strGuids.c_str(), rolesDebug.str().c_str());
                SetCompatibles(strGuids, LFG_INCOMPATIBLES_NO_ROLES);
                return LFG_INCOMPATIBLES_NO_ROLES;
            }
        }
        else
        {
            uint64 gguid = *check.begin();
            const LfgQueueData& queue = QueueDataStore[gguid];
            proposalDungeons = queue.dungeons;
            proposalRoles = queue.roles;
            LfgRolesMap debugRoles = proposalRoles;
            if (!CheckQueueRoles(proposalRoles, proposalDungeons))     // assign new roles
            {
                std::ostringstream o;
                for (LfgRolesMap::const_iterator it = debugRoles.begin(); it != debugRoles.end(); ++it)
                    o << ", " << it->first << ": " << GetRolesString(it->second);

                SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) single group roles not compatible%s", strGuids.c_str(), o.str().c_str());
                SetCompatibles(strGuids, LFG_INCOMPATIBLES_NO_ROLES);
                return LFG_INCOMPATIBLES_NO_ROLES;
            }
        }

        uint8 groupSize = GetQueueGroupSize(proposalDungeons);
        if (numPlayers > groupSize)
        {
            SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) Too much players (%u)", strGuids.c_str(), numPlayers);
            SetCompatibles(strGuids, LFG_INCOMPATIBLES_TOO_MUCH_PLAYERS);
            return LFG_INCOMPATIBLES_TOO_MUCH_PLAYERS;
        }

        // Enough players?
        if (numPlayers != groupSize)
        {
            SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) Compatibles but not enough players(%u)", strGuids.c_str(), numPlayers);
            LfgCompatibilityData data(LFG_COMPATIBLES_WITH_LESS_PLAYERS);
            data.roles = proposalRoles;

            for (LfgGuidList::const_iterator itr = check.begin(); itr != check.end(); ++itr)
                UpdateBestCompatibleInQueue(QueueDataStore.find(*itr), strGuids, data.roles);

            SetCompatibilityData(strGuids, data);
            return LFG_COMPATIBLES_WITH_LESS_PLAYERS;
        }

        uint64 gguid = *check.begin();
        proposal.queues = check;
        proposal.isNew = numLfgGroups != 1 || sLFGMgr->GetOldState(gguid) != LFG_STATE_DUNGEON;

        if (!sLFGMgr->AllQueued(check))
        {
            SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) Group MATCH but can't create proposal!", strGuids.c_str());
            SetCompatibles(strGuids, LFG_COMPATIBLES_BAD_STATES);
            return LFG_COMPATIBLES_BAD_STATES;
        }

        // Create a new proposal
        proposal.cancelTime = time(NULL) + LFG_TIME_PROPOSAL;
        proposal.state = LFG_PROPOSAL_INITIATING;
        proposal.leader = 0;
        proposal.dungeonId = Skyfire::Containers::SelectRandomContainerElement(proposalDungeons);

        bool leader = false;
        for (LfgRolesMap::const_iterator itRoles = proposalRoles.begin(); itRoles != proposalRoles.end(); ++itRoles)
        {
            // Assing new leader
            if (itRoles->second & PLAYER_ROLE_LEADER)
            {
                if (!leader || !proposal.leader || std::rand() % 2)
                    proposal.leader = itRoles->first;
                leader = true;
            }
            else if (!leader && (!proposal.leader || std::rand() % 2))
                proposal.leader = itRoles->first;

            // Assing player data and roles
            LfgProposalPlayer& data = proposal.players[itRoles->first];
            data.role = itRoles->second;
            data.group = proposalGroups.find(itRoles->first)->second;
            if (!proposal.isNew && data.group && data.group == proposal.group) // Player from existing group, autoaccept
                data.accept = LFG_ANSWER_AGREE;
        }

        // Mark proposal members as not queued (but not remove queue data)
        for (LfgGuidList::const_iterator itQueue = proposal.queues.begin(); itQueue != proposal.queues.end(); ++itQueue)
        {
            uint64 guid = (*itQueue);
            RemoveFromNewQueue(guid);
            RemoveFromCurrentQueue(guid);
        }

        sLFGMgr->AddProposal(proposal);

        SF_LOG_DEBUG("lfg.queue.match.compatibility.check", "Guids: (%s) MATCH! Group formed", strGuids.c_str());
        SetCompatibles(strGuids, LFG_COMPATIBLES_MATCH);
        return LFG_COMPATIBLES_MATCH;
    }

    void LFGQueue::UpdateQueueTimers(uint8 queueId, time_t currTime)
    {
        SF_LOG_TRACE("lfg.queue.timers.update", "Updating queue timers...");
        for (LfgQueueDataContainer::iterator itQueue = QueueDataStore.begin(); itQueue != QueueDataStore.end(); ++itQueue)
        {
            if (!QueueContainsGuid(newToQueueStore, itQueue->first) && !QueueContainsGuid(currentQueueStore, itQueue->first))
                continue;

            LfgQueueData& queueinfo = itQueue->second;
            if (queueinfo.dungeons.empty())
            {
                SF_LOG_DEBUG("lfg.queue.timers.update", "Skipping queue data for [%u] with no selected dungeons", GUID_LOPART(itQueue->first));
                continue;
            }

            uint32 dungeonId = (*queueinfo.dungeons.begin());
            uint32 queuedTime = uint32(currTime - queueinfo.joinTime);
            int32 waitTime = -1;
            int32 wtTank = waitTimesTankStore[dungeonId].time;
            int32 wtHealer = waitTimesHealerStore[dungeonId].time;
            int32 wtDps = waitTimesDpsStore[dungeonId].time;
            int32 wtAvg = waitTimesAvgStore[dungeonId].time;

            if (queueinfo.bestCompatible.empty())
                FindBestCompatibleInQueue(itQueue);

            uint8 tanks = queueinfo.tanks;
            uint8 healers = queueinfo.healers;
            uint8 dps = queueinfo.dps;
            if (!queueinfo.bestCompatible.empty())
                waitTime = SelectWaitTime(tanks, healers, dps, wtTank, wtHealer, wtDps, wtAvg);
            else
            {
                CalculateRoleShortage(queueinfo.roles, queueinfo.dungeons, tanks, healers, dps);
                waitTime = SelectWaitTime(tanks, healers, dps, wtTank, wtHealer, wtDps, wtAvg);
            }

            LfgQueueStatusData queueData(queueId, dungeonId, queueinfo.joinTime, waitTime, wtAvg, wtTank, wtHealer, wtDps, queuedTime, tanks, healers, dps);
            for (LfgRolesMap::const_iterator itPlayer = queueinfo.roles.begin(); itPlayer != queueinfo.roles.end(); ++itPlayer)
            {
                uint64 pguid = itPlayer->first;
                LFGMgr::SendLfgQueueStatus(pguid, queueData);
            }
        }
    }

    time_t LFGQueue::GetJoinTime(uint64 guid) const
    {
        LfgQueueDataContainer::const_iterator itr = QueueDataStore.find(guid);
        if (itr != QueueDataStore.end())
            return itr->second.joinTime;

        return 0;
    }

    std::string LFGQueue::DumpQueueInfo(bool full /* = false */) const
    {
        uint32 players = 0;
        uint32 groups = 0;
        uint32 playersInGroup = 0;
        uint32 currentEntries = 0;
        uint32 newEntries = 0;
        uint32 storedOnlyEntries = 0;
        uint32 tankRoles = 0;
        uint32 healerRoles = 0;
        uint32 damageRoles = 0;
        uint32 leaderRoles = 0;
        uint32 noRoles = 0;
        time_t const currTime = time(NULL);

        for (uint8 i = 0; i < 2; ++i)
        {
            LfgGuidList const& queue = i ? newToQueueStore : currentQueueStore;
            if (i)
                newEntries = uint32(queue.size());
            else
                currentEntries = uint32(queue.size());

            for (LfgGuidList::const_iterator it = queue.begin(); it != queue.end(); ++it)
            {
                uint64 guid = *it;
                if (IS_GROUP_GUID(guid))
                {
                    groups++;
                    playersInGroup += sLFGMgr->GetPlayerCount(guid);
                }
                else
                    players++;
            }
        }

        for (LfgQueueDataContainer::const_iterator itr = QueueDataStore.begin(); itr != QueueDataStore.end(); ++itr)
        {
            if (!QueueContainsGuid(currentQueueStore, itr->first) && !QueueContainsGuid(newToQueueStore, itr->first))
                ++storedOnlyEntries;

            for (LfgRolesMap::const_iterator itRoles = itr->second.roles.begin(); itRoles != itr->second.roles.end(); ++itRoles)
            {
                uint8 roles = itRoles->second;
                if (roles & PLAYER_ROLE_TANK)
                    ++tankRoles;
                if (roles & PLAYER_ROLE_HEALER)
                    ++healerRoles;
                if (roles & PLAYER_ROLE_DAMAGE)
                    ++damageRoles;
                if (roles & PLAYER_ROLE_LEADER)
                    ++leaderRoles;
                if (!(roles & LFG_COMBAT_ROLE_MASK))
                    ++noRoles;
            }
        }

        std::ostringstream o;
        o << "Queued Players: " << players << " (in group: " << playersInGroup << ") Groups: " << groups << "\n";
        o << "Entries current/new/stored-only: " << currentEntries << "/" << newEntries << "/" << storedOnlyEntries << "\n";
        o << "Role buckets tank/healer/damage/leader/none: " << tankRoles << "/" << healerRoles << "/" << damageRoles
            << "/" << leaderRoles << "/" << noRoles << "\n";
        if (!full)
            return o.str();

        o << "Current Queue: " << ConcatenateGuids(currentQueueStore) << "\n";
        o << "New Queue: " << ConcatenateGuids(newToQueueStore) << "\n";

        for (LfgQueueDataContainer::const_iterator itr = QueueDataStore.begin(); itr != QueueDataStore.end(); ++itr)
        {
            LfgQueueData const& queueInfo = itr->second;
            o << "  " << (IS_GROUP_GUID(itr->first) ? "Group" : "Player")
                << " " << itr->first
                << " queued " << uint32(currTime > queueInfo.joinTime ? currTime - queueInfo.joinTime : 0) << "s"
                << " roles: " << queueInfo.roles.size()
                << " dungeons: " << ConcatenateDungeons(queueInfo.dungeons)
                << " need T/H/D: " << uint32(queueInfo.tanks) << "/" << uint32(queueInfo.healers) << "/" << uint32(queueInfo.dps);

            if (!queueInfo.bestCompatible.empty())
                o << " best: " << queueInfo.bestCompatible;

            if (QueueContainsGuid(currentQueueStore, itr->first))
                o << " phase: current";
            else if (QueueContainsGuid(newToQueueStore, itr->first))
                o << " phase: new";
            else
                o << " phase: stored-only";

            o << "\n";

            for (LfgRolesMap::const_iterator itRoles = queueInfo.roles.begin(); itRoles != queueInfo.roles.end(); ++itRoles)
                o << "    role " << itRoles->first << ": " << GetRolesString(itRoles->second) << "\n";
        }

        return o.str();
    }

    std::string LFGQueue::DumpCompatibleInfo(bool full /* = false */) const
    {
        std::ostringstream o;
        o << "Compatible Map size: " << CompatibleMapStore.size() << "\n";
        if (full)
            for (LfgCompatibleContainer::const_iterator itr = CompatibleMapStore.begin(); itr != CompatibleMapStore.end(); ++itr)
                o << "(" << itr->first << "): " << GetCompatibleString(itr->second.compatibility) << "\n";

        return o.str();
    }

    void LFGQueue::FindBestCompatibleInQueue(LfgQueueDataContainer::iterator itrQueue)
    {
        SF_LOG_DEBUG("lfg.queue.compatibles.find", "Guid: " UI64FMTD, itrQueue->first);

        for (LfgCompatibleContainer::const_iterator itr = CompatibleMapStore.begin(); itr != CompatibleMapStore.end(); ++itr)
            if (itr->second.compatibility == LFG_COMPATIBLES_WITH_LESS_PLAYERS &&
                CompatibleKeyContainsGuid(itr->first, itrQueue->first))
            {
                UpdateBestCompatibleInQueue(itrQueue, itr->first, itr->second.roles);
            }
    }

    void LFGQueue::UpdateBestCompatibleInQueue(LfgQueueDataContainer::iterator itrQueue, std::string const& key, LfgRolesMap const& roles)
    {
        LfgQueueData& queueData = itrQueue->second;

        uint8 storedSize = queueData.bestCompatible.empty() ? 0 :
            std::count(queueData.bestCompatible.begin(), queueData.bestCompatible.end(), '|') + 1;

        uint8 size = std::count(key.begin(), key.end(), '|') + 1;

        if (size <= storedSize)
            return;

        SF_LOG_DEBUG("lfg.queue.compatibles.update", "Changed (%s) to (%s) as best compatible group for " UI64FMTD,
            queueData.bestCompatible.c_str(), key.c_str(), itrQueue->first);

        queueData.bestCompatible = key;
        CalculateRoleShortage(roles, queueData.dungeons, queueData.tanks, queueData.healers, queueData.dps);
    }

} // namespace lfg
