#pragma once

#include <array>
#include <cassert>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "utils/defs.hpp"

#include "../id.hpp"

namespace gfx
{

constexpr size_t RES_ARRAY_BLOCK_SIZE = 64;
constexpr size_t RES_ARRAY_BLOCK_BITS = 6;

template <typename T>
struct ResourceArrayBlock
{
    std::array<std::optional<T>, RES_ARRAY_BLOCK_SIZE> data;
};

template <typename T>
class ResourceArray
{
public:
    ResourceArray()
    {
        // Force-allocate the very first block so ID 0 is physically present
        blocks_.emplace_back(std::make_unique<ResourceArrayBlock<T>>());

        // Push the rest of the IDs from the first block (1 to RES_ARRAY_BLOCK_SIZE - 1) into the free list
        for (ID i = 1; i < RES_ARRAY_BLOCK_SIZE; ++i)
        {
            free_.push_back(i);
        }
    }

    DELETE_COPY_MOVE(ResourceArray);

    template <typename... Args>
    ID Alloc(Args&&... args)
    {
        if (!free_.empty())
        {
            ID id = free_.front();
            free_.pop_front();

            auto& slot = GetSlot(id);
            assert(!slot && "Slot not empty (internal bug?)");

            try
            {
                slot.emplace(std::forward<Args>(args)...);
            }
            catch (...)
            {
                free_.push_front(id); // Return to free list on failure
                throw;
            }

            return id;
        }

        // new block allocation
        blocks_.emplace_back(std::make_unique<ResourceArrayBlock<T>>());

        ID block_base = static_cast<ID>(blocks_.size() - 1) << RES_ARRAY_BLOCK_BITS;

        auto& slot = GetSlot(block_base);
        slot.emplace(std::forward<Args>(args)...);

        // Push the remaining IDs of this new block to the free list
        for (ID i = block_base + 1; i < (block_base + RES_ARRAY_BLOCK_SIZE); ++i)
        {
            free_.push_back(i);
        }

        return block_base;
    }

    T& Get(ID id)
    {
        assert(id != 0 && "Attempted to access reserved null ID 0");
        auto& slot = GetSlot(id);
        assert(slot && "Slot empty");
        return *slot;
    }

    void Free(ID id)
    {
        if (id == 0)
            return;

        auto& slot = GetSlot(id);
        assert(slot && "Slot empty");

        slot.reset();
        free_.push_back(id);
    }

private:
    std::optional<T>& GetSlot(ID id)
    {
        auto block_id = id >> RES_ARRAY_BLOCK_BITS;
        auto item_id = id & (RES_ARRAY_BLOCK_SIZE - 1);

        assert(block_id < blocks_.size() && "ID out of range");
        return blocks_[block_id]->data[item_id];
    }

private:
    std::vector<std::unique_ptr<ResourceArrayBlock<T>>> blocks_;
    std::deque<ID> free_;
};

} // namespace gfx
