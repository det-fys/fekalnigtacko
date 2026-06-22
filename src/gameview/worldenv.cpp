#include "worldenv.hpp"

#include "assets/cache.hpp"

game::view::WorldEnv::WorldEnv()
{
    sunmodel_ = assets::CacheManager::GetModel("data/env_sun.mdl");
    moonmodel_ = assets::CacheManager::GetModel("data/env_moon.mdl");
    halfspheremodel_ = assets::CacheManager::GetModel("data/env_halfsphere.mdl");
}

static const glm::vec4 color1(1.0f);

static const game::view::WorldEnvKeyframe env_kfs[] = {
    game::view::WorldEnvKeyframe{
        0.0f, // time
        glm::vec3(0.1f, 0.15f, 0.3f), // clear color
        glm::vec3(0.4f, 0.4f, 0.4f), // ambient color
        glm::vec3(0.5f, 0.8f, 1.0f) * 0.4f, // sun color
        -1.0f, // sun light source
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // sun disc color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // sun halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // moon disc color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.1f), // moon halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.00002f), // fog
    },

    game::view::WorldEnvKeyframe{
        5.0f, // time
        glm::vec3(0.15f, 0.2, 0.3f), // clear color
        glm::vec3(0.5f, 0.5f, 0.5f) * 1.0f, // ambient color
        glm::vec3(0.5f, 0.8f, 1.0f) * 0.4f, // sun color
        -1.0f, // sun light source
        glm::vec4(1.0f, 0.9f, 0.8f, 1.0f), // sun disc color
        glm::vec4(1.0f, 0.8f, 0.6f, 0.2f), // sun halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // moon disc color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // moon halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.00001f), // fog
    },

    game::view::WorldEnvKeyframe{
        6.0f, // time
        glm::vec3(0.65f, 0.8f, 1.0f) * 0.9f, // clear color
        glm::vec3(0.5f, 0.5f, 0.5f) * 1.1f, // ambient color
        glm::vec3(1.0f, 0.95f, 0.7f) * 0.8f, // sun color
        1.0f, // sun light source
        glm::vec4(1.0f, 0.9f, 0.8f, 1.0f), // sun disc color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.4f), // sun halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // moon disc color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // moon halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.00001f), // fog
    },

    game::view::WorldEnvKeyframe{
        12.0f, // time
        glm::vec3(0.6f, 0.8f, 1.0f) * 1.2f, // clear color
        glm::vec3(0.5f, 0.5f, 0.45f) * 1.1f, // ambient color
        glm::vec3(1.0f, 0.95f, 0.7f) * 0.9f, // sun color
        1.0f, // sun light source
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // sun disc color
        glm::vec4(1.0f, 0.9f, 0.9f, 0.5f), // sun halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // moon disc color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // moon halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.000001f), // fog
    },

    game::view::WorldEnvKeyframe{
        17.0f, // time
        glm::vec3(0.6f, 0.8f, 1.0f) * 1.2f, // clear color
        glm::vec3(0.5f, 0.5f, 0.4f) * 1.4f, // ambient color
        glm::vec3(1.0f, 0.95f, 0.7f) * 0.9f, // sun color
        1.0f, // sun light source
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // sun disc color
        glm::vec4(1.0f, 0.85f, 0.6f, 0.7f), // sun halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // moon disc color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // moon halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.000001f), // fog
    },

    game::view::WorldEnvKeyframe{
        17.5f, // time
        glm::vec3(0.6f, 0.8f, 1.0f) * 1.1f, // clear color
        glm::vec3(0.8f, 0.6f, 0.4f) * 1.1f, // ambient color
        glm::vec3(1.0f, 0.8f, 0.6f) * 0.8f, // sun color
        1.0f, // sun light source
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // sun disc color
        glm::vec4(1.0f, 0.7f, 0.5f, 1.0f), // sun halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.2f), // moon disc color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.1f), // moon halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.000002f), // fog
    },

    game::view::WorldEnvKeyframe{
        18.0f, // time
        glm::vec3(0.5f, 0.7f, 1.0f) * 0.7f, // clear color
        glm::vec3(0.8f, 0.6f, 0.4f) * 0.8f, // ambient color
        glm::vec3(1.0f, 0.7f, 0.6f) * 0.2f, // sun color
        1.0f, // sun light source
        glm::vec4(1.0f, 0.8f, 0.7f, 1.0f), // sun disc color
        glm::vec4(1.0f, 0.7f, 0.5f, 1.0f), // sun halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.2f), // moon disc color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.1f), // moon halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.000005f), // fog
    },

    game::view::WorldEnvKeyframe{
        19.0f, // time
        glm::vec3(0.1f, 0.15f, 0.3f), // clear color
        glm::vec3(0.4f, 0.4f, 0.4f), // ambient color
        glm::vec3(1.0f, 0.7f, 0.6f) * 0.2f, // sun color
        0.0f, // sun light source
        glm::vec4(1.0f, 0.5f, 0.3f, 1.0f), // sun disc color
        glm::vec4(1.0f, 0.5f, 0.2f, 0.8f), // sun halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // moon disc color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.1f), // moon halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.00001f), // fog
    },

    game::view::WorldEnvKeyframe{
        20.0f, // time
        glm::vec3(0.1f, 0.15f, 0.3f), // clear color
        glm::vec3(0.4f, 0.4f, 0.4f), // ambient color
        glm::vec3(0.5f, 0.8f, 1.0f) * 0.4f, // sun color
        -1.0f, // sun light source
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // sun disc color
        glm::vec4(1.0f, 0.5f, 0.2f, 0.0f), // sun halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // moon disc color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.1f), // moon halfsphere color
        glm::vec4(1.0f, 1.0f, 1.0f, 0.00003f), // fog
    },


};

void game::view::WorldEnv::Draw(const DrawArgs& args)
{
    const WorldEnvKeyframe *kf1, *kf2;

    if (daytime_ < 0.0f)
        daytime_ = 0.0f;

    const size_t num_kfs = sizeof(env_kfs) / sizeof(WorldEnvKeyframe);
    for (size_t i = 0; i < num_kfs; ++i)
    {
        if (daytime_ >= env_kfs[i].daytime)
        {
            kf1 = &env_kfs[i];
            kf2 = &env_kfs[(i + 1) % num_kfs];
        }
    }
    
    float t2 = kf2->daytime;
    if (t2 < daytime_)
        t2 += 24.0f;

    float t = (daytime_ - kf1->daytime) / (t2 - kf1->daytime);


    // args.env.clear_color = glm::vec3(0.1f, 0.15f, 0.3f);
    // args.env.ambient_light = glm::vec3(0.4f, 0.4f, 0.4f);
    // args.env.sun_color = glm::vec3(0.5f, 0.8f, 1.0f) * 0.4f;
    // args.env.sun_direction = glm::normalize(glm::vec3(1.0f, 1.0f, -1.0f));

    args.env.clear_color = glm::mix(kf1->clear_color, kf2->clear_color, t);
    args.env.ambient_light = glm::mix(kf1->ambient_color, kf2->ambient_color, t);
    args.env.sun_color = glm::mix(kf1->sun_color, kf2->sun_color, t);
    // args.env.fog = glm::mix(kf1->fog, kf2->fog, t);
    args.env.fog = glm::vec4(args.env.clear_color, glm::mix(kf1->fog.a, kf2->fog.a, t));

    float dist = args.farplane * 0.5f;

    // std::cout<<daytime_<<std::endl;


    float sun_angle = (daytime_ - 12.0f) * glm::two_pi<float>() / 24.0f;

    sun_color_ = glm::mix(kf1->sun_disc_color, kf2->sun_disc_color, t);
    sun_matrix_ = glm::mat4(1.0f);
    sun_matrix_ = glm::translate(sun_matrix_, args.eye);
    sun_matrix_ = glm::scale(sun_matrix_, glm::vec3(dist));
    sun_matrix_ = glm::rotate(sun_matrix_, 0.3f, glm::vec3(1.0f, 0.0f, 0.0f));
    sun_matrix_ = glm::rotate(sun_matrix_, sun_angle, glm::vec3(0.0f, 1.0f, 0.0f));
    sun_matrix_ = glm::translate(sun_matrix_, glm::vec3(0.0f, 0.0f, 1.0f));
    sun_matrix_ = glm::scale(sun_matrix_, glm::vec3(0.7f));
    
    sun_halfsphere_color_ = glm::mix(kf1->sun_halfsphere_color, kf2->sun_halfsphere_color, t);
    sun_halfsphere_matrix_ = glm::scale(sun_matrix_, glm::vec3(1.0f, 1.0f, 1.5f));
    
    moon_color_ = glm::mix(kf1->moon_disc_color, kf2->moon_disc_color, t);
    moon_matrix_ = glm::mat4(1.0f);
    moon_matrix_ = glm::translate(moon_matrix_, args.eye);
    moon_matrix_ = glm::scale(moon_matrix_, glm::vec3(dist));
    moon_matrix_ = glm::rotate(moon_matrix_, sun_angle + glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    moon_matrix_ = glm::translate(moon_matrix_, glm::vec3(0.0f, 0.0f, 1.0f));
    moon_matrix_ = glm::scale(moon_matrix_, glm::vec3(0.6f));
    
    moon_halfsphere_color_ = glm::mix(kf1->moon_halfsphere_color, kf2->moon_halfsphere_color, t);
    moon_halfsphere_matrix_ = glm::scale(moon_matrix_, glm::vec3(1.1f, 1.1f, 1.5f));
    
    float sun_dir = glm::mix(kf1->sun_dir, kf2->sun_dir, t);

    if (sun_dir >= 0.0f)
    {
        args.env.sun_direction = -glm::normalize(glm::vec3(sun_matrix_[2]));
        args.env.sun_color *= sun_dir;
    }
    else
    {
        args.env.sun_direction = -glm::normalize(glm::vec3(moon_matrix_[2]));
        args.env.sun_color *= -sun_dir;
    }

    DrawEnvModel(args, *halfspheremodel_, moon_halfsphere_matrix_, moon_halfsphere_color_, dist);
    DrawEnvModel(args, *halfspheremodel_, sun_halfsphere_matrix_, sun_halfsphere_color_, dist - 1.0f);
    DrawEnvModel(args, *sunmodel_, moon_matrix_, moon_color_, dist - 2.0f);
    DrawEnvModel(args, *sunmodel_, sun_matrix_, sun_color_, dist - 3.0f);
}

void game::view::WorldEnv::DrawEnvModel(const DrawArgs& args, const assets::Model& model, const glm::mat4& matrix,
                                        const glm::vec4& color, float dist)
{
    const auto& mesh = model.GetMesh();

    for (const auto& surface : mesh->surfaces)
    {
        gfx::DrawSurfaceCmd cmd{};
        cmd.surface = &surface;
        cmd.matrices = &matrix;
        cmd.color = &color;
        cmd.dist = dist;

        args.dlist.AddSurface(cmd);
    }
}
