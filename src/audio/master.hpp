#pragma once

#include <map>
#include <string>
#include <vector>

#include "utils/defs.hpp"
#include <glm/glm.hpp>

class ALCdevice;
class ALCcontext;

namespace audio
{

class Source;

class Category
{
public:
    Category(const std::string& name);
    // DELETE_COPY_MOVE(Category) // HACK: make it possible to push into reserved vector

    const std::string& GetName() const { return name_; }

    void SetVolume(float volume);
    float GetVolume() const { return volume_; }

private:
    float volume_ = 1.0f;
    std::string name_;

    Source* first_source_ = nullptr;
    friend class Source;
};

class Master
{
public:
    Master();
    DELETE_COPY_MOVE(Master)

    void SetListenerOrientation(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);
    void SetListenerOrientation(const glm::mat4& world_matrix);

    Category* GetCategory(const std::string& name);

    void SetMasterVolume(float volume);
    float GetMasterVolume() const { return master_volume_; }

    ~Master();

private:
    ALCdevice* device_ = nullptr;
    ALCcontext* context_ = nullptr;

    std::vector<Category> categories_;
    std::map<std::string, Category*> category_map_;

    float master_volume_ = 1.0f;
};

} // namespace audio