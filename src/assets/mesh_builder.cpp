#include "mesh_builder.hpp"

#include <stdexcept>

#include "utils/bufferput.hpp"

assets::MeshBuilder::MeshBuilder(gfx::MeshFlags mflags) : mflags_(mflags)
{
    mesh_ = std::make_shared<Mesh>();
}

void assets::MeshBuilder::BeginSurface(gfx::SurfaceFlags sflags, const std::string& name, std::shared_ptr<const gfx::Texture> texture)
{
    FinalizeSurface();

    gfx::Surface surface;
    surface.sflags = sflags;
    surface.texture = std::move(texture);
    surface.first = tris_.size();
    mesh_->surfaces.push_back(surface);

    mesh_->surface_names[name] = mesh_->surfaces.size() - 1;
}

void assets::MeshBuilder::AddVertex(const MeshVertex& v)
{
    verts_.push_back(v);
}

void assets::MeshBuilder::AddTriangle(const MeshTriangle& t)
{
    if (!mesh_ || mesh_->surfaces.size() == 0)
    {
        throw std::runtime_error("Cannot add triangle without a surface. Call BeginSurface() first.");
    }

    tris_.push_back(t);
}

static int GetVertexAttrFlags(gfx::MeshFlags mflags)
{
    int attrs = gfx::VA_POSITION | gfx::VA_NORMAL | gfx::VA_UV;

    if (mflags & gfx::MF_LIGHTMAP_UV)
    {
        attrs |= gfx::VA_LIGHTMAP_UV;
    }

    if (mflags & gfx::MF_SKELETAL)
    {
        attrs |= gfx::VA_BONE_INDICES | gfx::VA_BONE_WEIGHTS;
    }

    return attrs;
}

void assets::MeshBuilder::Build()
{
    // Finalize last surface
    FinalizeSurface();

    // Generate VBO data
    std::vector<char> buffer;
    for (const MeshVertex& vert : verts_)
    {
        BufferPut(buffer, vert.pos);
        BufferPut(buffer, vert.normal);
        BufferPut(buffer, vert.uv);

        if (mflags_ & gfx::MF_LIGHTMAP_UV)
        {
            BufferPut(buffer, vert.lightmap_uv);
        }

        if (mflags_ & gfx::MF_SKELETAL)
        {
            for (int i = 0; i < 4; ++i)
            {
                BufferPut(buffer, static_cast<int32_t>(vert.bones[i].bone_index));
            }

            for (int i = 0; i < 4; ++i)
            {
                BufferPut(buffer, vert.bones[i].weight);
            }
        }
    }

    // populate VA
    std::shared_ptr<gfx::VertexArray> va =
        std::make_shared<gfx::VertexArray>(GetVertexAttrFlags(mflags_), gfx::VF_CREATE_EBO);
    va->SetVBOData(buffer.data(), buffer.size());
    va->SetIndices(reinterpret_cast<const GLuint*>(tris_.data()), tris_.size() * 3U);

    // Assign VA to surfaces & set mesh flags
    for (gfx::Surface& surface : mesh_->surfaces)
    {
        surface.va = va;
        surface.mflags = mflags_;
    }
}

void assets::MeshBuilder::FinalizeSurface()
{
    if (mesh_->surfaces.size() > 0)
    {
        // finalize previous surface
        gfx::Surface& prev_surface = mesh_->surfaces.back();
        prev_surface.count = tris_.size() - prev_surface.first;
    }
}
