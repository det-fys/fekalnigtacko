#include "surface_shader_wgpu.hpp"

#include <string>
#include <cassert>

#include "shader_defs_wgsl.hpp"

static std::string GetShaderSource(gfx::SurfacePipelineFlags flags)
{
    std::string vertex_ins;
    std::string bindings;
    std::string functions;
    std::string vertex_main;
    std::string vertex_outs;
    std::string fragment_main;

    bool fragment_output = (flags & gfx::SPF_DEPTH_ONLY) == 0;

    // in depth prepass the position calculation must exactly match the main pass
    // due to EQUAL depth testing later
    bool need_view_pos = (flags & gfx::SPF_SHADOW_MAP) == 0;

    // DEFORM / SKELETAL
    if (flags & gfx::SPF_SKELETAL)
    {
        assert((flags & gfx::SPF_DEFORM) == 0 && "Shader cant do both skeleton and grid deform");

        vertex_ins += "@location(5) bone_ids : vec4u,\n";
        vertex_ins += "@location(6) bone_weights : vec4f,\n";

        bindings += "@group(3) @binding(0) var<uniform> u_bones: array<mat4x4f, MAX_BONES>;\n";

        vertex_main += R"WGSL(
            var bone_transform = mat4x4f(vec4f(0.0), vec4f(0.0), vec4f(0.0), vec4f(0.0));

            for (var i = 0u; i < 4u; i++) {
                let bone_id = in.bone_ids[i];
                if (bone_id < 255u) {
                    let weight = in.bone_weights[i];
                    bone_transform = bone_transform + u_bones[bone_id] * weight;
                }
            }

            let world_pos = (bone_transform * vec4f(in.position, 1.0)).xyz;
            let world_normal = normalize(
                mat3x3f(
                    bone_transform[0].xyz,
                    bone_transform[1].xyz,
                    bone_transform[2].xyz
                ) * in.normal
            );
        )WGSL";
    }
    else if (flags & gfx::SPF_DEFORM)
    {
        bindings += "@group(3) @binding(0) var deform_texture_sampler: sampler;\n";
        bindings += "@group(3) @binding(1) var deform_texture: texture_3d<f32>;\n";
        bindings += R"WGSL(
            struct DeformInfo {
                deform_min: vec3f,
                max_offset: f32,
                deform_max: vec3f,
                _pad0: f32,
            };
        )WGSL";
        bindings += "@group(3) @binding(2) var<uniform> u_deform_info: DeformInfo;\n";

        vertex_main += R"WGSL(
            let deform_pos = (in.position - u_deform_info.deform_min) / (u_deform_info.deform_max - u_deform_info.deform_min);
            let pos = in.position + textureSampleLevel(deform_texture, deform_texture_sampler, deform_pos, 0.0).xyz * u_deform_info.max_offset;

            let transform = instance.matrix;
            let world_pos = (transform * vec4f(pos, 1.0)).xyz;
            let world_normal = normalize(
                mat3x3f(
                    transform[0].xyz,
                    transform[1].xyz,
                    transform[2].xyz
                ) * in.normal
            );
        )WGSL";
    }
    else // no vertex deform
    {
        vertex_main += R"WGSL(
            let transform = instance.matrix;
            let world_pos = (transform * vec4f(in.position, 1.0)).xyz;
            let world_normal = normalize(
                mat3x3f(
                    transform[0].xyz,
                    transform[1].xyz,
                    transform[2].xyz
                ) * in.normal
            );
        )WGSL";
    }

    if (flags & gfx::SPF_TEXTURE)
    {
        fragment_main += "out *= textureSample(color_texture, texture_sampler, in.uv0);\n";
    }

    if (flags & gfx::SPF_CULL_ALPHA)
    {
        fragment_main += "if (out.a < 0.5) { discard; }\n";
    }

    if (flags & gfx::SPF_OBJECT_COLOR)
    {
        if (flags & gfx::SPF_OBJECT_COLOR_BACKGROUND)
        {
            fragment_main += "out = mix(unpack4x8unorm(instance.colors[0]), out, out.a);\n";
        }
        else
        {
            fragment_main += "out *= unpack4x8unorm(instance.colors[0]);\n";
        }
    }
    else if (flags & gfx::SPF_MULTICOLOR)
    {
        // TODO: use emis
        fragment_main += R"WGSL(
            let color_slot = clamp(u32(out.a * 9.0), 0, MAX_COLORS);
            out.a = 1.0;

            var emis = 0.0;

            if (color_slot < MAX_COLORS) {
                let color = unpack4x8unorm(instance.colors[color_slot]);
                out = vec4f(out.rgb * color.rgb, out.a);
                emis = color.a;
            }
        )WGSL";
    }

    if (flags & gfx::SPF_LIT)
    {
        need_view_pos = true;

        bindings += R"WGSL(
            @group(0) @binding(1) var shadow_sampler: sampler_comparison;
            @group(0) @binding(2) var csm_texture: texture_depth_2d_array;

            struct LightBufferData {
                view_pos: vec3f,
                radius: f32,
                color: vec3f,
                cos_inner: f32,
                view_dir: vec3f,
                cos_outer: f32,
                view_bounding_pos: vec3f,
                bounding_radius: f32,
            };

            @group(0) @binding(3) var<storage, read> u_lights: array<LightBufferData>;
            @group(0) @binding(4) var<storage, read> u_visible: array<u32>;
        )WGSL";

        if (flags & gfx::SPF_TRANSLUCENT)
        {
            functions += R"WGSL(
                fn get_normal_factor(normal: vec3f, dir: vec3f) -> f32 {
                    return max(dot(normal, dir), 0.5);
                }
            )WGSL";
        }
        else
        {
            functions += R"WGSL(
                fn get_normal_factor(normal: vec3f, dir: vec3f) -> f32 {
                    return max(dot(normal, dir), 0.0);
                }
            )WGSL";
        }

        functions += R"WGSL(
            fn select_csm_cascade(depth: f32) -> u32 {
                for (var i: u32 = 0u; i < MAX_CASCADES; i++) {
                    if (i >= u_global.csm_cascade_count) {
                        break;
                    }            
        
                    if (depth < u_global.csm_splits[i]) {
                        return i;
                    }
                }

                return MAX_CASCADES;
            }

            fn sample_csm(view_pos: vec3f, depth: f32) -> f32 {
                var cascade_idx = select_csm_cascade(depth);

                var factor = 0.0;

                if (cascade_idx == MAX_CASCADES) {
                    cascade_idx = MAX_CASCADES - 1u;
                    factor = 1.0;
                }

                var pos = u_global.csm_matrices[cascade_idx] * vec4f(view_pos, 1.0);
                pos /= pos.w;

                let uv = vec2f(
                    pos.x * 0.5 + 0.5,
                    1.0 - (pos.y * 0.5 + 0.5)
                );

                let texel_size = u_global.csm_texel_size;

                let bias = 0.002;

                var shadow = 0.0;

                for (var x: i32 = -1; x <= 1; x++) {
                    for (var y: i32 = -1; y <= 1; y++) {
                        shadow += textureSampleCompare(
                            csm_texture,
                            shadow_sampler,
                            uv + vec2f(f32(x), f32(y)) * texel_size,
                            cascade_idx,
                            pos.z - bias
                        );
                    }
                }

                shadow /= 9.0;

                return max(shadow, factor);
            }

            fn compute_sun_light(view_pos: vec3f, view_normal: vec3f) -> vec3f {
                let L = -u_global.sun_direction;

                let NdotL = get_normal_factor(view_normal, L);

                let depth = -view_pos.z;
                return u_global.sun_color * (NdotL * sample_csm(view_pos, depth));
            }

            fn compute_light_contribution(light: ptr<storage, LightBufferData, read>, view_pos: vec3f, view_normal: vec3f) -> vec3f {
                let to_light = light.view_pos - view_pos;
                let dist2 = dot(to_light, to_light);

                if (dist2 > light.radius * light.radius) {
                    return vec3f(0.0);
                }

                let dist = sqrt(dist2);
                let L = to_light / dist;

                var attenuation = 1.0 - (dist / light.radius);
                attenuation *= attenuation;

                var ndotl = get_normal_factor(view_normal, L);
                ndotl = sqrt(ndotl);

                var spot = 1.0;

                if (light.cos_inner > light.cos_outer) {
                    let c = dot(-L, light.view_dir);
                    let cone = smoothstep(light.cos_outer, light.cos_inner, c);
    
                    spot = cone * cone;
                }

                return light.color * (attenuation * ndotl * spot);
            }

            fn compute_small_lights(view_pos: vec3f, view_normal: vec3f, frag_pos: vec2f) -> vec3f {
                let tile = vec2u(frag_pos) / TILE_SIZE;
                let tile_index = tile.y * u_global.tile_count_x + tile.x;

                let base = tile_index * MAX_LIGHTS_PER_TILE;

                var accum = vec3f(0.0);

                for (var i = 0u; i < MAX_LIGHTS_PER_TILE; i++) {
                    let light_idx = u_visible[base + i];

                    if (light_idx == INVALID_LIGHT_IDX) {
                        break;
                    }
    
                    accum += compute_light_contribution(&u_lights[light_idx], view_pos, view_normal);
                    //accum.g += 0.2;

                    //if (distance(view_pos, light.view_bounding_pos) < light.bounding_radius)
                    //{
                    //    accum.b += 0.2;
                    //}
                }

                return accum;

            }

            fn compute_lights(view_pos: vec3f, view_normal: vec3f, frag_pos: vec2f) -> vec3f {
                var color = u_global.ambient_color;
                color += compute_sun_light(view_pos, view_normal);
                color += compute_small_lights(view_pos, view_normal, frag_pos);
                return color;
            }
        )WGSL";

        fragment_main += "out = vec4f(out.rgb * compute_lights(in.view_pos, normalize(in.view_normal), in.position.xy), out.a);\n";
    }

    if (flags & gfx::SPF_FOG)
    {
        need_view_pos = true;

        fragment_main += R"WGSL(
            let dist = length(in.view_pos);
            let fog_factor = 1.0 / (1.0 + dist * dist * u_global.fog.a);
            out = vec4f(mix(u_global.fog.rgb, out.rgb, fog_factor), out.a);
        )WGSL";
    }

    std::string shader;
    shader.reserve(2048);
    shader += SHADER_DEFS_WGSL;
    shader += "struct VertexInput {\n";
    shader += "    @builtin(instance_index) instance_id: u32,\n";
    shader += "    @location(0) position: vec3f,\n";
    shader += "    @location(1) normal: vec3f,\n";
    shader += "    @location(3) uv0: vec2f,\n";
    shader += vertex_ins;
    shader += "};\n";
    shader += "struct VertexOutput {\n";
    shader += "    @builtin(position) position: vec4f,\n";
    shader += "    @location(0) @interpolate(flat) instance_id: u32,\n";
    shader += "    @location(1) view_pos: vec3f,\n";
    shader += "    @location(2) view_normal: vec3f,\n";
    shader += "    @location(3) uv0: vec2f,\n";
    shader += vertex_outs;
    shader += "};\n";
    shader += R"WGSL(
        // global/pass
        struct GlobalData {
            view: mat4x4f,
            proj: mat4x4f,
            view_proj: mat4x4f,
            ambient_color: vec3f,
            _pad0: f32,
            sun_color: vec3f,
            tile_count_x: u32,
            sun_direction: vec3f,
            csm_texel_size: f32,
            camera_pos: vec3f,
            csm_cascade_count: u32,
            fog: vec4f,
            csm_splits: vec4f,
            csm_matrices: array<mat4x4f, MAX_CASCADES>,
        };
        @group(0) @binding(0) var<uniform> u_global: GlobalData;

        // material
        @group(1) @binding(0) var texture_sampler: sampler;
        @group(1) @binding(1) var color_texture: texture_2d<f32>;

        // instance
        struct InstanceData {
            matrix: mat4x4f,
            colors: array<u32, MAX_COLORS>,
        };
        @group(2) @binding(0) var<storage> u_instance: array<InstanceData>;

        // special
    )WGSL";
    shader += bindings;
    shader += functions;

    shader += "@vertex\n";
    shader += "fn vs_main(in: VertexInput) -> VertexOutput {\n";
    shader += "    let instance = u_instance[in.instance_id];\n";
    shader += "    var out: VertexOutput;\n";
    shader += vertex_main;
    shader += "    out.instance_id = in.instance_id;\n";
    if (need_view_pos)
    {
        shader += "    out.view_pos = (u_global.view * vec4f(world_pos, 1.0)).xyz;\n";
        shader += "    out.view_normal = mat3x3f(u_global.view[0].xyz, u_global.view[1].xyz, u_global.view[2].xyz) * world_normal;\n";
        shader += "    out.position = u_global.proj * vec4f(out.view_pos, 1.0);\n";
    }
    else
    {
        shader += "    out.position = u_global.view_proj * vec4f(world_pos, 1.0);\n";
    }
    shader += "    out.uv0 = in.uv0;\n";
    shader += "    return out;\n\n";
    shader += "}\n\n";

    shader += "@fragment\n";
    shader += "fn fs_main(in: VertexOutput) ";
    if (fragment_output)
        shader += "-> @location(0) vec4f";
    shader += "{\n";
    shader += "    let instance = u_instance[in.instance_id];\n";
    shader += "    var out = vec4f(1.0);\n\n";
    shader += fragment_main;
    if (fragment_output)
        shader += "    return out;\n\n";
    shader += "}\n\n";

    return shader;
}

wgpu::ShaderModule gfx::CreateSurfaceShaderWGPU(const wgpu::Device& device, SurfacePipelineFlags flags)
{
    auto src = GetShaderSource(flags);

    wgpu::ShaderModuleDescriptor shader_desc{};
    shader_desc.label = "A surface shader";
    wgpu::ShaderSourceWGSL shader_src{};
    shader_src.code = std::string_view(src);
    shader_desc.nextInChain = &shader_src;
    return device.CreateShaderModule(&shader_desc);
}
