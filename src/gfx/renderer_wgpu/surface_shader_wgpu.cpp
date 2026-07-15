#include "surface_shader_wgpu.hpp"

#include <string>
#include <cassert>

static std::string GetShaderSource(gfx::SurfacePipelineFlags flags)
{
    std::string vertex_ins;
    std::string special_bind_group;
    std::string vertex_main;
    std::string vertex_outs;
    std::string fragment_main;

    // DEFORM / SKELETAL
    if (flags & gfx::SPF_SKELETAL)
    {
        assert((flags & gfx::SPF_DEFORM) == 0 && "Shader cant do both skeleton and grid deform");

        vertex_ins += "@location(5) bone_ids : vec4u,\n";
        vertex_ins += "@location(6) bone_weights : vec4f,\n";

        special_bind_group += "@group(3) @binding(0) var<uniform> u_bones: array<mat4x4f, 128>;\n";

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
        special_bind_group += "@group(3) @binding(0) var deform_texture_sampler: sampler;\n";
        special_bind_group += "@group(3) @binding(1) var deform_texture: texture_2d<f32>;\n";
        special_bind_group += R"WGSL(
            struct DeformInfo {
                deform_min: vec3f,
                max_offset: f32,
                deform_max: vec3f,
                _pad0: f32,
            };
        )WGSL";
        special_bind_group += "@group(3) @binding(2) var<uniform> u_deform_info: DeformInfo;\n";

        vertex_main += R"WGSL(
            let deform_pos = (in.position - u_deform_info.deform_min) / (u_deform_info.deform_max - u_deform_info.deform_min);
            let pos = in.position + textureSample(deform_texture, deform_texture_sampler, deform_pos) * u_deform_info.max_offset;

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

    std::string shader;
    shader.reserve(2048);
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
            view_proj: mat4x4f,
        };
        @group(0) @binding(0) var<uniform> u_global: GlobalData;

        // material
        @group(1) @binding(0) var texture_sampler: sampler;
        @group(1) @binding(1) var color_texture: texture_2d<f32>;

        // instance
        struct InstanceData {
            matrix: mat4x4f,
            colors: array<u32, 8>,
        };
        @group(2) @binding(0) var<storage> u_instance: array<InstanceData>;

        // special
    )WGSL";
    shader += special_bind_group;

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
    shader += "fn fs_main(in: VertexOutput) -> @location(0) vec4f {\n";
    shader += "    var out = vec4f(1.0);\n\n";
    shader += fragment_main;
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
