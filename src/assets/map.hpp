#pragma once

#include <memory>
#include <string>

#include "model.hpp"
#include "utils/transform.hpp"

#ifdef CLIENT
#include "gfx/draw_list.hpp"
#endif // CLIENT

namespace assets
{

struct MapStaticObject
{
    Transform transform;
    std::shared_ptr<const Model> model;
    glm::vec3 color = glm::vec3(1.0f);
};

class Map
{
public:
    Map() = default;
    static std::shared_ptr<const Map> LoadFromFile(const std::string& filename);

    const std::shared_ptr<const Model>& GetBaseModel() const { return basemodel_; }
    const std::vector<MapStaticObject>& GetStaticObjects() const { return static_objects_; }

    CLIENT_ONLY(void Draw(gfx::DrawList& dlist) const;)

private:
    std::shared_ptr<const Model> basemodel_;
    std::vector<MapStaticObject> static_objects_;
};

} // namespace assets