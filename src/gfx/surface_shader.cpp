#include "surface_shader.hpp"

#include <string>

#include "shader_common.hpp"

std::unique_ptr<gfx::Shader> gfx::CreateSurfaceShader(SurfaceRenderFlags flags, SurfaceShaderInputFlags& input_flags)
{
    std::string vert_attributes, vert_uniforms, vert_outs, vert_funcs, vert_pos_calc, vert_main;
    std::string frag_ins, frag_uniforms, frag_funcs, frag_main;

    // default unlit untextured skeleton

    vert_attributes = R"GLSL(
        layout (location = 0) in vec3 a_pos;
        layout (location = 1) in vec3 a_normal;
        layout (location = 2) in vec4 a_color;
        layout (location = 3) in vec2 a_uv;
    )GLSL";

    vert_uniforms = R"GLSL(
        uniform mat4 u_view_proj;
        uniform mat4 u_model;
    )GLSL";

    vert_outs = R"GLSL(
        out vec3 v_color;
    )GLSL";
    
    vert_pos_calc = R"GLSL(
        vec4 world_pos = u_model * vec4(a_pos, 1.0);
        vec3 world_normal = normalize(mat3(u_model) * a_normal);
    )GLSL";

    vert_main = R"GLSL(
        gl_Position = u_view_proj * world_pos;
        v_color = a_color.rgb;
    )GLSL";

    frag_ins = R"GLSL(
        in vec3 v_color;
    )GLSL";

    frag_main = R"GLSL(
        o_color = vec4(1.0);
    )GLSL";

    input_flags = 0;

    // color texture
    if (flags & SRF_TEXTURE)
    {
        vert_outs += "out vec2 v_uv;\n";
        vert_main += "v_uv = a_uv;\n";

        frag_ins += "in vec2 v_uv;\n";
        frag_uniforms += "uniform sampler2D u_tex;\n";
        frag_main += "o_color *= texture(u_tex, v_uv);\n";
    
        input_flags |= SIF_COLOR_TEXTURE;
    }

    // any object color type
    if (flags & (SRF_OBJECT_COLOR | SRF_OBJECT_COLOR_BACKGROUND))
    {
        frag_uniforms += "uniform vec4 u_color;\n";
        
        if (flags & SRF_OBJECT_COLOR_BACKGROUND)
        {
            frag_main += "o_color = mix(u_color, o_color, o_color.a);\n"; 
        }
        else // just multiply
        {
            frag_main += "o_color *= u_color;\n";
        }

        input_flags |= SIF_OBJECT_COLOR;
    }
    else if (flags & SRF_MULTICOLOR)
    {
        frag_uniforms += "uniform vec4 u_color[MAX_COLORS];\n";
        frag_main += R"GLSL(
            int color_slot = clamp(int(o_color.a * 9.0), 0, MAX_COLORS);
            o_color.a = 1.0;

            float emis = 0.0;
            
            if (color_slot < MAX_COLORS)
            {
                vec4 color = u_color[color_slot];
                o_color.rgb *= color.rgb;
                emis = color.a;
            }

        )GLSL";

        input_flags |= SIF_MULTICOLOR_DATA;
    }

    // alpha culling
    if (flags & SRF_CULL_ALPHA)
    {
        frag_main += "if (o_color.a < 0.5) discard;\n";
    }

    // lighting
    if (flags & SRF_LIT)
    {
        vert_uniforms += R"GLSL(
            // global
            uniform vec3 u_ambient_light;
            uniform vec3 u_sun_direction;
            uniform vec3 u_sun_color;

            // local
            uniform int u_num_lights;
            uniform vec3 u_light_positions[MAX_LIGHTS];
            uniform vec4 u_light_colors_rs[MAX_LIGHTS]; // rgb = color, a = radius
        )GLSL";

        vert_funcs += R"GLSL(
            vec3 ComputeLights(in vec3 sector_pos, in vec3 sector_normal)
            {
                // Base ambient
                vec3 color = u_ambient_light;

                // Sunlight contribution
                float sun_dot = max(dot(sector_normal, -u_sun_direction), 0.0);
                color += u_sun_color * sun_dot;

                // Point lights
                for (int i = 0; i < u_num_lights; ++i) {
                    vec3 light_pos = u_light_positions[i];
                    vec3 light_color = u_light_colors_rs[i].rgb;
                    float light_radius = u_light_colors_rs[i].a;
                    
                    vec3 to_light = light_pos - sector_pos;
                    float dist2 = dot(to_light, to_light);
                    if (dist2 < light_radius * light_radius) {
                        float dist = sqrt(dist2);
                        float attenuation = 1.0 - (dist / light_radius);
                        //float dot_term = max(dot(sector_normal, normalize(to_light)), 0.0);
                        color += light_color * attenuation;
                    }
                }

                return color;
            }
        )GLSL";

        vert_main += "v_color *= ComputeLights(world_pos.xyz, world_normal);\n";

        input_flags |= SIF_LIGHTING_DATA;
    }

    if (flags & SRF_SKELETAL) // skeletal deform
    {
        vert_attributes += R"GLSL(
            layout (location = 5) in ivec4 a_bone_ids;
            layout (location = 6) in vec4 a_bone_weights;
        )GLSL";

        vert_uniforms += R"GLSL(
            layout (std140) uniform Bones {
                mat4 u_bone_matrices[MAX_BONES];
            };
        )GLSL";
        
        vert_pos_calc = R"GLSL(
            mat4 bone_transform = mat4(0.0);
            for (int i = 0; i < 4; ++i) {
                int bone_id = a_bone_ids[i];
                if (bone_id >= 0) {
                    bone_transform += u_bone_matrices[bone_id] * a_bone_weights[i];
                }
            }

            vec4 world_pos = bone_transform * vec4(a_pos, 1.0);
            vec3 world_normal = normalize(mat3(bone_transform) * a_normal);
        )GLSL";

        input_flags |= SIF_SKELETAL_DATA;
    }
    else if (flags & SRF_DEFORM) // grid deform
    {
        vert_uniforms += R"GLSL(
            uniform mediump sampler3D u_deform_tex;
            uniform mat3 u_deform_info;
        )GLSL";
        
        vert_pos_calc = R"GLSL(
            vec3 deform_pos = (a_pos - u_deform_info[0]) / (u_deform_info[1] - u_deform_info[0]);
            vec3 pos = a_pos + texture(u_deform_tex, deform_pos).xyz * u_deform_info[2].x;

            vec4 world_pos = u_model * vec4(pos, 1.0);
            vec3 world_normal = normalize(mat3(u_model) * a_normal);
        )GLSL";

        input_flags |= SIF_DEFORM_DATA;
    }

    if (flags & SRF_MULTICOLOR)
    {
        frag_main += "o_color.rgb *= mix(v_color, vec3(1.5), emis);";
    }
    else
    {
        frag_main += "o_color.rgb *= v_color;";
    }


    vert_main = vert_pos_calc + vert_main;

    std::string vertex_src = SHADER_HEADER + vert_attributes + vert_uniforms + vert_outs + vert_funcs + "\nvoid main() {\n" + vert_main + "\n}\n";
    std::string fragment_src = SHADER_HEADER + frag_ins + frag_uniforms + "\nlayout (location = 0) out vec4 o_color;\n" + frag_funcs + "\nvoid main() {\n" + frag_main + "\n}\n";

    return std::make_unique<Shader>(vertex_src.c_str(), fragment_src.c_str());
}