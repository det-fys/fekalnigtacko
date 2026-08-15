#pragma once

#include "modelview.hpp"
#include "gfx/scene.hpp"

namespace game::view
{

struct WorldEnvKeyframe
{
    float daytime;
    glm::vec3 clear_color;
    glm::vec3 ambient_color;
    glm::vec3 sun_color;
    float sun_dir; // 1=sun 0=none -1=moon
    glm::vec4 sun_disc_color;
    glm::vec4 sun_halfsphere_color;
    glm::vec4 moon_disc_color;
    glm::vec4 moon_halfsphere_color;
    glm::vec4 fog;
};

class WorldEnv
{
public:
    WorldEnv();

    void Draw(const gfx::DrawContext& ctx);

    void SetDayTime(float daytime) { daytime_ = glm::mod(daytime, 24.0f); }
    float GetDayTime() const { return daytime_; }

    const gfx::Environment& GetEnv() const { return env_; }

private:
    void DrawEnvModel(const gfx::DrawContext& ctx, const ModelView& model, const glm::mat4& matrix,
                      const glm::vec4& color, float dist);

private:
    float daytime_ = 0.0f; // 0 - 24

    std::shared_ptr<const ModelView> sunmodel_;
    std::shared_ptr<const ModelView> moonmodel_;
    std::shared_ptr<const ModelView> halfspheremodel_;

    glm::mat4 sun_matrix_;
    glm::vec4 sun_color_;
    
    glm::mat4 moon_matrix_;
    glm::vec4 moon_color_;

    glm::mat4 sun_halfsphere_matrix_;
    glm::vec4 sun_halfsphere_color_;

    glm::mat4 moon_halfsphere_matrix_;
    glm::vec4 moon_halfsphere_color_;

    gfx::Environment env_{};
};



}