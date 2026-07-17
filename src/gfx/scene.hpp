#pragma once

#include "draw_list.hpp"
#include "frustum.hpp"
#include "environment.hpp"

namespace gfx
{

enum DrawPassType
{
    DRAW_PASS_MAIN,
    DRAW_PASS_SHADOW_MAP,
    //DRAW_PASS_CSM,
};

struct DrawContext
{
    DrawList& dlist;
    DrawPassType pass;
    glm::vec3 eye;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 view_proj;
    Frustum frustum;
    float min_distance;
    float max_distance;
    glm::u32vec2 viewport_size;

    DrawContext(DrawList& dlist, DrawPassType pass, const glm::vec3& eye, const glm::mat4& view, const glm::mat4& proj,
                float min_distance, float max_distance, const glm::u32vec2& viewport_size)
        : dlist(dlist), pass(pass), eye(eye), view(view), proj(proj), view_proj(proj * view), frustum(view_proj),
          min_distance(min_distance), max_distance(max_distance), viewport_size(viewport_size)
    {
    }
};

class Scene
{
public:
    virtual void Draw(const DrawContext& ctx) = 0;

    virtual Environment GetSceneEnvironment() = 0;
    virtual float GetMapChunkSize() = 0;

};


}