#include "modelview.hpp"

#include "utils/bufferput.hpp"

game::view::ModelView::ModelView(std::shared_ptr<const assets::Model> model) : model_(std::move(model))
{
    // init mesh flags
    mflags_ = 0;
    if (model_->GetSkeleton())
    {
        mflags_ = gfx::MF_SKELETAL;
    }

    CreateVA();
    CreateSurfaces();
}

std::shared_ptr<game::view::ModelView> game::view::ModelView::Load(const std::string& name)
{
    return std::make_shared<ModelView>(assets::AssetManager::GetInstance().Get<assets::Model>(name));
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

void game::view::ModelView::CreateVA()
{
    auto verts = model_->GetVertices();
    auto tris = model_->GetTriangles();

    // Generate VBO data
    std::vector<char> buffer;
    for (const auto& vert : verts)
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
    va->SetIndices(reinterpret_cast<const GLuint*>(tris.data()), tris.size() * 3U);

    va_ = std::move(va);
}

void game::view::ModelView::CreateSurfaces()
{
    for (auto surfaces = model_->GetSurfaces(); const auto& mdl_surface : surfaces)
    {
        gfx::Surface surface{};
        surface.va = va_;
        surface.first = mdl_surface.first_tri;
        surface.count = mdl_surface.num_tris;
        surface.mflags = mflags_;

        // texture
        if (!mdl_surface.texture_name.empty())
        {
            surface.texture = assets::AssetManager::GetInstance().Get<gfx::Texture>(mdl_surface.texture_name);
        }

        if (mdl_surface.two_sided)
            surface.sflags |= gfx::SF_2SIDED;

        if (mdl_surface.object_color)
            surface.sflags |= gfx::SF_OBJECT_COLOR;

        if (mdl_surface.object_color_mult)
            surface.sflags |= gfx::SF_OBJECT_COLOR_MULT;

        if (mdl_surface.multicolor)
            surface.sflags |= gfx::SF_MULTICOLOR;

        if (mdl_surface.blend)
            surface.sflags |= gfx::SF_BLEND;

        if (mdl_surface.blend_additive)
            surface.sflags |= gfx::SF_BLEND_ADDITIVE;

        if (mdl_surface.unlit)
            surface.sflags |= gfx::SF_UNLIT;

        if (mdl_surface.translucent)
            surface.sflags |= gfx::SF_TRANSLUCENT;

        surfaces_.emplace_back(std::move(surface));
    }
}
