#pragma once

#include <string>
#include "mesh_builder.hpp"

namespace assets
{

class Model
{
public:
    Model() = default;
    static std::shared_ptr<const Model> LoadFromFile(const std::string& filename);

    const std::shared_ptr<const Mesh>& GetMesh() const { return mesh_; }

private:
    std::shared_ptr<const Mesh> mesh_;
};

}