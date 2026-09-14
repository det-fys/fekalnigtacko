#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "assets/asset_manager.hpp"
#include "assets/material.hpp"
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

// set

struct ResourceSet : public assets::Asset
{
public:
    ResourceSet() = default;

    static std::shared_ptr<ResourceSet> Load(const std::string& path);
    static std::shared_ptr<ResourceSet> LoadFromFile(const std::string& path);

    const std::vector<assets::ModelMaterial>& GetMaterials() const { return materials_; }
    const assets::ModelMaterial* GetMaterialByName(const std::string& name) const;
    uint32_t GetMaterialIndexByName(const std::string& name) const;

    const std::vector<TemplateProfile>& GetProfiles() const { return profiles_; }
    const TemplateProfile* GetProfileByName(const std::string& name) const;
    uint32_t GetProfileIndexByName(const std::string& name) const;

    const std::vector<TemplateMesh>& GetMeshes() const { return meshes_; }
    const TemplateMesh* GetMeshByName(const std::string& name) const;
    uint32_t GetMeshIndexByName(const std::string& name) const;

private:
    std::vector<assets::ModelMaterial> materials_;
    std::map<std::string, uint32_t> material_map_;

    std::vector<TemplateProfile> profiles_;
    std::map<std::string, uint32_t> profile_map_;

    std::vector<TemplateMesh> meshes_;
    std::map<std::string, uint32_t> mesh_map_;
};

} // namespace mg
