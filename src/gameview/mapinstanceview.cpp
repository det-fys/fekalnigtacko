#include "mapinstanceview.hpp"
#include "collision/dynamicsworld.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

game::view::MapInstanceView::MapInstanceView(collision::DynamicsWorld& world, const std::string& map_name) : world_(world)
{
    loader_ = std::make_unique<assets::MapLoader>("data/" + map_name + ".map");
}

void game::view::MapInstanceView::LoadNext()
{
    if (IsLoaded())
        return;

    if (loader_->Next())
        return;

    // just loaded
    map_ = loader_->GetMap();
    loader_.reset();
    
    InitObjsAndCollisions();
}

int game::view::MapInstanceView::GetLoadingPercent() const
{
    if (!loader_)
        return 100;

    return loader_->GetPercent();
}

void game::view::MapInstanceView::Draw(const game::view::DrawArgs& args) const
{
    if (!map_)
        return;

    const auto& basemodel = map_->GetBaseModel();

    if (!basemodel || !basemodel->GetMesh())
        return;

    const auto& mesh = *basemodel->GetMesh();

    const float max_dist = args.render_distance + 200.0f;
    const float max_dist2 = max_dist * max_dist;

    for (const auto& chunks = map_->GetChunks(); const auto& chunk : chunks)
    {
        glm::vec3 center = (chunk.aabb.min + chunk.aabb.max) * 0.5f;
        if (glm::distance2(args.eye, center) > max_dist2)
            continue;

        if (!args.frustum.IsAABBVisible(chunk.aabb))
            continue;

        DrawChunk(args, mesh, chunk);
    }

}

void game::view::MapInstanceView::EnableObj(net::ObjNum num, bool enable)
{
    size_t i = static_cast<size_t>(num);

    // map may be not loaded yet, in that case make the visible flag fit
    if (i >= objs_visible_.size())
        objs_visible_.resize(i + 1, true);

    objs_visible_[i] = enable;

    if (i < obj_cols_.size())
    {
        obj_cols_[i]->SetEnabled(enable);
    }
}

void game::view::MapInstanceView::InitObjsAndCollisions()
{
    // add basemodel col
    const auto& basemodel = map_->GetBaseModel();
    if (basemodel)
    {
        Transform identity;
        basemodel_col_ = std::make_unique<MapObjectCollisionView>(world_, basemodel, identity);
        basemodel_col_->SetEnabled(true);
    }

    auto& objs = map_->GetStaticObjects();
    objs_visible_.resize(objs.size(), true);
    obj_cols_.resize(objs.size());

    for (size_t i = 0; i < objs.size(); ++i)
    {
        obj_cols_[i] = std::make_unique<MapObjectCollisionView>(world_, objs[i].model, objs[i].node.local);
        obj_cols_[i]->SetEnabled(objs_visible_[i]);
    }
}

void game::view::MapInstanceView::DrawChunk(const game::view::DrawArgs& args, const assets::Mesh& basemesh,
                                            const assets::Chunk& chunk) const
{
    for (const auto& surface_range : chunk.surfaces)
    {
        auto& surface = basemesh.surfaces[surface_range.idx];

        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &surface;
        cmd.first = surface_range.first;
        cmd.count = surface_range.count;
        args.dlist.AddSurface(cmd);
    }

    const auto& objs = map_->GetStaticObjects();

    for (size_t i = 0; i < chunk.num_objs; ++i)
    {
        size_t abs_i = chunk.first_obj + i;

        if (abs_i >= objs.size())
            continue;

        if (!objs_visible_[abs_i])
            continue;

        const auto& obj = objs[abs_i];

        if (!obj.model || !obj.model->GetMesh())
            continue;

        if (!args.frustum.IsAABBVisible(obj.aabb))
            continue;

        const auto& surfaces = obj.model->GetMesh()->surfaces;

        for (const auto& surface : surfaces)
        {
            gfx::DrawSurfaceCmd cmd;
            cmd.surface = &surface;
            cmd.matrices = &obj.node.matrix;
            // cmd.color_mod = glm::vec4(obj.color, 1.0f);
            args.dlist.AddSurface(cmd);
        }
    }
}

game::view::MapObjectCollisionView::MapObjectCollisionView(collision::DynamicsWorld& world,
                                                           std::shared_ptr<const assets::Model> model,
                                                           const Transform& trans) :
    world_(world), model_(std::move(model))
{
    auto cshape = model_->GetColShape();
    auto cmesh = model_->GetColMesh();

    btVector3 local_inertia(0, 0, 0);

    if (cshape)
    {
        body_ = std::make_unique<btRigidBody>(
            btRigidBody::btRigidBodyConstructionInfo(0.0f, nullptr, cshape, local_inertia));
    }
    else if (cmesh)
    {
        body_ = std::make_unique<btRigidBody>(
            btRigidBody::btRigidBodyConstructionInfo(0.0f, nullptr, cmesh->GetShape(), local_inertia));
    }

    auto offset_trans = trans;
    offset_trans.position += trans.rotation * model_->GetColOffset();

    body_->setWorldTransform(offset_trans.ToBtTransform());
}

void game::view::MapObjectCollisionView::SetEnabled(bool enabled)
{
    if (enabled == enabled_)
        return;

    auto& bt_world = world_.GetBtWorld();

    if (enabled)
    {
        bt_world.addRigidBody(body_.get());
    }
    else
    {
        bt_world.removeRigidBody(body_.get());
    }

    enabled_ = enabled;
}

game::view::MapObjectCollisionView::~MapObjectCollisionView()
{
    SetEnabled(false); // delete from world if there
}
