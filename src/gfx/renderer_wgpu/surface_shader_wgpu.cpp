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
        bindings += R"WGSL(
            @group(0) @binding(1) var shadow_sampler: sampler_comparison;
            @group(0) @binding(2) var csm_texture: texture_depth_2d_array;
        )WGSL";

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

            fn sample_csm(world_pos: vec3f, depth: f32) -> f32 {
                var cascade_idx = select_csm_cascade(depth);

                var factor = 0.0;

                if (cascade_idx == MAX_CASCADES) {
                    cascade_idx = MAX_CASCADES - 1u;
                    factor = 1.0;
                }

                var pos = u_global.csm_matrices[cascade_idx] * vec4f(world_pos, 1.0);
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

            fn compute_sun_light(world_pos: vec3f, world_normal: vec3f) -> vec3f {
                let N = normalize(world_normal);
                let L = normalize(-u_global.sun_direction);

                let NdotL = max(dot(N, L), 0.0);

                let view_pos = u_global.view * vec4f(world_pos, 1.0);
                let depth = -view_pos.z;
                return u_global.sun_color * (NdotL * sample_csm(world_pos, depth));
            }

            fn compute_lights(world_pos: vec3f, world_normal: vec3f) -> vec3f {
                var color = u_global.ambient_color;
                color += compute_sun_light(world_pos, world_normal);
                return color;
            }
        )WGSL";

        //fragment_main += "out *= get_cascade_color(in.world_position);\n";
        fragment_main += "out = vec4f(out.rgb * compute_lights(in.world_position, in.world_normal), out.a);\n";
    }

    if (flags & gfx::SPF_FOG)
    {
        fragment_main += R"WGSL(
            let dist = distance(in.world_position, u_global.camera_pos);
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
    shader += "    @location(1) world_normal: vec3f,\n";
    shader += "    @location(2) world_position: vec3f,\n";
    shader += "    @location(3) uv0: vec2f,\n";
    shader += vertex_outs;
    shader += "};\n";
    shader += R"WGSL(
        // global/pass
        struct GlobalData {
            view: mat4x4f,
            view_proj: mat4x4f,
            ambient_color: vec3f,
            _pad0: f32,
            sun_color: vec3f,
            _pad1: f32,
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
    shader += "    out.position = u_global.view_proj * vec4f(world_pos, 1.0);\n";
    shader += "    out.instance_id = in.instance_id;\n";
    shader += "    out.world_position = world_pos;\n";
    shader += "    out.world_normal = world_normal;\n";
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
