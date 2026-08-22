#include "object.hpp"

#include "map_project.hpp"

edit::Object::Object(Project& project) : project_(&project) {}

void edit::Object::InvalidateChunks(const AABB3& aabb)
{
    const float margin = 20.0f; // Add a margin to ensure neighboring chunks are also invalidated

    auto& map_cfg = project_->GetMapConfig();
    auto [min_chunk, max_chunk] = mg::GetChunkRange(map_cfg, AABB2(aabb.min - margin, aabb.max + margin));

    for (int chunk_y = min_chunk.y; chunk_y <= max_chunk.y; ++chunk_y)
    {
        for (int chunk_x = min_chunk.x; chunk_x <= max_chunk.x; ++chunk_x)
        {
            project_->InvalidateChunk(glm::ivec2(chunk_x, chunk_y));
        }
    }
}
