#pragma once

#include "assets/effect.hpp"
#include "modelview.hpp"
#include "draw_args.hpp"
#include "audio/player.hpp"

namespace game::view
{

struct Particle
{
    std::shared_ptr<const assets::Effect> fx; // to keep resources alive

    gfx::Surface surface;

    // instance specific
    float time;
    glm::vec3 position;
    float rotation;
    float rotation_speed;
    float size;
    float size_speed;
    glm::vec3 velocity;
    float gravity;
    float lifetime;
    float fade_start;

    // for drawing
    glm::mat4 matrix;
    glm::vec4 color;
};

class ParticleEmitter
{
public:
    ParticleEmitter(audio::Player* audioplayer);

    void Update(float delta_time);
    void Draw(const DrawArgs& args);

    void Emit(const std::shared_ptr<const assets::Effect>& fx, const glm::vec3& pos, const glm::vec3& dir);

private:
    audio::Player* audioplayer_;

    std::shared_ptr<const ModelView> quad_model_; // to steal quad VAO from
    
    std::vector<Particle> particles_;

};

}