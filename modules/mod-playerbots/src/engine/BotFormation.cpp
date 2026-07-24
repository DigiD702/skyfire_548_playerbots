/*
 * Formation follow angle/distance by combat role.
 *
 * Same-role bots get unique ordinals (GUID-sorted) so two melee DPS never share
 * the same flank angle — that was the 5-man stacking bug.
 */

#include "BotFormation.h"
#include "PlayerbotAI.h"

#include "DBCStores.h"
#include "Group.h"
#include "GroupReference.h"
#include "Player.h"
#include "SharedDefines.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BotFormation
{
namespace
{
    enum class Bucket : uint8
    {
        Tank = 0,
        Healer,
        MeleeDps,
        RangedDps
    };

    bool IsRangedSpec(Player* player)
    {
        if (!player)
            return false;

        uint32 const specId = player->GetTalentSpecialization(player->GetActiveSpec());
        uint8 const cls = player->getClass();
        uint32 const* specs = GetClassSpecializations(cls);
        auto isSpec = [&](uint8 tab) -> bool
        {
            return specs && specs[tab] == specId;
        };

        switch (cls)
        {
            case CLASS_HUNTER:
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                return true;
            case CLASS_SHAMAN:
                return isSpec(0); // Elemental
            case CLASS_DRUID:
                return isSpec(0); // Balance
            default:
                return false;
        }
    }

    Bucket Classify(Player* player)
    {
        if (!player)
            return Bucket::MeleeDps;

        uint32 const specId = player->GetTalentSpecialization(player->GetActiveSpec());
        uint8 const cls = player->getClass();
        uint32 const* specs = GetClassSpecializations(cls);
        auto isSpec = [&](uint8 tab) -> bool
        {
            return specs && specs[tab] == specId;
        };

        bool tank = false;
        bool healer = false;
        switch (cls)
        {
            case CLASS_WARRIOR:      tank = isSpec(2); break;
            case CLASS_PALADIN:      tank = isSpec(1); healer = isSpec(0); break;
            case CLASS_DEATH_KNIGHT: tank = isSpec(0); break;
            case CLASS_PRIEST:       healer = !isSpec(2); break;
            case CLASS_SHAMAN:       healer = isSpec(2); break;
            case CLASS_MONK:         tank = isSpec(0); healer = isSpec(1); break;
            case CLASS_DRUID:        tank = isSpec(2); healer = isSpec(3); break;
            default: break;
        }

        if (tank)
            return Bucket::Tank;
        if (healer)
            return Bucket::Healer;
        return IsRangedSpec(player) ? Bucket::RangedDps : Bucket::MeleeDps;
    }

    // Returns (index, count) for bot inside its role bucket among living members
    // other than the group leader. Index is GUID-sorted for stability.
    bool GetBucketSlot(Player* bot, int& outIndex, int& outCount, Bucket& outBucket)
    {
        outIndex = 0;
        outCount = 1;
        outBucket = Classify(bot);
        if (!bot)
            return false;

        Group* group = bot->GetGroup();
        if (!group)
            return true;

        uint64 const leaderGuid = group->GetLeaderGUID();
        std::vector<Player*> bucket;
        bucket.reserve(5);

        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsInWorld() || !member->IsAlive())
                continue;
            if (member->GetGUID() == leaderGuid)
                continue;
            if (member->GetMap() != bot->GetMap())
                continue;
            if (Classify(member) != outBucket)
                continue;
            bucket.push_back(member);
        }

        if (bucket.empty())
        {
            bucket.push_back(bot);
        }

        std::sort(bucket.begin(), bucket.end(),
            [](Player* a, Player* b) { return a->GetGUIDLow() < b->GetGUIDLow(); });

        outCount = int(bucket.size());
        for (int i = 0; i < outCount; ++i)
        {
            if (bucket[i] == bot)
            {
                outIndex = i;
                return true;
            }
        }

        // Bot somehow not in list (different map mid-teleport); keep index 0.
        outIndex = 0;
        return true;
    }

    // Evenly fan `count` slots across [center - halfSpan, center + halfSpan].
    float FanAngle(float center, float halfSpan, int index, int count)
    {
        if (count <= 1)
            return center;
        float const t = float(index) / float(count - 1); // 0..1
        return center - halfSpan + t * (2.0f * halfSpan);
    }
}

float FollowAngle(PlayerbotAI* ai)
{
    if (!ai || !ai->GetBot())
        return static_cast<float>(M_PI);

    int index = 0;
    int count = 1;
    Bucket bucket = Bucket::MeleeDps;
    GetBucketSlot(ai->GetBot(), index, count, bucket);

    switch (bucket)
    {
        case Bucket::Tank:
            // Slightly ahead / beside leader.
            return FanAngle(0.40f, 0.55f, index, count);
        case Bucket::Healer:
            // Dead behind, small fan if multiple healers.
            return FanAngle(static_cast<float>(M_PI), 0.45f, index, count);
        case Bucket::RangedDps:
            // Rear-right arc.
            return FanAngle(static_cast<float>(M_PI) + 0.95f, 0.70f, index, count);
        case Bucket::MeleeDps:
        default:
            // Rear-left arc.
            return FanAngle(static_cast<float>(M_PI) - 0.95f, 0.70f, index, count);
    }
}

float FollowDistance(PlayerbotAI* ai)
{
    if (!ai || !ai->GetBot())
        return 2.5f;

    int index = 0;
    int count = 1;
    Bucket bucket = Bucket::MeleeDps;
    GetBucketSlot(ai->GetBot(), index, count, bucket);

    float base = 2.5f;
    switch (bucket)
    {
        case Bucket::Tank:     base = 2.0f; break;
        case Bucket::Healer:   base = 3.5f; break;
        case Bucket::RangedDps: base = 5.0f; break;
        case Bucket::MeleeDps:
        default:               base = 2.5f; break;
    }

    return base + float(index) * 0.6f;
}
}
