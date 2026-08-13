#pragma once

#include "utils/defs.hpp"

#include "scene.hpp"
#include "camera.hpp"

#include "viewport_desc.hpp"

namespace gfx
{

class Viewport
{
public:
    Viewport();
    DELETE_COPY_MOVE(Viewport);

    void Draw(Scene& scene, const CameraParams& camera, const glm::u32vec2& size);

    ViewportTextureHandle GetNativeHandle() const;

    static bool NeedsYFlip();

    ~Viewport();

private:
    ViewportID id_;
};



}