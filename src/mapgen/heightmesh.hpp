#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "assets/asset_manager.hpp"
#include "defs.hpp"

namespace mg
{

class HeightMesh : public assets::Asset
{
public:
    HeightMesh() = default;

    static std::shared_ptr<HeightMesh> Load(const std::string& name);
    static std::shared_ptr<HeightMesh> LoadFromFile(const std::string& name);

    const std::vector<glm::vec3>& GetVerts() const { return verts_; }
    const std::vector<Triangle>& GetTris() const { return tris_; }

private:
    std::vector<glm::vec3> verts_;
    std::vector<Triangle> tris_;
};



}