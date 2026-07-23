/*
 * SkyFire playerbots — action priority queue (AC Queue / ActionBasket thin port).
 */

#ifndef SF_BOT_QUEUE_H
#define SF_BOT_QUEUE_H

#include "BotAction.h"
#include <queue>
#include <string>
#include <vector>

struct BotQueueItem
{
    std::string name;
    float relevance = 0.0f;

    bool operator<(BotQueueItem const& other) const
    {
        // priority_queue is max-heap; higher relevance first.
        return relevance < other.relevance;
    }
};

class BotQueue
{
public:
    void Push(std::string name, float relevance)
    {
        _items.push(BotQueueItem{ std::move(name), relevance });
    }

    bool Empty() const { return _items.empty(); }
    size_t Size() const { return _items.size(); }

    BotQueueItem const* Peek() const
    {
        if (_items.empty())
            return nullptr;
        return &_items.top();
    }

    BotQueueItem Pop()
    {
        BotQueueItem top = _items.top();
        _items.pop();
        return top;
    }

    void Clear()
    {
        while (!_items.empty())
            _items.pop();
    }

private:
    std::priority_queue<BotQueueItem> _items;
};

#endif
