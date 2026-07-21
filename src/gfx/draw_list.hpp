#pragma once

#include <vector>

#include "mesh_desc.hpp"
#include "material_desc.hpp"
#include "skeleton_pose_desc.hpp"
#include "deform_texture_desc.hpp"

#include "light_cell.hpp"
#include "light_data.hpp"

namespace gfx
{

struct DrawSurfaceCmd
{
    // mesh
    MeshID mesh = 0;
    MeshIndex tri_offset = 0;
    MeshIndex tri_count = 0;

    // material
    MaterialID material = 0;

    // instance
    const glm::mat4* matrix = nullptr;
    std::span<const glm::vec4> colors;
    SkeletonPoseID pose = 0;
    DeformTextureID deform_tex = 0;
    float dist = 0.0f;
    LightCellCoordHash map_chunk_hash = 0;
};

using LightFlags = uint8_t;
enum LightFlag : LightFlags
{
    LF_DETAIL = 1,
    LF_SHADOWS = 2,
};

struct DrawLightCmd
{
    LightData light;
    float dist2 = 0.0f;
    LightFlags flags = 0;

    DrawLightCmd(const glm::vec3& position, const glm::vec3& color, float radius, LightFlags flags = 0) : flags(flags)
    {
        light.position = position;
        light.color = color;
        light.radius = radius;

        light.dir = glm::vec3(0.0f);
        light.cos_inner = -1.0f;
        light.cos_outer = 0.0f;
    }

    DrawLightCmd(const glm::vec3& position, const glm::vec3& color, float radius, const glm::vec3& dir,
                 float angle_inner, float angle_outer, LightFlags flags = 0)
        : flags(flags)
    {
        light.position = position;
        light.color = color;
        light.radius = radius;

        light.dir = dir;
        light.cos_inner = glm::cos(angle_inner);
        light.cos_outer = glm::cos(angle_outer);
    }
};

struct DrawCoronaCmd
{
    glm::vec3 pos;
    glm::vec3 dir;
    glm::vec3 color;
    float size;

    DrawCoronaCmd(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& color, float size)
        : pos(pos), dir(dir), color(color), size(size)
    {
    }
};

struct DrawBeamCmd
{
    glm::vec3 start;
    glm::vec3 end;
    uint32_t color;
    float radius;
    size_t num_segments;
    float max_offset;

    DrawBeamCmd(const glm::vec3& start, const glm::vec3& end, uint32_t color, float radius, size_t num_segments,
                float max_offset)
        : start(start), end(end), color(color), radius(radius), num_segments(num_segments), max_offset(max_offset)
    {
    }
};

struct DrawHudCmd
{
    // mesh
    MeshID mesh = 0;
    MeshIndex tri_offset = 0;
    MeshIndex tri_count = 0;

    // texture
    TextureID texture = 0;

    // instance
    const glm::mat3* matrix = nullptr;
};

struct DrawList
{
    std::vector<DrawSurfaceCmd> surfaces;
    std::vector<DrawLightCmd> lights;
    std::vector<DrawCoronaCmd> coronas;
    std::vector<DrawBeamCmd> beams;
    std::vector<DrawHudCmd> huds;

    void AddSurface(const DrawSurfaceCmd& cmd) { surfaces.emplace_back(cmd); }

    void AddLight(const DrawLightCmd& cmd) { lights.emplace_back(cmd); }
    void AddLight(const glm::vec3& position, const glm::vec3& color, float radius, LightFlags flags = 0)
    {
        lights.emplace_back(position, color, radius, flags);
    }

    void AddSpotLight(const glm::vec3& position, const glm::vec3& color, float radius, const glm::vec3& dir,
                      float angle_inner, float angle_outer, LightFlags flags = 0)
    {
        lights.emplace_back(position, color, radius, dir, angle_inner, angle_outer, flags);
    }

    void AddCorona(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& color, float size)
    {
        coronas.emplace_back(pos, dir, color, size);
    }

    void AddBeam(const DrawBeamCmd& cmd) { beams.emplace_back(cmd); }
    void AddBeam(const glm::vec3& start, const glm::vec3& end, uint32_t color = 0xFFFFFFFF, float radius = 0.1f,
                 size_t num_segments = 1, float max_offset = 0.0f)
    {
        beams.emplace_back(start, end, color, radius, num_segments, max_offset);
    }

    void AddHUD(const DrawHudCmd& cmd) { huds.emplace_back(cmd); }

    void Clear()
    {
        surfaces.clear();
        lights.clear();
        coronas.clear();
        beams.clear();
        huds.clear();
    }
};

} // namespace gfx