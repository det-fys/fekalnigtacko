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
    
    InitModels();
    InitObjsAndCollisions();
}

int game::view::MapInstanceView::GetLoadingPercent() const
{
    if (!loader_)
        return 100;

    return loader_->GetPercent();
}

void game::view::MapInstanceView::Draw(const game::view::DrawArgs& args)
{
    if (!map_)
        return;

    const auto& basemodel = map_->GetBaseModel();

    if (!basemodel_view_)
        return;

    const float chunk_radius = glm::length(glm::vec2(map_->GetChunkSize()));

    const float min_dist = glm::max(args.ctx.min_distance - chunk_radius, 0.0f);
    const float min_dist2 = min_dist * min_dist;

    const float max_dist = args.ctx.max_distance + chunk_radius;
    const float max_dist2 = max_dist * max_dist;

    const auto& frustum_aabb = args.ctx.frustum.GetAABB();

    static std::vector<uint32_t> visible_chunks;
    map_->GetOverlappingChunks(frustum_aabb, visible_chunks);

    const auto& chunks = map_->GetChunks();

    for (auto chunk_idx : visible_chunks)
    {
        const auto& chunk = chunks[chunk_idx];
        
        glm::vec3 center = (chunk.aabb.min + chunk.aabb.max) * 0.5f;
        auto dist2 = glm::distance2(args.ctx.eye, center);

        if (dist2 < min_dist2 || dist2 > max_dist2)
            continue;
        
        if (!args.ctx.frustum.IsAABBVisible(chunk.aabb))
            continue;

        DrawChunk(args, chunk);
    }
}

void game::view::MapInstanceView::Update()
{
    ++update_frame_;
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

void game::view::MapInstanceView::InitModels()
{
    basemodel_view_ = assets::AssetManager::GetInstance().Get<ModelView>(map_->GetBaseModel()->GetAssetName());

    const auto& obj_models = map_->GetObjModels();
    obj_models_.reserve(obj_models.size());
    for (const auto& obj_model : obj_models)
    {
        auto& mapmodel = obj_models_.emplace_back();
        mapmodel.model = assets::AssetManager::GetInstance().Get<ModelView>(obj_model->GetAssetName());
        InitModel(mapmodel);
    }
}

void game::view::MapInstanceView::InitModel(MapModel& mapmodel)
{
    const auto& model = *mapmodel.model->GetModel();
    auto special = model.GetParam("special");
    if (!special)
        return;

    if (*special == "nightlight")
    {
        mapmodel.special = MMS_NIGHTLIGHT;

        auto light0 = model.GetLocation("light0");
        if (!light0)
        {
            throw std::runtime_error("Map model: nightlight without light0 location");
        }

        mapmodel.light.position = light0->position;
        mapmodel.light.dir = light0->rotation * glm::vec3(0.0f, 0.0f, 1.0f);
        
        mapmodel.light.radius = 15.0f;
        float inner_deg = 25.0f;
        float outer_deg = 70.0f;
        mapmodel.light.color = glm::vec3(1.0f);
        float color_mult = 1.0f;

        model.GetParamFloat("light0_radius", mapmodel.light.radius);
        model.GetParamFloat("light0_angle_inner", inner_deg);
        model.GetParamFloat("light0_angle_outer", outer_deg);
        model.GetParamFloat("light0_color_r", mapmodel.light.color.r);
        model.GetParamFloat("light0_color_g", mapmodel.light.color.g);
        model.GetParamFloat("light0_color_b", mapmodel.light.color.b);
        model.GetParamFloat("light0_color_mult", color_mult);

        mapmodel.light.color *= color_mult;

        mapmodel.light.cos_inner = glm::radians(inner_deg);
        mapmodel.light.cos_outer = glm::radians(outer_deg);
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
        obj_cols_[i] = std::make_unique<MapObjectCollisionView>(world_, map_->GetObjModels()[objs[i].model_idx],
                                                                objs[i].node.local);
        obj_cols_[i]->SetEnabled(objs_visible_[i]);
    }
}

void game::view::MapInstanceView::DrawChunk(const game::view::DrawArgs& args, const assets::Chunk& chunk)
{
    // make basemodel not cast shadows
    if (args.ctx.pass != gfx::DRAW_PASS_SHADOW_MAP)
    {
        auto surfaces = basemodel_view_->GetSurfaces();

        gfx::DrawSurfaceCmd cmd{};
        cmd.mesh = basemodel_view_->GetMesh().GetID();
        cmd.map_chunk_hash = chunk.light_hash;

        auto& dlist = args.ctx.dlist;
        for (const auto& surface_range : chunk.surfaces)
        {
            auto& surface = surfaces[surface_range.idx];
            cmd.material = surface.material->GetID();
            cmd.tri_offset = surface.tri_offset + surface_range.first;
            cmd.tri_count = surface_range.count;
            dlist.AddSurface(cmd);
        }
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

        if (!args.ctx.frustum.IsAABBVisible(obj.aabb))
            continue;

        DrawObj(args, obj_models_[obj.model_idx], obj.node.matrix);
    }
}

void game::view::MapInstanceView::DrawObj(const DrawArgs& args, MapModel& mapmodel, const glm::mat4& matrix)
{
    if (mapmodel.special == MMS_NONE)
    {
        // no special effect, just draw
        mapmodel.model->Draw(args.ctx, matrix, {});
        return;
    }

    UpdateModelSpecial(mapmodel);

    if (mapmodel.special == MMS_NIGHTLIGHT)
    {
        mapmodel.model->Draw(args.ctx, matrix, { mapmodel.colors.data(), 1 });

        if (mapmodel.light_on)
        {
            glm::vec3 light_pos = matrix * glm::vec4(mapmodel.light.position, 1.0f);
            glm::vec3 light_dir = glm::mat3(matrix) * mapmodel.light.dir;

            args.ctx.dlist.AddSpotLight(light_pos, mapmodel.light.color, mapmodel.light.radius, light_dir,
                                        mapmodel.light.cos_inner, mapmodel.light.cos_outer, gfx::LF_SHADOWS);

            args.ctx.dlist.AddCorona(light_pos, light_dir, mapmodel.light.color, 1.0f);
        }
    }
}

void game::view::MapInstanceView::UpdateModelSpecial(MapModel& mapmodel)
{
    if (mapmodel.update_frame == update_frame_)
    {
        return; // already updated
    }

    mapmodel.update_frame = update_frame_;

    if (mapmodel.special == MMS_NIGHTLIGHT)
    {
        if (daytime_ < 6.0f || daytime_ > 18.0f)
        {
            // night
            mapmodel.colors[0] = glm::vec4(glm::normalize(mapmodel.light.color) * 1.5f, 1.0f);
            mapmodel.light_on = true;
        }
        else
        {
            // day
            mapmodel.colors[0] = glm::vec4(0.9f, 0.9f, 0.9f, 0.0f);
            mapmodel.light_on = false;
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
