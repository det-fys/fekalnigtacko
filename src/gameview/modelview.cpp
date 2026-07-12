#include "modelview.hpp"

#include "utils/bufferput.hpp"

game::view::ModelView::ModelView(std::shared_ptr<const assets::Model> model) : model_(std::move(model))
{
    CreateMesh();
    CreateSurfaces();
}

std::shared_ptr<game::view::ModelView> game::view::ModelView::Load(const std::string& name)
{
    return std::make_shared<ModelView>(assets::AssetManager::GetInstance().Get<assets::Model>(name));
}

void game::view::ModelView::Draw(const gfx::DrawContext& ctx, const glm::mat4& matrix,
                                 std::span<const glm::vec4> colors, gfx::SkeletonPoseID pose_id,
                                 gfx::DeformTextureID deform_id, uint32_t surface_mask) const
{
    auto d = ctx.eye - glm::vec3(matrix[3]);

    gfx::DrawSurfaceCmd cmd{};
    cmd.matrix = &matrix;
    cmd.colors = colors;
    cmd.pose = pose_id;
    cmd.deform_tex = deform_id;
    cmd.dist = glm::dot(d, d);

    auto& dlist = ctx.dlist;

    for (size_t i = 0; i < surfaces_.size(); ++i)
    {
        if ((surface_mask & (1 << i)) == 0)
            continue;
        
        const auto& surface = surfaces_[i];

        cmd.mesh = mesh_->GetID();
        cmd.tri_offset = surface.tri_offset;
        cmd.tri_count = surface.tri_count;
        cmd.material = surface.material->GetID();
        dlist.AddSurface(cmd);
    }
}

void game::view::ModelView::CreateMesh()
{
    bool skeletal = model_->GetSkeleton().get() != nullptr;

    auto& verts = model_->GetVertices();
    auto& tris = model_->GetTriangles();

    gfx::MeshDescriptor desc{};
    desc.attributes = gfx::MESH_VERTEX_ATTR_POSITION | gfx::MESH_VERTEX_ATTR_NORMAL | gfx::MESH_VERTEX_ATTR_UV0 |
                      (skeletal ? gfx::MESH_VERTEX_ATTR_BONE_DATA : 0);
    desc.use_index_buffer = true;

    mesh_.emplace(desc);

    gfx::MeshVertexData vertex_data{};
    vertex_data.count = verts.positions.size();
    vertex_data.position = verts.positions;
    vertex_data.normal = verts.normals;
    vertex_data.uv0 = verts.uvs;
    vertex_data.bone = verts.bones;
    mesh_->SetVertexData(vertex_data);

    gfx::MeshTriangleData triangle_data{};
    triangle_data.triangles = std::span<const gfx::MeshTriangle>{reinterpret_cast<const gfx::MeshTriangle*>(tris.data()), tris.size()};
    mesh_->SetTriangleData(triangle_data);
}

void game::view::ModelView::CreateSurfaces()
{
    for (auto surfaces = model_->GetSurfaces(); const auto& mdl_surface : surfaces)
    {
        gfx::MaterialInfo material_info{};
        material_info.properties = mdl_surface.properties;

        // texture
        if (!mdl_surface.texture_name.empty())
        {
            material_info.texture = assets::AssetManager::GetInstance().Get<gfx::Texture>(mdl_surface.texture_name);
        }

        ModelViewSurface surface{};
        surface.material = std::make_shared<gfx::Material>(material_info);
        surface.tri_offset = mdl_surface.tri_offset;
        surface.tri_count = mdl_surface.tri_count;

        surfaces_.emplace_back(std::move(surface));
    }
}
