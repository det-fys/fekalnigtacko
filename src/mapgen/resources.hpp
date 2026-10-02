#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "assets/asset_manager.hpp"
#include "assets/material.hpp"
#include "assets/model.hpp"
#include "defs.hpp"

namespace mg
{

// material

enum TemplateMaterialType : uint8_t
{
    TPL_MATERIAL_NORMAL,     // custom material
    TPL_MATERIAL_TERRAIN,    // uses local terrain material & color
    TPL_MATERIAL_HEIGHTMESH, // generates heightmesh
};

struct TemplateMaterialRef
{
    TemplateMaterialType type = TPL_MATERIAL_NORMAL;
    uint16_t id = 0; // for TPL_MATERIAL_NORMAL

    bool operator==(const TemplateMaterialRef& other) const { return type == other.type && id == other.id; }
};

// profile

struct TemplateProfileVertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec2 uv_advance;
};

struct TemplateProfileEdge
{
    std::array<uint32_t, 2> verts;
    TemplateMaterialRef material;
};

struct TemplateProfile
{
    std::string name;
    std::vector<TemplateProfileVertex> verts;
    std::vector<TemplateProfileEdge> edges;
    std::vector<TemplateProfileEdge> edges_reverse;
};

// mesh

struct TemplateMeshVertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    std::array<float, 4> link_weights;
    //bool is_link_vertex = false;
};

struct TemplateMeshTriangle
{
    Triangle tri;
    TemplateMaterialRef material;
};

//struct TemplateMeshLinkEdgeMapping
//{
//    std::array<uint32_t, 2> mesh_verts{};
//};

struct TemplateMeshLink
{
    uint32_t profile_id = 0;
    glm::vec3 pos{0.0f};
    //std::vector<TemplateMeshLinkEdgeMapping> edge_mappings;
    std::vector<uint32_t> profile_vert_mappings; // profile vertex index -> mesh vertex index
    float margin = 0.0f;
    glm::mat4 start_matrix{1.0f};
    glm::mat4 end_matrix{1.0f};
};

struct TemplateMesh
{
    std::string name;
    
    std::vector<TemplateMeshVertex> verts;
    std::vector<TemplateMeshTriangle> tris;

    std::vector<TemplateMeshLink> links;
};

// model

struct StaticObjectModel
{
    std::string name;
    std::shared_ptr<const assets::Model> model;
    uint32_t cross_mesh_id = 0xFFFFFFFF;
    // TODO: info about coloring, billboarding etc.
};

// set

template <typename T>
class ResourceArray
{
public:
    ResourceArray() = default;

    uint32_t Add(const std::string& name, T&& item)
    {
        uint32_t idx = static_cast<uint32_t>(resources_.size());
        resources_.emplace_back(std::move(item));
        name_map_[name] = idx;
        return idx;
    }

    uint32_t GetIndexByName(const std::string& name) const
    {
        auto it = name_map_.find(name);
        return it != name_map_.end() ? it->second : UINT32_MAX;
    }

    const T* TryGetByIndex(uint32_t idx) const { return idx < resources_.size() ? &resources_[idx] : nullptr; }
    const T* TryGetByName(const std::string& name) const { return TryGetByIndex(GetIndexByName(name)); }

    const T& GetByIndex(uint32_t idx) const
    {
        if (idx >= resources_.size())
            throw std::out_of_range("Index out of range");
        return resources_[idx];
    }

    const T& GetByName(const std::string& name) const
    {
        uint32_t idx = GetIndexByName(name);
        if (idx == UINT32_MAX)
            throw std::out_of_range("Name not found");
        return GetByIndex(idx);
    }

private:
    std::vector<T> resources_;
    std::map<std::string, uint32_t> name_map_;
};

struct ResourceSet : public assets::Asset
{
public:
    ResourceSet() = default;

    static std::shared_ptr<ResourceSet> Load(const std::string& path);
    static std::shared_ptr<ResourceSet> LoadFromFile(const std::string& path);

    const ResourceArray<assets::ModelMaterial>& GetMaterials() const { return materials_; }
    const ResourceArray<TemplateProfile>& GetProfiles() const { return profiles_; }
    const ResourceArray<TemplateMesh>& GetMeshes() const { return meshes_; }
    const ResourceArray<StaticObjectModel>& GetModels() const { return models_; }

private:
    ResourceArray<assets::ModelMaterial> materials_;
    ResourceArray<TemplateProfile> profiles_;
    ResourceArray<TemplateMesh> meshes_;
    ResourceArray<StaticObjectModel> models_;
};

} // namespace mg
