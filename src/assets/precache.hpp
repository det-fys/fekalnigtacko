#pragma once

#include <string>
#include <vector>
#include <any>

namespace assets
{

struct PrecacheItem
{
    std::string type;
    std::string name;
};

class Precache
{
public:
    Precache(const std::string& path);

    void LoadNext();
    
    size_t GetNumItems() const { return items_.size(); }
    size_t GetNumLoaded() const { return num_loaded_; }
    bool IsDone() const { return GetNumLoaded() >= GetNumItems(); };

private:
    std::vector<PrecacheItem> items_;
    size_t num_loaded_ = 0;
    std::vector<std::any> refs_;
};

}