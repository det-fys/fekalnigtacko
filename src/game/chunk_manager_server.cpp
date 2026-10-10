#include "chunk_manager_server.hpp"

#include "collision/object_info.hpp"
#include "world.hpp"

game::ServerChunkManager::ServerChunkManager(std::shared_ptr<const mg::ResourceSet> res, const mg::MapConfig& map_cfg,
                                             World& world)
    : mg::ChunkManager(res, map_cfg), world_(world)
{
}

game::ServerChunkManager::~ServerChunkManager()
{
    // remove collisions
    for (auto& [coord, chunk_data] : chunks_)
    {
        RemoveChunkCollision(chunk_data);
    }
}

void game::ServerChunkManager::GetChunkParams(mg::ChunkParams& out_params)
{
    out_params.collision_only = true;

    // TODO
}

void game::ServerChunkManager::OnChunkGenerated(const mg::ChunkGenData& gen_data)
{
    auto& chunk_data = chunks_[gen_data.coord];
    chunk_data.coord = gen_data.coord;
    chunk_data.center = gen_data.center;

    RemoveChunkCollision(chunk_data); // ensure old one is removed first
    chunk_data.col_mesh = gen_data.model->GetColMeshShared();
    CreateChunkCollision(chunk_data);
}

void game::ServerChunkManager::OnChunkRemoved(const glm::ivec2& coord)
{
    auto it = chunks_.find(coord);
    if (it == chunks_.end())
        return;

    RemoveChunkCollision(it->second);
    chunks_.erase(it);
}

void game::ServerChunkManager::CreateChunkCollision(ServerChunkData& chunk_data)
{
    if (!chunk_data.col_mesh)
        return;

    // TODO: make class or use MapCollisionObject

    btVector3 local_inertia(0, 0, 0);

    chunk_data.col_body = std::make_unique<btRigidBody>(
        btRigidBody::btRigidBodyConstructionInfo(0.0f, nullptr, chunk_data.col_mesh->GetShape(), local_inertia));

    // trans
    FarTransform trans{};
    trans.position = chunk_data.center;

    chunk_data.col_body->setWorldTransform(trans.ToBtTransform());

    int col_mask = ~collision::OG_STATIC;

    collision::SetObjectInfo(chunk_data.col_body.get(), collision::OT_UNDEFINED, col_mask, nullptr);
    world_.GetBtWorld().addRigidBody(chunk_data.col_body.get(), collision::OG_STATIC, col_mask);
}

void game::ServerChunkManager::RemoveChunkCollision(ServerChunkData& chunk_data)
{
    if (!chunk_data.col_body)
        return;

    world_.GetBtWorld().removeRigidBody(chunk_data.col_body.get());
    chunk_data.col_body.reset();
}
