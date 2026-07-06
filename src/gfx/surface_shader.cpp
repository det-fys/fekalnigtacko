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
        out vec3 v_world_pos;
    )GLSL";
    
    vert_pos_calc = R"GLSL(
        vec4 world_pos = u_model * vec4(a_pos, 1.0);
        vec3 world_normal = normalize(mat3(u_model) * a_normal);
    )GLSL";

    vert_main = R"GLSL(
        gl_Position = u_view_proj * world_pos;
        v_color = a_color.rgb;
        v_world_pos = world_pos.xyz;
    )GLSL";

    frag_ins = R"GLSL(
        in vec3 v_color;
        in vec3 v_world_pos;
    )GLSL";

    frag_main = R"GLSL(
        o_color = vec4(v_color, 1.0);
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
        constexpr std::string_view light_uniforms = R"GLSL(
            // global
            uniform vec3 u_ambient_light;
            uniform vec3 u_sun_direction;
            uniform vec3 u_sun_color;

            // local
            uniform int u_num_lights;
            uniform mat3x4 u_light_data[MAX_LIGHTS];
        )GLSL";

        constexpr std::string_view light_normal_function = R"GLSL(
            float GetNormalFactor(in vec3 world_normal, in vec3 dir) {
                return max(dot(world_normal, dir), 0.0);
            }

        )GLSL";

        constexpr std::string_view light_normal_translucent_function = R"GLSL(
            float GetNormalFactor(in vec3 world_normal, in vec3 dir) {
                return max(dot(world_normal, dir), 0.5);
            }

        )GLSL";

        constexpr std::string_view lights_function = R"GLSL(
            vec3 ComputeLights(in vec3 world_pos, in vec3 world_normal)
            {
                // Base ambient
                vec3 color = u_ambient_light;

                // Sunlight contribution
                color += u_sun_color * GetNormalFactor(world_normal, -u_sun_direction);

                // Point/spot lights
                for (int i = 0; i < u_num_lights; ++i) {

                    vec4 data_p = u_light_data[i][0];
                    vec4 data_c = u_light_data[i][1];
                    vec4 data_d = u_light_data[i][2];

                    vec3 light_pos = data_p.xyz;
                    float light_radius = data_p.w;

                    vec3 light_color = data_c.xyz;
                    float light_cos_inner = data_c.w;

                    vec3 light_dir = data_d.xyz;
                    float light_cos_outer = data_d.w;

                    vec3 to_light = light_pos - world_pos;
                    float dist2 = dot(to_light, to_light);

                    if (dist2 > light_radius * light_radius)
                        continue;

                    float dist = sqrt(dist2);
                    vec3 L = to_light / dist;

                    float attenuation = 1.0 - (dist / light_radius);
                    attenuation *= attenuation;

                    float ndotl = GetNormalFactor(world_normal, L);
                    //ndotl = mix(1.0, ndotl, 0.35);
                    ndotl = sqrt(ndotl);

                    float spot = 1.0;

                    if (light_cos_inner > light_cos_outer) {

                        float c = dot(-L, light_dir);
                        float cone = smoothstep(light_cos_outer, light_cos_inner, c);

                        // sharpen the beam a bit
                        spot = cone * cone;
                    }

                    color += light_color * (attenuation * ndotl * spot);
                }

                return color;
            }

        )GLSL";

        if (flags & SRF_LIT_VERTEX)
        {
            vert_uniforms += light_uniforms;
            vert_funcs += (flags & SRF_TRANSLUCENT) ? light_normal_translucent_function : light_normal_function;
            vert_funcs += lights_function;
            vert_outs += "out vec3 v_light_color;\n";
            vert_main += "v_light_color = ComputeLights(world_pos.xyz, world_normal);\n";

            frag_ins += "in vec3 v_light_color;\n";
            frag_main += "vec3 light_color = v_light_color;\n";
        }
        else
        {
            vert_outs += "out vec3 v_world_normal;\n";
            vert_main += "v_world_normal = world_normal;\n";

            frag_ins += "in vec3 v_world_normal;\n";
            frag_uniforms += light_uniforms;
            frag_funcs += (flags & SRF_TRANSLUCENT) ? light_normal_translucent_function : light_normal_function;
            frag_funcs += lights_function;

            // this is currently weird with trees that have fake +Z normals
            // TODO: make this another flag
            //frag_main += "vec3 light_color = ComputeLights(v_world_pos, normalize(gl_FrontFacing ? v_world_normal : -v_world_normal));\n";
            frag_main += "vec3 light_color = ComputeLights(v_world_pos, normalize(v_world_normal));\n";
        }

        input_flags |= SIF_LIGHTING_DATA;
    }
    else
    {
        frag_main += "vec3 light_color = vec3(1.0);\n";
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
        frag_main += "o_color.rgb *= mix(light_color, vec3(1.5), emis);\n";
    }
    else
    {
        frag_main += "o_color.rgb *= light_color;\n";
    }

    if (flags & SRF_FOG)
    {
        frag_uniforms += R"GLSL(
            uniform vec4 u_fog;
            uniform vec3 u_camera_pos;
        )GLSL";
    
        frag_main += R"GLSL(
            float dist = distance(v_world_pos, u_camera_pos);
            float fog_factor = 1.0 / (1.0 + dist * dist * u_fog.a);
            o_color.rgb = mix(u_fog.rgb, o_color.rgb, fog_factor);
        )GLSL";

        input_flags |= SIF_FOG_DATA;
    }

    vert_main = vert_pos_calc + vert_main;

    std::string vertex_src = SHADER_HEADER + vert_attributes + vert_uniforms + vert_outs + vert_funcs + "\nvoid main() {\n" + vert_main + "\n}\n";
    std::string fragment_src = SHADER_HEADER + frag_ins + frag_uniforms + "\nlayout (location = 0) out vec4 o_color;\n" + frag_funcs + "\nvoid main() {\n" + frag_main + "\n}\n";

    return std::make_unique<Shader>(vertex_src.c_str(), fragment_src.c_str());
}