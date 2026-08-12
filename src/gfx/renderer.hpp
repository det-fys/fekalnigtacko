#pragma once

#include <cstddef>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "mesh_desc.hpp"
#include "texture_desc.hpp"
#include "material_desc.hpp"
#include "skeleton_pose_desc.hpp"
#include "deform_texture_desc.hpp"
#include "viewport_desc.hpp"

#include "camera.hpp"

struct SDL_Window;

namespace gfx
{

class Scene;

class Renderer
{
public:
    Renderer(SDL_Window* window);

    virtual MeshID CreateMesh(const MeshDescriptor& desc) = 0;
    virtual void SetMeshVertexData(MeshID mesh_id, const MeshVertexData& data) = 0;
    virtual void SetMeshTriangleData(MeshID mesh_id, const MeshTriangleData& data) = 0;
    virtual void ReleaseMesh(MeshID mesh_id) = 0;

    virtual TextureID CreateTexture(const TextureDescriptor& desc) = 0;
    virtual void SetTextureData(TextureID texture_id, std::span<const uint8_t> data) = 0;
    virtual void ReleaseTexture(TextureID texture_id) = 0;

    virtual MaterialID CreateMaterial(const MaterialDescriptor& desc) = 0;
    virtual void ReleaseMaterial(MaterialID material_id) = 0;

    virtual SkeletonPoseID CreateSkeletonPose(const SkeletonPoseDescriptor& desc) = 0;
    virtual void SetSkeletonPoseTransforms(SkeletonPoseID pose_id, std::span<const glm::mat4> transforms) = 0;
    virtual void ReleaseSkeletonPose(SkeletonPoseID pose_id) = 0;

    virtual DeformTextureID CreateDeformTexture(const DeformTextureDescriptor& desc) = 0;
    virtual void SetDeformTextureData(DeformTextureID deform_id, std::span<const glm::i8vec3> data) = 0;
    virtual void ReleaseDeformTexture(DeformTextureID deform_id) = 0;

    virtual ViewportID CreateViewport() = 0;
    virtual void DrawViewport(ViewportID viewport_id, Scene& scene, const CameraParams& camera,
                              const glm::u32vec2& size) = 0;
    virtual ViewportTextureHandle GetViewportNativeHandle(ViewportID viewport_id) = 0;
    virtual void ReleaseViewport(ViewportID viewport_id) = 0;

    virtual void Draw(Scene& scene, const CameraParams& camera) = 0;
    
    virtual ~Renderer() = default;

    // setup
    static bool IsGL(); // to setup window
    static void Init(SDL_Window* window);
    static Renderer& GetInstance();
    static void Uninit();

protected:
    glm::u32vec2 GetViewportSize() const;

protected:
    SDL_Window* const window_;
};

} // namespace gfx
