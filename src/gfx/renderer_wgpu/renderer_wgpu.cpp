#include "renderer_wgpu.hpp"

#include <stdexcept>
#include <iostream>
#include <algorithm>

#include <dawn/webgpu_cpp_print.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include "assets/asset_manager.hpp"
#include "surface_sdl.hpp"
#include "../common/vertex_pack.hpp"
#include "../common/hud_matrix.hpp"
#include "surface_shader_wgpu.hpp"
#include "utils/cvars.hpp"
#include "shader_defs_wgsl.hpp"

//CVAR_CL(uint8_t, r_msaa, CV_SAVE, 0, 0, 1);

// max render distance
CVAR_CL(uint32_t, r_distance, CV_SAVE, 3000, 1, 3000);

// 0: 256
// 1: 512
// 2: 1024
// 3: 2048
// 4: 4096
CVAR_CL(uint32_t, r_csm_resolution, CV_SAVE, 3, 0, 4);
CVAR_CL(uint32_t, r_shadow_resolution, CV_SAVE, 2, 0, 4);

CVAR_CL(uint32_t, r_csm_cascades, CV_SAVE, 4, 0, 4);
CVAR_CL(uint32_t, r_shadow_count, CV_SAVE, 4, 0, 8);

// 0: linear
// 1: PCF 2x2
// 2: PCF 3x3
// 3: IGN
CVAR_CL(uint32_t, r_shadow_quality, CV_SAVE, 3, 0, 3);

// 0: immediate
// 1: FIFO
// 2: mailbox
CVAR_CL(uint32_t, r_vsync, CV_SAVE, 0, 0, 2);

CVAR_CL(uint8_t, r_tile_debug, CV_NONE, 0, 0, 1);

// 0: none
// 1: SSAO
CVAR_CL(uint8_t, r_ao, CV_SAVE, 0, 0, 1);

// 0: disabled
// 1: enabled
CVAR_CL(uint8_t, r_fxaa, CV_SAVE, 0, 0, 1);

static std::vector<uint8_t> temp_buffer;

static inline glm::vec3 LinearizeColor(const glm::vec3 color_srgb)
{
    return color_srgb * color_srgb;
}

static inline uint32_t GetMultisampleCount()
{
    return 1;
    //return r_msaa.Get() > 0 ? 4 : 1;
}

gfx::RendererWGPU::RendererWGPU(SDL_Window* window) : Renderer(window)
{
    InitWGPU();
    SelectSurfaceFormat();

    msaa_samples_ = GetMultisampleCount();

    CreateGlobalResources();
    CreateMipmapPipeline();
    CreateGuiPipeline();
    CreateLightCullingPipeline();
    CreateCoronaPipeline();
    CreateFXAAPipeline();
}

gfx::MeshID gfx::RendererWGPU::CreateMesh(const MeshDescriptor& desc)
{
    MeshWGPU mesh{};
    mesh.desc = desc;
    mesh.vertex_buffer.usage |= wgpu::BufferUsage::Vertex;
    mesh.index_buffer.usage |= wgpu::BufferUsage::Index;
    return meshes_.Alloc(std::move(mesh));
}

void gfx::RendererWGPU::SetMeshVertexData(MeshID mesh_id, const MeshVertexData& data)
{
    auto& mesh = meshes_.Get(mesh_id);
    PackVertexData(mesh.desc, data, temp_buffer);
    SetBufferData(mesh.vertex_buffer, temp_buffer);
}

void gfx::RendererWGPU::SetMeshTriangleData(MeshID mesh_id, const MeshTriangleData& data)
{
    auto& mesh = meshes_.Get(mesh_id);
    std::span<const uint8_t> buffer_data(reinterpret_cast<const uint8_t*>(data.triangles.data()),
                                         data.triangles.size_bytes());

    SetBufferData(mesh.index_buffer, buffer_data);
}

void gfx::RendererWGPU::ReleaseMesh(MeshID mesh_id)
{
    meshes_.Free(mesh_id);
}

gfx::TextureID gfx::RendererWGPU::CreateTexture(const TextureDescriptor& desc)
{
    TextureWGPU texture{};
    texture.desc = desc;
    if (desc.mipmaps && !desc.linear_rgb) // TODO: enable linear mipmap generation
    {
        texture.mip_levels = std::max(
            1U, std::min(desc.max_mipmap_level, uint32_t(std::floor(std::log2(std::max(desc.width, desc.height))))));
    }

    wgpu::TextureDescriptor tex_desc{};
    tex_desc.dimension = wgpu::TextureDimension::e2D;
    tex_desc.size = {desc.width, desc.height, 1};
    tex_desc.format = desc.linear_rgb ? wgpu::TextureFormat::RGBA8Unorm : wgpu::TextureFormat::RGBA8UnormSrgb;
    tex_desc.mipLevelCount = texture.mip_levels;
    tex_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
    if (tex_desc.mipLevelCount > 1)
    {
        tex_desc.usage |= wgpu::TextureUsage::RenderAttachment; // need to render mipmaps
    }

    tex_desc.label = "Game 2D texture";
    texture.texture = device_.CreateTexture(&tex_desc);

    texture.view = texture.texture.CreateView();

    return textures_.Alloc(std::move(texture));
}

static bool IsFullyOpaque(std::span<const uint8_t> data)
{
    for (size_t i = 3; i < data.size(); i += 4)
    {
        if (data[i] < 200)
            return false;
    }

    return true;
}

void gfx::RendererWGPU::SetTextureData(TextureID texture_id, std::span<const uint8_t> data)
{
    auto& texture = textures_.Get(texture_id);
    texture.fully_opaque = IsFullyOpaque(data);

    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = texture.texture;

    wgpu::TexelCopyBufferLayout layout{};
    layout.offset = 0;
    layout.bytesPerRow = texture.desc.width * 4;
    layout.rowsPerImage = texture.desc.height;

    wgpu::Extent3D size{texture.desc.width, texture.desc.height, 1};

    queue_.WriteTexture(&dst, data.data(), data.size_bytes(), &layout, &size);

    if (texture.mip_levels < 2)
        return;

    // generate mipmaps
    auto encoder = device_.CreateCommandEncoder();
    auto sampler = GetSampler(true, false);

    wgpu::TextureViewDescriptor view_desc{};
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;

    auto srcView = texture.texture.CreateView(&view_desc);

    for (uint32_t level = 1; level < texture.mip_levels; level++)
    {
        view_desc.baseMipLevel = level;
        auto dstView = texture.texture.CreateView(&view_desc);

        wgpu::BindGroupEntry bgEntries[2]{};

        bgEntries[0].binding = 0;
        bgEntries[0].sampler = sampler;

        bgEntries[1].binding = 1;
        bgEntries[1].textureView = srcView;

        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = mipmap_bind_group_layout_;
        bgDesc.entryCount = 2;
        bgDesc.entries = bgEntries;

        auto bindGroup = device_.CreateBindGroup(&bgDesc);

        wgpu::RenderPassColorAttachment color{};
        color.view = dstView;
        color.loadOp = wgpu::LoadOp::Clear;
        color.storeOp = wgpu::StoreOp::Store;

        wgpu::RenderPassDescriptor passDesc{};
        passDesc.colorAttachmentCount = 1;
        passDesc.colorAttachments = &color;

        auto pass = encoder.BeginRenderPass(&passDesc);

        pass.SetPipeline(mipmap_pipeline_);
        pass.SetBindGroup(0, bindGroup);
        pass.Draw(3);
        pass.End();

        srcView = std::move(dstView);
    }

    auto command = encoder.Finish();
    queue_.Submit(1, &command);
}

void gfx::RendererWGPU::ReleaseTexture(TextureID texture_id)
{
    textures_.Free(texture_id);
}

gfx::MaterialID gfx::RendererWGPU::CreateMaterial(const MaterialDescriptor& desc)
{
    MaterialWGPU material{};
    material.desc = desc;

    auto& color_texture = textures_.Get(desc.texture ? desc.texture : GetWhiteTexture());
    material.color_texture = color_texture.texture;
    material.color_texture_view = color_texture.view;

    material.alpha_culling =
        desc.properties.blend == MATERIAL_BLEND_TYPE_NONE && !color_texture.fully_opaque &&
                             (desc.properties.color == MATERIAL_OBJECT_COLOR_TYPE_NONE ||
                              desc.properties.color == MATERIAL_OBJECT_COLOR_TYPE_MULTIPLY);

    // decide sampler based on color texture parameters
    // TODO: maybe change the criteria if multiple textures involved later?
    auto sampler = GetSamplerForTexture(color_texture);

    // setup bind group
    {
        std::array<wgpu::BindGroupEntry, 2> entries;

        auto& sampler_entry = entries[0];
        sampler_entry.binding = 0;
        sampler_entry.sampler = sampler;

        auto& color_texture_entry = entries[1];
        color_texture_entry.binding = 1;
        color_texture_entry.textureView = color_texture.view;

        wgpu::BindGroupDescriptor group_desc{};
        group_desc.layout = material_bind_group_layout_;
        group_desc.entryCount = entries.size();
        group_desc.entries = entries.data();
        group_desc.label = "Material bind group";
        material.bind_group = device_.CreateBindGroup(&group_desc);
    }

    return materials_.Alloc(std::move(material));
}

void gfx::RendererWGPU::ReleaseMaterial(MaterialID material_id)
{
    materials_.Free(material_id);
}

gfx::SkeletonPoseID gfx::RendererWGPU::CreateSkeletonPose(const SkeletonPoseDescriptor& desc)
{
    SkeletonPoseWGPU pose{};
    pose.num_bones = desc.num_bones;

    wgpu::BufferDescriptor buffer_desc{};
    //buffer_desc.size = desc.num_bones * sizeof(glm::mat4);
    buffer_desc.size = MAX_BONES * sizeof(glm::mat4);
    buffer_desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    buffer_desc.label = "Skeleton pose buffer";
    pose.bones_buffer = device_.CreateBuffer(&buffer_desc);

    // setup bind group
    {
        std::array<wgpu::BindGroupEntry, 1> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.buffer = pose.bones_buffer;

        wgpu::BindGroupDescriptor desc{};
        desc.layout = skeletal_bind_group_layout_;
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Skeleton pose bind group";
        pose.bind_group = device_.CreateBindGroup(&desc);
    }

    return poses_.Alloc(std::move(pose));
}

void gfx::RendererWGPU::SetSkeletonPoseTransforms(SkeletonPoseID pose_id, std::span<const glm::mat4> transforms)
{
    auto& pose = poses_.Get(pose_id);
    assert(transforms.size() == pose.num_bones && "Invalid number of transforms");

    queue_.WriteBuffer(pose.bones_buffer, 0, transforms.data(), transforms.size_bytes());
}

void gfx::RendererWGPU::ReleaseSkeletonPose(SkeletonPoseID pose_id)
{
    poses_.Free(pose_id);
}

gfx::DeformTextureID gfx::RendererWGPU::CreateDeformTexture(const DeformTextureDescriptor& desc)
{
    DeformTextureWGPU deform{};
    deform.desc = desc;

    glm::uvec3 tex_size(desc.grid.res);

    // setup texture
    wgpu::TextureDescriptor tex_desc{};
    tex_desc.dimension = wgpu::TextureDimension::e3D;
    tex_desc.size = {tex_size.x, tex_size.y, tex_size.z};
    tex_desc.format = wgpu::TextureFormat::RGBA8Snorm;
    tex_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
    tex_desc.label = "Deform texture";
    deform.texture = device_.CreateTexture(&tex_desc);

    deform.view = deform.texture.CreateView();

    // setup info buffer
    wgpu::BufferDescriptor info_desc{};
    info_desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    info_desc.size = sizeof(DeformInfoWGPU);
    info_desc.label = "Deform texture info buffer";
    deform.info_buffer = device_.CreateBuffer(&info_desc);

    DeformInfoWGPU info{};
    info.deform_min = desc.grid.min;
    info.deform_max = desc.grid.max;
    info.max_offset = desc.grid.max_offset;
    queue_.WriteBuffer(deform.info_buffer, 0, &info, sizeof(info));

    // setup bind group
    {
        std::array<wgpu::BindGroupEntry, 3> entries;

        auto& sampler_entry = entries[0];
        sampler_entry.binding = 0;
        sampler_entry.sampler = GetSampler(true, false);

        auto& texture_entry = entries[1];
        texture_entry.binding = 1;
        texture_entry.textureView = deform.view;

        auto& info_entry = entries[2];
        info_entry.binding = 2;
        info_entry.buffer = deform.info_buffer;

        wgpu::BindGroupDescriptor group_desc{};
        group_desc.layout = deform_bind_group_layout_;
        group_desc.entryCount = entries.size();
        group_desc.entries = entries.data();
        group_desc.label = "Deform texture bind group";
        deform.bind_group = device_.CreateBindGroup(&group_desc);
    }

    return deforms_.Alloc(std::move(deform));
}

void gfx::RendererWGPU::SetDeformTextureData(DeformTextureID deform_id, std::span<const glm::i8vec3> data)
{
    // convert to RGBA :(
    // TODO: make i8vec4 the original format
    static std::vector<glm::i8vec4> data_v4;
    data_v4.resize(data.size());
    for (size_t i = 0; i < data.size(); ++i)
    {
        data_v4[i] = glm::i8vec4(data[i], 0);
    }

    auto& deform = deforms_.Get(deform_id);
    const glm::uvec3 res(deform.desc.grid.res);

    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = deform.texture;

    wgpu::TexelCopyBufferLayout layout{};
    layout.offset = 0;
    layout.bytesPerRow = res.x * 4;
    layout.rowsPerImage = res.y;

    wgpu::Extent3D size{res.x, res.y, res.z};

    queue_.WriteTexture(&dst, data_v4.data(), data_v4.size() * sizeof(data_v4[0]), &layout, &size);
}

void gfx::RendererWGPU::ReleaseDeformTexture(DeformTextureID deform_id)
{
    deforms_.Free(deform_id);
}

void gfx::RendererWGPU::Draw(Scene& scene, const CameraParams& camera)
{
#if !defined(__EMSCRIPTEN__)
    instance_.ProcessEvents();
#endif

    UpdateSettings();

    Render(scene, camera);
}

gfx::RendererWGPU::~RendererWGPU()
{
    Unload();
}

void gfx::RendererWGPU::InitWGPU()
{
    // 2. Initialize Instance
    static const auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
    wgpu::InstanceDescriptor instanceDesc{.requiredFeatureCount = 1, .requiredFeatures = &kTimedWaitAny};
    instance_ = wgpu::CreateInstance(&instanceDesc);

    // 3. Request Adapter
    wgpu::RequestAdapterOptions options{};
#if !defined(__EMSCRIPTEN__)
    options.backendType = wgpu::BackendType::Vulkan;
#endif

    auto f1 = instance_.RequestAdapter(
        &options, wgpu::CallbackMode::WaitAnyOnly,
        [this](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
            if (status != wgpu::RequestAdapterStatus::Success)
            {
                //throw std::runtime_error("Could not request adapter: " + std::string(message));
            }
            adapter_ = std::move(adapter);
        });

    instance_.WaitAny(f1, UINT32_MAX);
    if (!adapter_)
    {
        throw std::runtime_error("could not request adapter");
    }

    // 4. Request Device
    wgpu::DeviceDescriptor desc{};
    desc.SetUncapturedErrorCallback([](const wgpu::Device&, wgpu::ErrorType errorType, wgpu::StringView message) {
        std::cerr << "WebGPU Error: " << errorType << " - message: " << message << "\n";
    });

    desc.SetDeviceLostCallback(wgpu::CallbackMode::AllowProcessEvents,
                               [](const wgpu::Device&, wgpu::DeviceLostReason type, wgpu::StringView msg) {
        std::cerr << "WebGPU Device lost: " << type << " - message: " << msg << "\n";
    });

    auto f2 = adapter_.RequestDevice(&desc, wgpu::CallbackMode::WaitAnyOnly,
        [this](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
            if (status != wgpu::RequestDeviceStatus::Success)
            {
                //throw std::runtime_error("Could not request device: " + std::string(message));
            }
            device_ = std::move(device);
        });

    instance_.WaitAny(f2, UINT32_MAX);
    if (!device_)
    {
        throw std::runtime_error("could not request device");
    }

    queue_ = device_.GetQueue();

    // 5. Setup Surface
    surface_ = CreateWGPUSurfaceFromSDLWindow(instance_, window_);
    if (!adapter_)
    {
        throw std::runtime_error("could not create surface");
    }
}

static wgpu::TextureFormat GetBestSurfaceFormat(std::span<const wgpu::TextureFormat> formats)
{
    std::cout << "surface formats count:" << formats.size() << std::endl;

    for (auto& format : formats)
    {
        std::cout << format << std::endl;
        if (format == wgpu::TextureFormat::BGRA8UnormSrgb || format == wgpu::TextureFormat::RGBA8UnormSrgb)
            return format;
    }

    return formats[0];
}

static wgpu::TextureFormat GetSurfaceSrgbViewFormat(wgpu::TextureFormat format)
{
    if (format == wgpu::TextureFormat::BGRA8Unorm)
        return wgpu::TextureFormat::BGRA8UnormSrgb;

    if (format == wgpu::TextureFormat::RGBA8Unorm)
        return wgpu::TextureFormat::RGBA8UnormSrgb;

    return format; // already srgb or weird format
}

void gfx::RendererWGPU::SelectSurfaceFormat()
{
    wgpu::SurfaceCapabilities caps{};
    surface_.GetCapabilities(adapter_, &caps);
    surface_real_format_ = GetBestSurfaceFormat({caps.formats, caps.formatCount});
    surface_format_ = GetSurfaceSrgbViewFormat(surface_real_format_);
}

void gfx::RendererWGPU::CreateGlobalResources()
{
    // MIPMAPS
    CreateMipmapResources();

    // shadow
    CreateShadowResources();

    // SURFACE
    CreateSurfaceGlobalResources();
    CreateSurfaceMaterialResources();
    CreateSurfaceInstanceResources();

    // GUI
    CreateGuiResources();

    // depth rendering
    CreateDepthResources();

    // light culling
    CreateLightCullingResources();

    // corona
    CreateCoronaResources();

    // ao
    CreateAOResources();

    // AA
    CreateFXAAResources();
}

void gfx::RendererWGPU::CreateMipmapResources()
{
    // mipmap bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 2> entries;

        auto& sampler_entry = entries[0];
        sampler_entry.binding = 0;
        sampler_entry.visibility = wgpu::ShaderStage::Fragment;
        sampler_entry.sampler.type = wgpu::SamplerBindingType::Filtering;

        auto& texture_entry = entries[1];
        texture_entry.binding = 1;
        texture_entry.visibility = wgpu::ShaderStage::Fragment;
        texture_entry.texture.sampleType = wgpu::TextureSampleType::Float;
        texture_entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Mipmap bind group layout";
        mipmap_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }

}

void gfx::RendererWGPU::CreateMipmapPipeline()
{
    constexpr std::string_view SHADER_SRC = R"WGSL(
        struct VSOut {
            @builtin(position) pos: vec4f,
            @location(0) uv: vec2f,
        };

        @vertex
        fn vs_main(@builtin(vertex_index) index: u32) -> VSOut {
            var positions = array<vec2f, 3>(
                vec2f(-1.0, -1.0),
                vec2f( 3.0, -1.0),
                vec2f(-1.0,  3.0)
            );

            var uvs = array<vec2f, 3>(
                vec2f(0.0, 1.0),
                vec2f(2.0, 1.0),
                vec2f(0.0, -1.0)
            );

            var out: VSOut;
            out.pos = vec4f(positions[index], 0.0, 1.0);
            out.uv = uvs[index];
            return out;
        }

        @group(0) @binding(0)
        var srcSampler: sampler;

        @group(0) @binding(1)
        var srcTexture: texture_2d<f32>;

        @fragment
        fn fs_main(in: VSOut) -> @location(0) vec4f {
            return textureSampleLevel(
                srcTexture,
                srcSampler,
                in.uv,
                0.0
            );
        }
    )WGSL";

    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "Mipmap pipeline";

    // shader
    wgpu::ShaderModuleDescriptor shader_desc{};
    shader_desc.label = "Mipmap shader";
    wgpu::ShaderSourceWGSL shader_src{};
    shader_src.code = SHADER_SRC;
    shader_desc.nextInChain = &shader_src;
    auto shader_module = device_.CreateShaderModule(&shader_desc);

    // vertex
    desc.vertex.module = shader_module;
    desc.vertex.entryPoint = "vs_main";
    desc.vertex.bufferCount = 0;
    desc.vertex.buffers = nullptr;

    // assembly
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.frontFace = wgpu::FrontFace::CCW;
    desc.primitive.cullMode = wgpu::CullMode::None;

    // color
    wgpu::ColorTargetState color{};
    color.format = wgpu::TextureFormat::RGBA8UnormSrgb;
    color.blend = nullptr;
    color.writeMask = wgpu::ColorWriteMask::All;

    // depth
    desc.depthStencil = nullptr;

    // fragment
    wgpu::FragmentState fragment{};
    fragment.module = shader_module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 1;
    fragment.targets = &color;
    desc.fragment = &fragment;

    // multisampling
    desc.multisample.count = 1;
    desc.multisample.mask = ~0u;

    // layout
    wgpu::PipelineLayoutDescriptor layout_desc{};
    layout_desc.bindGroupLayoutCount = 1;
    layout_desc.bindGroupLayouts = &mipmap_bind_group_layout_;
    layout_desc.label = "Mipmap pipeline layout";
    desc.layout = device_.CreatePipelineLayout(&layout_desc);

    mipmap_pipeline_ = device_.CreateRenderPipeline(&desc);
}

static uint32_t GetShadowMapResolution(uint32_t level)
{
    if (level == 0)
        return 256;
    else if (level == 1)
        return 512;
    else if (level == 2)
        return 1024;
    else if (level == 3)
        return 2048;
    else
        return 4096;
}

static gfx::ShadowSampleFunction GetShadowSampleFunction(uint32_t level)
{
    if (level == 0)
        return gfx::SHADOW_SAMPLE_DEFAULT;
    else if (level == 1)
        return gfx::SHADOW_SAMPLE_PCF2X2;
    else if (level == 2)
        return gfx::SHADOW_SAMPLE_PCF3X3;
    else
        return gfx::SHADOW_SAMPLE_IGN;

}

void gfx::RendererWGPU::CreateShadowResources()
{
    // load settings
    // resolutions
    csm_resolution_ = GetShadowMapResolution(r_csm_resolution.Get());
    spotlight_shadow_resolution_ = GetShadowMapResolution(r_shadow_resolution.Get());

    // csm cascade count & splits
    csm_num_cascades_ = std::min(r_csm_cascades.Get(), 4U);
    uint32_t csm_layers = csm_num_cascades_;

    if (csm_num_cascades_ == 4)
    {
        csm_splits_[0] = 15.0f;
        csm_splits_[1] = 45.0f;
        csm_splits_[2] = 100.0f;
        csm_splits_[3] = 200.0f;
    }
    else if (csm_num_cascades_ == 3)
    {
        csm_splits_[0] = 20.0f;
        csm_splits_[1] = 60.0f;
        csm_splits_[2] = 150.0f;
    }
    else if (csm_num_cascades_ == 2)
    {
        csm_splits_[0] = 30.0f;
        csm_splits_[1] = 90.0f;
    }
    else if (csm_num_cascades_ == 1)
    {
        csm_splits_[0] = 40.0f;
    }
    else // 0
    {
        csm_layers = 1;
        csm_resolution_ = 1; // dummy single-layer texture
    }

    // spotlight shadow count
    spotlight_shadow_count_ = std::min(r_shadow_count.Get(), MAX_SPOTLIGHT_SHADOWMAPS);
    uint32_t spotlight_layers = spotlight_shadow_count_;

    if (spotlight_layers == 0)
    {
        spotlight_layers = 1;
        spotlight_shadow_resolution_ = 1; // dummy
    }

    // shadow quality
    surface_shader_cfg_.shadow_sample_func = GetShadowSampleFunction(r_shadow_quality.Get());

    // create CSM texture array
    {
        wgpu::TextureDescriptor tex_desc{};
        tex_desc.dimension = wgpu::TextureDimension::e2D;
        tex_desc.size = {csm_resolution_, csm_resolution_, csm_layers};
        tex_desc.format = wgpu::TextureFormat::Depth32Float;
        tex_desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        tex_desc.label = "CSM texture array";
        csm_texture_ = device_.CreateTexture(&tex_desc);

        wgpu::TextureViewDescriptor view_desc{};
        view_desc.dimension = wgpu::TextureViewDimension::e2DArray;
        view_desc.arrayLayerCount = csm_layers;
        view_desc.label = "CSM texture array view";
        csm_texture_view_ = csm_texture_.CreateView(&view_desc);
    }

    // create individual CSM views
    for (uint32_t i = 0; i < csm_layers; ++i)
    {
        wgpu::TextureViewDescriptor view_desc{};
        view_desc.dimension = wgpu::TextureViewDimension::e2D;
        view_desc.baseArrayLayer = i;
        view_desc.arrayLayerCount = 1;
        view_desc.label = "CSM texture array layer view";
        csm_cascades_[i].texture_view = csm_texture_.CreateView(&view_desc);
    }

    // create spotlight shadow texture array
    {
        wgpu::TextureDescriptor tex_desc{};
        tex_desc.dimension = wgpu::TextureDimension::e2D;
        tex_desc.size = {spotlight_shadow_resolution_, spotlight_shadow_resolution_, spotlight_layers};
        tex_desc.format = wgpu::TextureFormat::Depth32Float;
        tex_desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        tex_desc.label = "Spotlight shadow texture array";
        spotlight_shadow_texture_ = device_.CreateTexture(&tex_desc);

        wgpu::TextureViewDescriptor view_desc{};
        view_desc.dimension = wgpu::TextureViewDimension::e2DArray;
        view_desc.arrayLayerCount = spotlight_layers;
        view_desc.label = "Spotlight shadow texture array view";
        spotlight_shadow_texture_view_ = spotlight_shadow_texture_.CreateView(&view_desc);
    }

    // create individual spotlight shadow views
    for (uint32_t i = 0; i < spotlight_layers; ++i)
    {
        wgpu::TextureViewDescriptor view_desc{};
        view_desc.dimension = wgpu::TextureViewDimension::e2D;
        view_desc.baseArrayLayer = i;
        view_desc.arrayLayerCount = 1;
        view_desc.label = "Spotlight shadow texture array layer view";
        spotlight_shadows_[i].texture_view = spotlight_shadow_texture_.CreateView(&view_desc);
    }

    // create shadow sampler
    if (!shadow_sampler_)
    {
        wgpu::SamplerDescriptor sampler_desc{};
        sampler_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
        sampler_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
        sampler_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
        sampler_desc.magFilter = wgpu::FilterMode::Linear;
        sampler_desc.minFilter = wgpu::FilterMode::Linear;
        sampler_desc.compare = wgpu::CompareFunction::LessEqual;
        sampler_desc.lodMinClamp = 0.0f;
        sampler_desc.lodMaxClamp = 1.0f;
        sampler_desc.label = "Comparison sampler";
        sampler_desc.mipmapFilter = wgpu::MipmapFilterMode::Undefined;
        shadow_sampler_ = device_.CreateSampler(&sampler_desc);
    }

    shadow_resources_setup_ = true;
}

void gfx::RendererWGPU::InvalidateShadowResources()
{
    InvalidateSurfaceGlobalBindGroup();
    InvalidateSurfacePipelines();
    csm_texture_ = nullptr;
    csm_texture_view_ = nullptr;
    spotlight_shadow_texture_ = nullptr;
    spotlight_shadow_texture_view_ = nullptr;

    for (auto& cascade : csm_cascades_)
    {
        cascade.texture_view = nullptr;
    }

    for (auto& shadowmap : spotlight_shadows_)
    {
        shadowmap.texture_view = nullptr;
    }

    shadow_resources_setup_ = false;

}

void gfx::RendererWGPU::CreateSurfaceGlobalResources()
{
    InvalidateSurfacePipelines();
    global_bind_group_layout_ = nullptr;
    global_bind_group_ = nullptr;

    // global bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 7> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
        uniforms_entry.buffer.type = wgpu::BufferBindingType::Uniform;
        uniforms_entry.buffer.minBindingSize = sizeof(GlobalUniformData);
        uniforms_entry.buffer.hasDynamicOffset = true;

        auto& shadow_sampler_entry = entries[1];
        shadow_sampler_entry.binding = 1;
        shadow_sampler_entry.visibility = wgpu::ShaderStage::Fragment;
        shadow_sampler_entry.sampler.type = wgpu::SamplerBindingType::Comparison;

        auto& csm_texture_entry = entries[2];
        csm_texture_entry.binding = 2;
        csm_texture_entry.visibility = wgpu::ShaderStage::Fragment;
        csm_texture_entry.texture.viewDimension = wgpu::TextureViewDimension::e2DArray;
        csm_texture_entry.texture.sampleType = wgpu::TextureSampleType::Depth;

        auto& light_buffer_entry = entries[3];
        light_buffer_entry.binding = 3;
        light_buffer_entry.visibility = wgpu::ShaderStage::Fragment;
        light_buffer_entry.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

        auto& visible_lights_entry = entries[4];
        visible_lights_entry.binding = 4;
        visible_lights_entry.visibility = wgpu::ShaderStage::Fragment;
        visible_lights_entry.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

        auto& shadow_texture_entry = entries[5];
        shadow_texture_entry.binding = 5;
        shadow_texture_entry.visibility = wgpu::ShaderStage::Fragment;
        shadow_texture_entry.texture.viewDimension = wgpu::TextureViewDimension::e2DArray;
        shadow_texture_entry.texture.sampleType = wgpu::TextureSampleType::Depth;

        auto& ao_texture_entry = entries[6];
        ao_texture_entry.binding = 6;
        ao_texture_entry.visibility = wgpu::ShaderStage::Fragment;
        ao_texture_entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
        ao_texture_entry.texture.sampleType = wgpu::TextureSampleType::UnfilterableFloat;

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Global bind group layout";
        global_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }

    // global uniforms buffer
    if (!global_buffer_) // no need to recreate this
    { 
        wgpu::BufferDescriptor desc{};
        desc.size = GLOBAL_BUFFER_STRIDE * MAX_PASSES;
        desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        desc.label = "Global uniform buffer";
        global_buffer_ = device_.CreateBuffer(&desc);
    }
}

void gfx::RendererWGPU::CreateSurfaceGlobalBindGroup()
{
    std::array<wgpu::BindGroupEntry, 7> entries;

    auto& uniforms_entry = entries[0];
    uniforms_entry.binding = 0;
    uniforms_entry.buffer = global_buffer_;
    uniforms_entry.size = sizeof(GlobalUniformData);

    auto& shadow_sampler_entry = entries[1];
    shadow_sampler_entry.binding = 1;
    shadow_sampler_entry.sampler = shadow_sampler_;

    auto& csm_texture_entry = entries[2];
    csm_texture_entry.binding = 2;
    csm_texture_entry.textureView = csm_texture_view_;

    auto& light_buffer_entry = entries[3];
    light_buffer_entry.binding = 3;
    light_buffer_entry.buffer = light_buffer_;
    light_buffer_entry.size = MAX_LIGHTS * sizeof(LightBufferData);

    auto& visible_lights_entry = entries[4];
    visible_lights_entry.binding = 4;
    visible_lights_entry.buffer = visible_lights_buffer_;
    visible_lights_entry.size = visible_lights_buffer_size_;

    auto& shadow_texture_entry = entries[5];
    shadow_texture_entry.binding = 5;
    shadow_texture_entry.textureView = spotlight_shadow_texture_view_;

    auto& ao_texture_entry = entries[6];
    ao_texture_entry.binding = 6;
    ao_texture_entry.textureView = ao_output_texture_view_ ? ao_output_texture_view_ : textures_.Get(GetWhiteTexture()).view;

    wgpu::BindGroupDescriptor desc{};
    desc.layout = global_bind_group_layout_;
    desc.entryCount = entries.size();
    desc.entries = entries.data();
    desc.label = "Global bind group";
    global_bind_group_ = device_.CreateBindGroup(&desc);
}

void gfx::RendererWGPU::InvalidateSurfaceGlobalBindGroup()
{
    global_bind_group_ = nullptr;
}

void gfx::RendererWGPU::CreateSurfaceMaterialResources()
{
    // material bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 2> entries;

        auto& sampler_entry = entries[0];
        sampler_entry.binding = 0;
        sampler_entry.visibility = wgpu::ShaderStage::Fragment;
        sampler_entry.sampler.type = wgpu::SamplerBindingType::Filtering;

        auto& color_texture_entry = entries[1];
        color_texture_entry.binding = 1;
        color_texture_entry.visibility = wgpu::ShaderStage::Fragment;
        color_texture_entry.texture.sampleType = wgpu::TextureSampleType::Float;
        color_texture_entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Material bind group layout";
        material_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }
}

void gfx::RendererWGPU::CreateSurfaceInstanceResources()
{
    // instance bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 1> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
        uniforms_entry.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        uniforms_entry.buffer.minBindingSize = sizeof(InstanceUniformData);

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Instance bind group layout";
        instance_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }

    // instance buffer properties
    instance_buffer_.usage |= wgpu::BufferUsage::Storage;

    // skeletal bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 1> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.visibility = wgpu::ShaderStage::Vertex;
        uniforms_entry.buffer.type = wgpu::BufferBindingType::Uniform;
        uniforms_entry.buffer.minBindingSize = MAX_BONES * sizeof(glm::mat4);

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Skeletal bind group layout";
        skeletal_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }

    // deform bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 3> entries;

        auto& sampler_entry = entries[0];
        sampler_entry.binding = 0;
        sampler_entry.visibility = wgpu::ShaderStage::Vertex;
        sampler_entry.sampler.type = wgpu::SamplerBindingType::Filtering;

        auto& texture_entry = entries[1];
        texture_entry.binding = 1;
        texture_entry.visibility = wgpu::ShaderStage::Vertex;
        texture_entry.texture.sampleType = wgpu::TextureSampleType::Float;
        texture_entry.texture.viewDimension = wgpu::TextureViewDimension::e3D;

        auto& info_entry = entries[2];
        info_entry.binding = 2;
        info_entry.visibility = wgpu::ShaderStage::Vertex;
        info_entry.buffer.type = wgpu::BufferBindingType::Uniform;
        info_entry.buffer.minBindingSize = sizeof(DeformInfoWGPU);

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Deform bind group layout";
        deform_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }
}

void gfx::RendererWGPU::CreateDepthResources()
{
    // global depth only bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 1> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
        uniforms_entry.buffer.type = wgpu::BufferBindingType::Uniform;
        uniforms_entry.buffer.minBindingSize = sizeof(GlobalUniformData);
        uniforms_entry.buffer.hasDynamicOffset = true;

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Global depth bind group layout";
        global_depth_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }

    // global depth bind group
    {
        std::array<wgpu::BindGroupEntry, 1> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.buffer = global_buffer_;
        uniforms_entry.size = sizeof(GlobalUniformData);

        wgpu::BindGroupDescriptor desc{};
        desc.layout = global_depth_bind_group_layout_;
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Global depth bind group";
        global_depth_bind_group_ = device_.CreateBindGroup(&desc);
    }

}

void gfx::RendererWGPU::CreateLightCullingResources()
{
    // light culling bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 4> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.visibility = wgpu::ShaderStage::Compute;
        uniforms_entry.buffer.type = wgpu::BufferBindingType::Uniform;
        uniforms_entry.buffer.minBindingSize = sizeof(LightCullingGlobalData);

        auto& depth_entry = entries[1];
        depth_entry.binding = 1;
        depth_entry.visibility = wgpu::ShaderStage::Compute;
        depth_entry.texture.sampleType = wgpu::TextureSampleType::Depth;
        depth_entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;

        auto& light_buffer_entry = entries[2];
        light_buffer_entry.binding = 2;
        light_buffer_entry.visibility = wgpu::ShaderStage::Compute;
        light_buffer_entry.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        light_buffer_entry.buffer.minBindingSize = MAX_LIGHTS * sizeof(LightBufferData);

        auto& visible_lights_entry = entries[3];
        visible_lights_entry.binding = 3;
        visible_lights_entry.visibility = wgpu::ShaderStage::Compute;
        visible_lights_entry.buffer.type = wgpu::BufferBindingType::Storage;
        visible_lights_entry.buffer.minBindingSize = sizeof(uint32_t);

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Light culling bind group layout";
        light_culling_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }

    // light culling global buffer
    {
        wgpu::BufferDescriptor desc{};
        desc.size = sizeof(LightCullingGlobalData);
        desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        desc.label = "Lights culling global buffer";
        light_culling_global_buffer_ = device_.CreateBuffer(&desc);
    }

    // light buffer
    {
        wgpu::BufferDescriptor desc{};
        desc.size = MAX_LIGHTS * sizeof(LightBufferData);
        desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
        desc.label = "Lights buffer";
        light_buffer_ = device_.CreateBuffer(&desc);
    }
}

void gfx::RendererWGPU::CreateLightCullingVisibleLightsBuffer()
{
    InvalidateLightCullingBindGroup();
    InvalidateSurfaceGlobalBindGroup();

    visible_lights_buffer_ = nullptr;
    
    visible_lights_buffer_size_ = std::max(1U, light_tiles_.x * light_tiles_.y * MAX_LIGHTS_PER_TILE) * sizeof(uint32_t);

    wgpu::BufferDescriptor desc{};
    desc.size = visible_lights_buffer_size_;
    desc.usage = wgpu::BufferUsage::Storage;
    desc.label = "Visible lights buffer";
    visible_lights_buffer_ = device_.CreateBuffer(&desc);
}

void gfx::RendererWGPU::CreateLightCullingBindGroup()
{
    light_culling_bind_group_ = nullptr;

    std::array<wgpu::BindGroupEntry, 4> entries;

    auto& uniforms_entry = entries[0];
    uniforms_entry.binding = 0;
    uniforms_entry.buffer = light_culling_global_buffer_;
    uniforms_entry.size = sizeof(LightCullingGlobalData);
    
    auto& depth_entry = entries[1];
    depth_entry.binding = 1;
    depth_entry.textureView = depth_texture_view_;

    auto& light_buffer_entry = entries[2];
    light_buffer_entry.binding = 2;
    light_buffer_entry.buffer = light_buffer_;
    light_buffer_entry.size = MAX_LIGHTS * sizeof(LightBufferData);

    auto& visible_lights_entry = entries[3];
    visible_lights_entry.binding = 3;
    visible_lights_entry.buffer = visible_lights_buffer_;
    visible_lights_entry.size = visible_lights_buffer_size_;

    wgpu::BindGroupDescriptor desc{};
    desc.layout = light_culling_bind_group_layout_;
    desc.entryCount = entries.size();
    desc.entries = entries.data();
    desc.label = "Light culling bind group";
    light_culling_bind_group_ = device_.CreateBindGroup(&desc);

}

void gfx::RendererWGPU::CreateLightCullingPipeline()
{
    constexpr std::string_view SHADER_SRC = SHADER_DEFS_WGSL R"WGSL(
const TILE_PIXELS = TILE_SIZE * TILE_SIZE;

struct GlobalData {
    screen_size: vec2u,
    tile_count: vec2u,
    proj: mat4x4f,
    inv_proj: mat4x4f,
    light_count: u32,
    _pad0: f32,
    _pad1: f32,
    _pad2: f32,
};

struct LightBufferData {
    view_pos: vec3f,
    radius: f32,
    color: vec3f,
    cos_inner: f32,
    view_dir: vec3f,
    cos_outer: f32,
    view_bounding_pos: vec3f,
    bounding_radius: f32,
    shadow_idx: u32,
    _pad0: f32,
    _pad1: f32,
    _pad2: f32,
};

@group(0) @binding(0) var<uniform> u_global: GlobalData;
@group(0) @binding(1) var depth_texture: texture_depth_2d;
@group(0) @binding(2) var<storage, read> u_lights: array<LightBufferData>;
@group(0) @binding(3) var<storage, read_write> u_visible: array<u32>;

// Dual atomic counters for the two list halves
var<workgroup> opaque_count: atomic<u32>;
var<workgroup> transparent_count: atomic<u32>;

var<workgroup> tile_min_depth_u32: atomic<u32>;
var<workgroup> tile_max_depth_u32: atomic<u32>;

// Shared frustum side planes (0..3) and far plane (4)
var<workgroup> side_and_far_planes: array<vec4f, 5>;
var<workgroup> opaque_near_plane: vec4f;
var<workgroup> transparent_near_plane: vec4f;

// Separate AABBs for Opaque (min_depth -> max_depth) and Transparent (0.0 -> max_depth)
var<workgroup> opaque_aabb_min: vec3f;
var<workgroup> opaque_aabb_max: vec3f;
var<workgroup> transparent_aabb_min: vec3f;
var<workgroup> transparent_aabb_max: vec3f;

fn sphere_in_sides_and_far(pos: vec3f, radius: f32) -> bool {
    for (var i = 0u; i < 4u; i = i + 1u) {
        if (dot(side_and_far_planes[i].xyz, pos) < -radius) {
            return false;
        }
    }
    let p_far = side_and_far_planes[4];
    if (dot(p_far.xyz, pos) + p_far.w < -radius) {
        return false;
    }
    return true;
}

fn sphere_pass_plane(pos: vec3f, radius: f32, plane: vec4f) -> bool {
    return (dot(plane.xyz, pos) + plane.w) >= -radius;
}

// Unprojects an NDC point (x, y) at a specific depth (z) into View Space
fn ndc_to_view(ndc: vec2f, depth: f32, inv_proj: mat4x4f) -> vec3f {
    let clip_pos = vec4f(ndc.x, ndc.y, depth, 1.0);
    let view_pos_homogenous = inv_proj * clip_pos;
    return view_pos_homogenous.xyz / view_pos_homogenous.w;
}

fn sphere_intersects_aabb(light_view_pos: vec3f, radius: f32, aabb_min: vec3f, aabb_max: vec3f) -> bool {
    let closest_point = clamp(light_view_pos, aabb_min, aabb_max);
    let delta = light_view_pos - closest_point;
    return dot(delta, delta) <= (radius * radius);
}

@compute @workgroup_size(TILE_SIZE, TILE_SIZE)
fn cull_lights(
    @builtin(local_invocation_id) lid: vec3u,
    @builtin(workgroup_id) wgid: vec3u
) {
    let tile_index = wgid.y * u_global.tile_count.x + wgid.x;
    let local_index = lid.y * TILE_SIZE + lid.x;
    let half_max_lights = MAX_LIGHTS_PER_TILE / 2u;

    // Initialize atomics in a single thread
    if (local_index == 0u) {
        atomicStore(&opaque_count, 0u);
        atomicStore(&transparent_count, 0u);
        atomicStore(&tile_min_depth_u32, 0xFFFFFFFFu); // Max u32 represents 1.0f
        atomicStore(&tile_max_depth_u32, 0u);          // 0 represents 0.0f
    }
    workgroupBarrier();

    // Load depths with edge-clamping to prevent artificial far-plane stretching
    let max_coord = u_global.screen_size - vec2u(1u);
    let pixel = min(wgid.xy * TILE_SIZE + lid.xy, max_coord);
    let depth = textureLoad(depth_texture, vec2i(pixel), 0);

    let depth_u32 = bitcast<u32>(depth);
    atomicMin(&tile_min_depth_u32, depth_u32);
    atomicMax(&tile_max_depth_u32, depth_u32);
    workgroupBarrier();

    // Compute Tile Frustums and AABBs natively in View Space
    if (local_index == 0u) {
        let tile_min_depth = bitcast<f32>(atomicLoad(&tile_min_depth_u32));
        let tile_max_depth = bitcast<f32>(atomicLoad(&tile_max_depth_u32));

        let tile = wgid.xy;
        let tile_scale = vec2f(u_global.tile_count);
        let step_min = vec2f(tile) / tile_scale;
        let step_max = vec2f(tile + vec2u(1u)) / tile_scale;
        let ndc_x_min = -1.0 + 2.0 * step_min.x;
        let ndc_x_max = -1.0 + 2.0 * step_max.x;
        let ndc_y_max =  1.0 - 2.0 * step_min.y;
        let ndc_y_min =  1.0 - 2.0 * step_max.y;

        // Clip space plane definitions
        let p0 = vec4f( 1.0,  0.0,  0.0, -ndc_x_min); // Left
        let p1 = vec4f(-1.0,  0.0,  0.0,  ndc_x_max); // Right
        let p2 = vec4f( 0.0,  1.0,  0.0, -ndc_y_min); // Bottom
        let p3 = vec4f( 0.0, -1.0,  0.0,  ndc_y_max); // Top
        let p_far         = vec4f( 0.0,  0.0, -1.0,  tile_max_depth); // Far (ZO layout)
        let p_near_opaque = vec4f( 0.0,  0.0,  1.0, -tile_min_depth); // Near Opaque
        let p_near_trans  = vec4f( 0.0,  0.0,  1.0,  0.0);            // Near Transparent (Depth = 0.0)

        let transform_mat = transpose(u_global.proj);
        let raw_sides = array<vec4f, 4>(p0, p1, p2, p3);

        for (var i = 0u; i < 4u; i = i + 1u) {
            var p = transform_mat * raw_sides[i];
            p = p / length(p.xyz);
            p.w = 0.0; // Prevent FP precision drift on side planes
            side_and_far_planes[i] = p;
        }

        var far_p = transform_mat * p_far;
        side_and_far_planes[4] = far_p / length(far_p.xyz);

        var n_op = transform_mat * p_near_opaque;
        opaque_near_plane = n_op / length(n_op.xyz);

        var n_tr = transform_mat * p_near_trans;
        transparent_near_plane = n_tr / length(n_tr.xyz);

        // Unproject 4 corners at Z = 0.0 (Transparent Near Plane)
        let f_tr_0 = ndc_to_view(vec2f(ndc_x_min, ndc_y_min), 0.0, u_global.inv_proj);
        let f_tr_1 = ndc_to_view(vec2f(ndc_x_max, ndc_y_min), 0.0, u_global.inv_proj);
        let f_tr_2 = ndc_to_view(vec2f(ndc_x_min, ndc_y_max), 0.0, u_global.inv_proj);
        let f_tr_3 = ndc_to_view(vec2f(ndc_x_max, ndc_y_max), 0.0, u_global.inv_proj);

        // Unproject 4 corners at Opaque Near Depth Plane
        let f_op_0 = ndc_to_view(vec2f(ndc_x_min, ndc_y_min), tile_min_depth, u_global.inv_proj);
        let f_op_1 = ndc_to_view(vec2f(ndc_x_max, ndc_y_min), tile_min_depth, u_global.inv_proj);
        let f_op_2 = ndc_to_view(vec2f(ndc_x_min, ndc_y_max), tile_min_depth, u_global.inv_proj);
        let f_op_3 = ndc_to_view(vec2f(ndc_x_max, ndc_y_max), tile_min_depth, u_global.inv_proj);

        // Unproject 4 corners at Far Depth Plane
        let f_far_0 = ndc_to_view(vec2f(ndc_x_min, ndc_y_min), tile_max_depth, u_global.inv_proj);
        let f_far_1 = ndc_to_view(vec2f(ndc_x_max, ndc_y_min), tile_max_depth, u_global.inv_proj);
        let f_far_2 = ndc_to_view(vec2f(ndc_x_min, ndc_y_max), tile_max_depth, u_global.inv_proj);
        let f_far_3 = ndc_to_view(vec2f(ndc_x_max, ndc_y_max), tile_max_depth, u_global.inv_proj);

        let far_min = min(min(f_far_0, f_far_1), min(f_far_2, f_far_3));
        let far_max = max(max(f_far_0, f_far_1), max(f_far_2, f_far_3));

        // Construct Opaque AABB
        let op_near_min = min(min(f_op_0, f_op_1), min(f_op_2, f_op_3));
        let op_near_max = max(max(f_op_0, f_op_1), max(f_op_2, f_op_3));
        opaque_aabb_min = min(op_near_min, far_min);
        opaque_aabb_max = max(op_near_max, far_max);
        opaque_aabb_min.z = min(f_op_0.z, f_far_0.z);
        opaque_aabb_max.z = max(f_op_0.z, f_far_0.z);

        // Construct Transparent AABB
        let tr_near_min = min(min(f_tr_0, f_tr_1), min(f_tr_2, f_tr_3));
        let tr_near_max = max(max(f_tr_0, f_tr_1), max(f_tr_2, f_tr_3));
        transparent_aabb_min = min(tr_near_min, far_min);
        transparent_aabb_max = max(tr_near_max, far_max);
        transparent_aabb_min.z = min(f_tr_0.z, f_far_0.z);
        transparent_aabb_max.z = max(f_tr_0.z, f_far_0.z);
    }
    workgroupBarrier();

    // Test Lights in View Space
    for (var i = local_index; i < u_global.light_count; i += TILE_PIXELS) {
        let light = u_lights[i];
        let pos = light.view_bounding_pos;
        let rad = light.bounding_radius;

        // Step 1: Broad check against Extended Frustum/AABB (ignoring min depth Z)
        if (sphere_in_sides_and_far(pos, rad) &&
            sphere_pass_plane(pos, rad, transparent_near_plane) &&
            sphere_intersects_aabb(pos, rad, transparent_aabb_min, transparent_aabb_max)) 
        {
            // Step 2: Decide list routing based on min depth Z
            if (sphere_pass_plane(pos, rad, opaque_near_plane) &&
                sphere_intersects_aabb(pos, rad, opaque_aabb_min, opaque_aabb_max)) 
            {
                // Route to First Half (Opaque List)
                let index = atomicAdd(&opaque_count, 1u);
                if (index < half_max_lights) {
                    u_visible[tile_index * MAX_LIGHTS_PER_TILE + index] = i;
                }
            } else {
                // Route to Second Half (Transparent Supplementary List)
                let index = atomicAdd(&transparent_count, 1u);
                if (index < half_max_lights) {
                    u_visible[tile_index * MAX_LIGHTS_PER_TILE + half_max_lights + index] = i;
                }
            }
        }
    }
    workgroupBarrier();

    // Write Dual Sentinels
    if (local_index == 0u) {
        let base = tile_index * MAX_LIGHTS_PER_TILE;

        // Terminate Opaque List (First Half)
        let op_count = min(atomicLoad(&opaque_count), half_max_lights);
        if (op_count < half_max_lights) {
            u_visible[base + op_count] = INVALID_LIGHT_IDX;
        }

        // Terminate Transparent List (Second Half)
        let tr_count = min(atomicLoad(&transparent_count), half_max_lights);
        if (tr_count < half_max_lights) {
            u_visible[base + half_max_lights + tr_count] = INVALID_LIGHT_IDX;
        }
    }
}
    )WGSL";

    // make shader
    wgpu::ShaderModuleDescriptor shader_desc{};
    shader_desc.label = "Light culling shader";
    wgpu::ShaderSourceWGSL shader_src{};
    shader_src.code = SHADER_SRC;
    shader_desc.nextInChain = &shader_src;
    auto shader_module = device_.CreateShaderModule(&shader_desc);

    wgpu::ComputePipelineDescriptor desc{};
    desc.label = "Light culling pipeline";
    
    desc.compute.module = shader_module;
    desc.compute.entryPoint = "cull_lights";
   
    wgpu::PipelineLayoutDescriptor layout_desc{};
    layout_desc.bindGroupLayoutCount = 1;
    layout_desc.bindGroupLayouts = &light_culling_bind_group_layout_;
    desc.layout = device_.CreatePipelineLayout(&layout_desc);

    light_culling_pipeline_ = device_.CreateComputePipeline(&desc);
}

void gfx::RendererWGPU::InvalidateLightCullingBindGroup()
{
    light_culling_bind_group_ = nullptr;
}

void gfx::RendererWGPU::CreateCoronaResources()
{
    // corona bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 2> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.visibility = wgpu::ShaderStage::Vertex;
        uniforms_entry.buffer.type = wgpu::BufferBindingType::Uniform;
        uniforms_entry.buffer.minBindingSize = sizeof(CoronaGlobalData);

        auto& buffer_entry = entries[1];
        buffer_entry.binding = 1;
        buffer_entry.visibility = wgpu::ShaderStage::Vertex;
        buffer_entry.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        buffer_entry.buffer.minBindingSize = sizeof(CoronaBufferData);

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Corona bind group layout";
        corona_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }

    // corona global buffer
    {
        wgpu::BufferDescriptor desc{};
        desc.size = sizeof(CoronaGlobalData);
        desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        desc.label = "Corona global buffer";
        corona_global_buffer_ = device_.CreateBuffer(&desc);
    }

    // corona buffer
    corona_buffer_.usage |= wgpu::BufferUsage::Storage;

}

void gfx::RendererWGPU::CreateCoronaBindGroup()
{
    corona_bind_group_ = nullptr;

    std::array<wgpu::BindGroupEntry, 2> entries;

    auto& uniforms_entry = entries[0];
    uniforms_entry.binding = 0;
    uniforms_entry.buffer = corona_global_buffer_;
    uniforms_entry.size = sizeof(CoronaGlobalData);

    auto& buffer_entry = entries[1];
    buffer_entry.binding = 1;
    buffer_entry.buffer = corona_buffer_.buffer;
    buffer_entry.size = corona_buffer_.buffer.GetSize();

    wgpu::BindGroupDescriptor desc{};
    desc.layout = corona_bind_group_layout_;
    desc.entryCount = entries.size();
    desc.entries = entries.data();
    desc.label = "Corona bind group";
    corona_bind_group_ = device_.CreateBindGroup(&desc);
}

void gfx::RendererWGPU::CreateCoronaPipeline()
{
    constexpr std::string_view SHADER_SRC = R"WGSL(
        struct VertexInput {
            @builtin(vertex_index) vertex_id: u32,
            @builtin(instance_index) instance_id: u32,
        };

        struct VertexOutput {
	        @builtin(position) position: vec4f,
	        @location(0) color: vec3f,
	        @location(1) quad_pos: vec2f,
        };

        struct GlobalData {
            proj: mat4x4f,
            scale_xy: vec2f,
            _pad0: f32,
            _pad1: f32,
        };

        struct CoronaBufferData {
            view_pos: vec3f,
            size: f32,
            view_dir: vec3f,
            _pad0: f32,
            color: vec4f,
        };

        @group(0) @binding(0) var<uniform> u_global: GlobalData;
        @group(0) @binding(1) var<storage> u_coronas: array<CoronaBufferData>;

        const VERTICES = array<vec2f, 4>(
            vec2f(-1.0f, -1.0f),
            vec2f( 1.0f, -1.0f),
            vec2f(-1.0f,  1.0f),
            vec2f( 1.0f,  1.0f),
        );

        @vertex
        fn vs_main(in: VertexInput) -> VertexOutput {
            let corona = u_coronas[in.instance_id];

            let dist = length(corona.view_pos);
            let offset_pos = corona.view_pos * (1.0 - (0.2 / dist));
            
            let clip_pos = u_global.proj * vec4f(offset_pos, 1.0);

            let angle_factor = sqrt(max(corona.view_dir.z, 0.0));
            let angle_mult = mix(0.0, 0.7, angle_factor);
            let angle_scale = mix(0.3, 1.0, angle_factor);

            let scale_dist = max(0.1, dist);
            const a = 1.0;
            const b = 0.15;
            let scale = 0.2 * angle_scale * (1.0 / (a + b * scale_dist)) * corona.size;
            
            let quad_pos = VERTICES[in.vertex_id];
            
            var out: VertexOutput;
            out.position = vec4f(clip_pos.xy + quad_pos * u_global.scale_xy * (scale * clip_pos.w), clip_pos.zw);
            out.quad_pos = quad_pos;
            out.color = corona.color.rgb * angle_mult;
            return out;
        }

        @fragment
        fn fs_main(in: VertexOutput) -> @location(0) vec4f {
            let two_quad_pos = in.quad_pos * 2.0;
            let r2 = dot(two_quad_pos, two_quad_pos);

            if (r2 > 4.0) {
                return vec4f(0.0);
            }

            let core = exp(-18.0 * r2);
            let halo = exp(-3.5 * r2);

            let alpha = core + 0.35 * halo;
            return vec4f(in.color * alpha, 1.0);
        }
    )WGSL";

    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "Corona pipeline";

    // shader
    wgpu::ShaderModuleDescriptor shader_desc{};
    shader_desc.label = "Corona shader";
    wgpu::ShaderSourceWGSL shader_src{};
    shader_src.code = SHADER_SRC;
    shader_desc.nextInChain = &shader_src;
    auto shader_module = device_.CreateShaderModule(&shader_desc);

    // vertex
    desc.vertex.bufferCount = 0;
    desc.vertex.buffers = nullptr;
    desc.vertex.module = shader_module;
    desc.vertex.entryPoint = "vs_main";

    // assembly
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
    desc.primitive.frontFace = wgpu::FrontFace::CCW;
    desc.primitive.cullMode = wgpu::CullMode::None;

    // blending
    wgpu::BlendState blend{};
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::One;
    blend.color.operation = wgpu::BlendOperation::Add;
    //
    blend.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blend.alpha.dstFactor = wgpu::BlendFactor::One;
    blend.alpha.operation = wgpu::BlendOperation::Add;

    // color
    wgpu::ColorTargetState color{};
    color.format = surface_format_;
    color.blend = &blend;
    color.writeMask = wgpu::ColorWriteMask::All;

    // depth
    wgpu::DepthStencilState depth_state{};
    depth_state.depthCompare = wgpu::CompareFunction::LessEqual;
    depth_state.depthWriteEnabled = wgpu::OptionalBool::False;
    depth_state.format = wgpu::TextureFormat::Depth24Plus;
    desc.depthStencil = &depth_state;

    // fragment
    wgpu::FragmentState fragment{};
    fragment.module = shader_module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 1;
    fragment.targets = &color;
    desc.fragment = &fragment;

    // multisampling
    desc.multisample.count = msaa_samples_;

    // layout
    wgpu::PipelineLayoutDescriptor layout_desc{};
    layout_desc.bindGroupLayoutCount = 1;
    layout_desc.bindGroupLayouts = &corona_bind_group_layout_;
    layout_desc.label = "Corona pipeline layout";
    desc.layout = device_.CreatePipelineLayout(&layout_desc);

    corona_pipeline_ = device_.CreateRenderPipeline(&desc);
}

void gfx::RendererWGPU::CreateAOResources()
{
    // AO bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 4> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.visibility = wgpu::ShaderStage::Compute;
        uniforms_entry.buffer.type = wgpu::BufferBindingType::Uniform;
        uniforms_entry.buffer.minBindingSize = sizeof(AOGlobalData);

        auto& depth_texture_entry = entries[1];
        depth_texture_entry.binding = 1;
        depth_texture_entry.visibility = wgpu::ShaderStage::Compute;
        depth_texture_entry.texture.sampleType = wgpu::TextureSampleType::Depth;
        depth_texture_entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;

        auto& normal_texture_entry = entries[2];
        normal_texture_entry.binding = 2;
        normal_texture_entry.visibility = wgpu::ShaderStage::Compute;
        normal_texture_entry.texture.sampleType = wgpu::TextureSampleType::UnfilterableFloat;
        normal_texture_entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;

        auto& output_texture_entry = entries[3];
        output_texture_entry.binding = 3;
        output_texture_entry.visibility = wgpu::ShaderStage::Compute;
        output_texture_entry.storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
        output_texture_entry.storageTexture.format = wgpu::TextureFormat::RGBA8Unorm;
        output_texture_entry.storageTexture.viewDimension = wgpu::TextureViewDimension::e2D;

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "AO bind group layout";
        ao_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }

    // AO global buffer
    {
        wgpu::BufferDescriptor desc{};
        desc.size = sizeof(AOGlobalData);
        desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        desc.label = "AO global buffer";
        ao_global_buffer_ = device_.CreateBuffer(&desc);
    }
}

void gfx::RendererWGPU::CreateAOBindGroup()
{
    ao_bind_group_ = nullptr;

    std::array<wgpu::BindGroupEntry, 4> entries;

    auto& uniforms_entry = entries[0];
    uniforms_entry.binding = 0;
    uniforms_entry.buffer = ao_global_buffer_;
    uniforms_entry.size = sizeof(AOGlobalData);

    auto& depth_texture_entry = entries[1];
    depth_texture_entry.binding = 1;
    depth_texture_entry.textureView = depth_texture_view_;

    auto& normal_texture_entry = entries[2];
    normal_texture_entry.binding = 2;
    normal_texture_entry.textureView = normal_texture_view_;

    auto& output_texture_entry = entries[3];
    output_texture_entry.binding = 3;
    output_texture_entry.textureView = ao_output_texture_view_;

    wgpu::BindGroupDescriptor desc{};
    desc.layout = ao_bind_group_layout_;
    desc.entryCount = entries.size();
    desc.entries = entries.data();
    desc.label = "AO bind group";
    ao_bind_group_ = device_.CreateBindGroup(&desc);
}

void gfx::RendererWGPU::InvalidateAOBindGroup()
{
    ao_bind_group_ = nullptr;
}

void gfx::RendererWGPU::CreateAOPipeline()
{
    ao_pipeline_ = nullptr;

    if (!surface_shader_cfg_.ao)
    {
        return;
    }

    constexpr std::string_view SHADER_SRC = SHADER_DEFS_WGSL R"WGSL(
const WORKGROUP_SIZE = 16u;
const PI = 3.14159265359;

struct AOGlobalData {
    inv_proj: mat4x4<f32>,
    proj: mat4x4<f32>,
    viewport_size: vec2<f32>,
    radius: f32,          // World-space AO radius (Recommended: 0.5 - 1.5)
    sample_count: u32,    // Replaces slice_count (Recommended: 12u - 16u)
    steps_per_slice: u32, // Unused in SSAO (kept for layout compatibility)
    frame_index: u32,     // Spatio-temporal noise frame counter
    bias: f32,            // Replaces _pad0: self-shadowing acne bias (Recommended: 0.025)
    intensity: f32,       // Replaces _pad1: occlusion darkening strength (Recommended: 1.5 - 2.0)
};

@group(0) @binding(0) var<uniform> params: AOGlobalData;
@group(0) @binding(1) var depth_texture: texture_depth_2d;
@group(0) @binding(2) var normal_texture: texture_2d<f32>;
@group(0) @binding(3) var output_texture: texture_storage_2d<rgba8unorm, write>;

// Interleaved Gradient Noise for spatial jittering
fn interleaved_gradient_noise(pixel_pos: vec2<f32>, frame: u32) -> f32 {
    let frame_offset = f32(frame % 16u) * 0.0625;
    let pos = pixel_pos + vec2<f32>(frame_offset * 5.588238, frame_offset * 3.588238);
    let magic = vec3<f32>(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(pos, magic.xy)));
}

// Reconstruct View Space Position from perspectiveRH_ZO clip space depth
fn get_view_pos(uv: vec2<f32>, depth: f32) -> vec3<f32> {
    let clip_xy = vec2<f32>(uv.x * 2.0 - 1.0, (1.0 - uv.y) * 2.0 - 1.0);
    let clip = vec4<f32>(clip_xy, depth, 1.0);
    let view_h = params.inv_proj * clip;
    return view_h.xyz / view_h.w;
}

// Procedural Fibonacci Hemisphere: Generates optimal uniform kernel samples on the fly
fn get_hemisphere_sample(index: u32, num_samples: u32) -> vec3<f32> {
    let f_idx = f32(index);
    let f_num = f32(num_samples);

    // Golden ratio spiral angle
    let phi = f_idx * 2.399963229728653;

    // Uniform distribution over hemisphere along Z [0, 1]
    let cos_theta = 1.0 - (f_idx + 0.5) / f_num;
    let sin_theta = sqrt(1.0 - cos_theta * cos_theta);

    let unscaled_pos = vec3<f32>(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

    // Quadratic scale distribution: clusters samples closer to the surface origin
    let frac = (f_idx + 1.0) / f_num;
    let scale = mix(0.1, 1.0, frac * frac);

    return unscaled_pos * scale;
}

@compute @workgroup_size(WORKGROUP_SIZE, WORKGROUP_SIZE)
fn ao(@builtin(global_invocation_id) gid: vec3u) {
    let pixel = vec2<i32>(gid.xy);
    let dims = vec2<i32>(params.viewport_size);
    if (pixel.x >= dims.x || pixel.y >= dims.y) {
        return;
    }

    // Fetch depth & skip background/sky pixels
    let depth = textureLoad(depth_texture, pixel, 0);
    if (depth >= 0.99999) {
        textureStore(output_texture, pixel, vec4<f32>(1.0, 0.0, 0.0, 1.0));
        return;
    }

    let uv = (vec2<f32>(pixel) + 0.5) / params.viewport_size;
    let V_pos = get_view_pos(uv, depth);

    // Unpack normal and ensure valid fallback
    let raw_normal = textureLoad(normal_texture, pixel, 0).rgb;
    var N = normalize(raw_normal * 2.0 - 1.0);
    if (length(raw_normal) < 0.001) {
        N = normalize(-V_pos);
    }

    // Safe fallback values in case CPU uniforms are unassigned (0.0)
    let num_samples = select(params.sample_count, 16u, params.sample_count < 4u);
    let bias = select(params.bias, 0.025, params.bias <= 0.0001);
    let intensity = select(params.intensity, 1.5, params.intensity <= 0.0001);

    // Build orthonormal TBN basis oriented along normal N
    var up = vec3<f32>(0.0, 0.0, 1.0);
    if (abs(N.z) > 0.999) {
        up = vec3<f32>(1.0, 0.0, 0.0);
    }
    let tangent_base = normalize(cross(up, N));
    let bitangent_base = cross(N, tangent_base);

    // Rotate basis using Interleaved Gradient Noise to trade banding for high-frequency noise
    let noise_angle = interleaved_gradient_noise(vec2<f32>(pixel), params.frame_index) * 2.0 * PI;
    let cos_a = cos(noise_angle);
    let sin_a = sin(noise_angle);
    let tangent = tangent_base * cos_a + bitangent_base * sin_a;
    let bitangent = bitangent_base * cos_a - tangent_base * sin_a;
    let TBN = mat3x3<f32>(tangent, bitangent, N);

    var occlusion = 0.0;

    for (var s = 0u; s < num_samples; s = s + 1u) {
        // Orient sample along surface normal and scale by world radius
        let sample_vec = TBN * get_hemisphere_sample(s, num_samples);
        let sample_pos = V_pos + sample_vec * params.radius;

        // Project 3D sample point to 2D screen UV coordinates
        let clip_pos = params.proj * vec4<f32>(sample_pos, 1.0);
        let ndc = clip_pos.xy / clip_pos.w;
        let sample_uv = vec2<f32>(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);

        // Skip samples that project outside the viewport
        if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0) {
            continue;
        }

        // Fetch occluder depth and reconstruct its view-space Z
        let sample_pixel = vec2<i32>(sample_uv * params.viewport_size);
        let occluder_depth = textureLoad(depth_texture, sample_pixel, 0);
        let occluder_pos = get_view_pos(sample_uv, occluder_depth);

        // Range check: prevent foreground objects from casting halos on background walls
        let range_diff = abs(V_pos.z - occluder_pos.z);
        let range_check = smoothstep(0.0, 1.0, params.radius / max(0.0001, range_diff));

        // Depth check (RH coordinate system: closer to camera = less negative Z)
        if (occluder_pos.z >= sample_pos.z + bias) {
            occlusion += range_check;
        }
    }

    // Convert accumulated occlusion into visibility [0.0 = dark, 1.0 = white]
    let ao_visibility = 1.0 - (occlusion / f32(num_samples)) * intensity;
    let final_ao = clamp(ao_visibility, 0.0, 1.0);

    textureStore(output_texture, pixel, vec4<f32>(final_ao, 0.0, 0.0, 1.0));
}
    )WGSL";

    // make shader
    wgpu::ShaderModuleDescriptor shader_desc{};
    shader_desc.label = "AO shader";
    wgpu::ShaderSourceWGSL shader_src{};
    shader_src.code = SHADER_SRC;
    shader_desc.nextInChain = &shader_src;
    auto shader_module = device_.CreateShaderModule(&shader_desc);

    wgpu::ComputePipelineDescriptor desc{};
    desc.label = "AO pipeline";

    desc.compute.module = shader_module;
    desc.compute.entryPoint = "ao";

    wgpu::PipelineLayoutDescriptor layout_desc{};
    layout_desc.bindGroupLayoutCount = 1;
    layout_desc.bindGroupLayouts = &ao_bind_group_layout_;
    desc.layout = device_.CreatePipelineLayout(&layout_desc);

    ao_pipeline_ = device_.CreateComputePipeline(&desc);
}

void gfx::RendererWGPU::InvalidateAOPipeline()
{
    ao_pipeline_ = nullptr;
}

void gfx::RendererWGPU::CreateFXAAResources()
{
    // FXAA bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 2> entries;

        auto& sampler_entry = entries[0];
        sampler_entry.binding = 0;
        sampler_entry.visibility = wgpu::ShaderStage::Fragment;
        sampler_entry.sampler.type = wgpu::SamplerBindingType::Filtering;

        auto& texture_entry = entries[1];
        texture_entry.binding = 1;
        texture_entry.visibility = wgpu::ShaderStage::Fragment;
        texture_entry.texture.sampleType = wgpu::TextureSampleType::Float;
        texture_entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "FXAA bind group layout";
        fxaa_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }
}

void gfx::RendererWGPU::CreateFXAABindGroup()
{
    fxaa_bind_group_ = nullptr;

    std::array<wgpu::BindGroupEntry, 2> entries;

    auto& sampler_entry = entries[0];
    sampler_entry.binding = 0;
    sampler_entry.sampler = GetSampler(true, false);

    auto& texture_entry = entries[1];
    texture_entry.binding = 1;
    texture_entry.textureView = color_texture_view_;

    wgpu::BindGroupDescriptor desc{};
    desc.layout = fxaa_bind_group_layout_;
    desc.entryCount = entries.size();
    desc.entries = entries.data();
    desc.label = "FXAA bind group";
    fxaa_bind_group_ = device_.CreateBindGroup(&desc);
}

void gfx::RendererWGPU::InvalidateFXAABindGroup()
{
    fxaa_bind_group_ = nullptr;
}

void gfx::RendererWGPU::CreateFXAAPipeline()
{
    constexpr std::string_view SHADER_SRC = R"WGSL(
        struct VertexInput {
            @builtin(vertex_index) vertex_id: u32,
        };

        struct VertexOutput {
	        @builtin(position) position: vec4f,
        };

        @group(0) @binding(0) var linear_sampler: sampler;
        @group(0) @binding(1) var color_texture: texture_2d<f32>;

        const VERTICES = array<vec2f, 4>(
            vec2f(-1.0f, -1.0f),
            vec2f( 1.0f, -1.0f),
            vec2f(-1.0f,  1.0f),
            vec2f( 1.0f,  1.0f),
        );

        @vertex
        fn vs_main(in: VertexInput) -> VertexOutput {
            var out: VertexOutput;
            out.position = vec4f(VERTICES[in.vertex_id], 0.0, 1.0);
            return out;
        }

// FXAA Quality Settings
const FXAA_EDGE_THRESHOLD: f32 = 0.125;      // Minimum local contrast required to apply AA
const FXAA_EDGE_THRESHOLD_MIN: f32 = 0.0312; // Trims algorithm from processing dark areas
const FXAA_SEARCH_STEPS: i32 = 12;           // Maximum number of search steps along the edge
const FXAA_SUBPIXEL_QUALITY: f32 = 0.75;     // Controls removal of sub-pixel aliasing (0.0 to 1.0)

fn rgb_to_luma(rgb: vec3f) -> f32 {
    // Standard Rec. 709 luminance weights (green weighted heavily for human vision sensitivity)
    return dot(rgb, vec3f(0.299, 0.587, 0.114));
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // 1. Calculate screen coordinates and inverse texture dimensions
    let tex_size = vec2f(textureDimensions(color_texture));
    let inv_tex_size = 1.0 / tex_size;
    let uv = in.position.xy * inv_tex_size;

    // 2. Sample center and 4 cardinal neighbors (using explicit mip level 0.0)
    let color_center = textureSampleLevel(color_texture, linear_sampler, uv, 0.0).rgb;
    let luma_center  = rgb_to_luma(color_center);

    let luma_n = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv + vec2f( 0.0, -inv_tex_size.y), 0.0).rgb);
    let luma_s = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv + vec2f( 0.0,  inv_tex_size.y), 0.0).rgb);
    let luma_e = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv + vec2f( inv_tex_size.x,  0.0), 0.0).rgb);
    let luma_w = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv + vec2f(-inv_tex_size.x,  0.0), 0.0).rgb);

    // Find min and max luminance around the pixel
    let luma_min = min(luma_center, min(min(luma_n, luma_s), min(luma_e, luma_w)));
    let luma_max = max(luma_center, max(max(luma_n, luma_s), max(luma_e, luma_w)));
    let luma_range = luma_max - luma_min;

    // Early exit if local contrast is below threshold (pixel is not on an edge)
    if (luma_range < max(FXAA_EDGE_THRESHOLD_MIN, luma_max * FXAA_EDGE_THRESHOLD)) {
        return vec4f(color_center, 1.0);
    }

    // 3. Sample 4 diagonal neighbors to determine edge orientation
    let luma_nw = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv + vec2f(-inv_tex_size.x, -inv_tex_size.y), 0.0).rgb);
    let luma_ne = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv + vec2f( inv_tex_size.x, -inv_tex_size.y), 0.0).rgb);
    let luma_sw = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv + vec2f(-inv_tex_size.x,  inv_tex_size.y), 0.0).rgb);
    let luma_se = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv + vec2f( inv_tex_size.x,  inv_tex_size.y), 0.0).rgb);

    let luma_down_up    = luma_n + luma_s;
    let luma_left_right = luma_w + luma_e;

    // Sobel-like edge detection filter for horizontal vs vertical
    let edge_horizontal = abs(-2.0 * luma_w + luma_nw + luma_sw) +
                          abs(-2.0 * luma_center + luma_down_up) * 2.0 +
                          abs(-2.0 * luma_e + luma_ne + luma_se);

    let edge_vertical   = abs(-2.0 * luma_n + luma_nw + luma_ne) +
                          abs(-2.0 * luma_center + luma_left_right) * 2.0 +
                          abs(-2.0 * luma_s + luma_sw + luma_se);

    let is_horizontal = (edge_horizontal >= edge_vertical);

    // 4. Determine step direction and initial edge offset
    var step_length = select(inv_tex_size.x, inv_tex_size.y, is_horizontal);
    let luma_1 = select(luma_w, luma_n, is_horizontal);
    let luma_2 = select(luma_e, luma_s, is_horizontal);

    let gradient_1 = abs(luma_1 - luma_center);
    let gradient_2 = abs(luma_2 - luma_center);
    let is_1_steeper = gradient_1 >= gradient_2;

    let gradient_scaled = 0.25 * max(gradient_1, gradient_2);

    // Step size along the edge (orthogonal to gradient direction)
    let step_offset = select(
        vec2f(0.0, inv_tex_size.y),
        vec2f(inv_tex_size.x, 0.0),
        is_horizontal
    );

    var luma_local_average = 0.0;
    if (is_1_steeper) {
        step_length = -step_length;
        luma_local_average = 0.5 * (luma_1 + luma_center);
    } else {
        luma_local_average = 0.5 * (luma_2 + luma_center);
    }

    // Shift UV coordinate half a pixel towards the edge
    var current_uv = uv;
    if (is_horizontal) {
        current_uv.y += step_length * 0.5;
    } else {
        current_uv.x += step_length * 0.5;
    }

    // 5. Explore along the edge in both directions to find endpoints
    var uv1 = current_uv - step_offset;
    var uv2 = current_uv + step_offset;

    var luma_end_1 = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv1, 0.0).rgb) - luma_local_average;
    var luma_end_2 = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv2, 0.0).rgb) - luma_local_average;

    var reached_1 = abs(luma_end_1) >= gradient_scaled;
    var reached_2 = abs(luma_end_2) >= gradient_scaled;
    var reached_both = reached_1 && reached_2;

    if (!reached_1) { uv1 -= step_offset; }
    if (!reached_2) { uv2 += step_offset; }

    if (!reached_both) {
        for (var i = 2; i < FXAA_SEARCH_STEPS; i++) {
            if (!reached_1) {
                luma_end_1 = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv1, 0.0).rgb) - luma_local_average;
                reached_1 = abs(luma_end_1) >= gradient_scaled;
            }
            if (!reached_2) {
                luma_end_2 = rgb_to_luma(textureSampleLevel(color_texture, linear_sampler, uv2, 0.0).rgb) - luma_local_average;
                reached_2 = abs(luma_end_2) >= gradient_scaled;
            }
            if (reached_1 && reached_2) {
                break;
            }
            if (!reached_1) { uv1 -= step_offset; }
            if (!reached_2) { uv2 += step_offset; }
        }
    }

    // 6. Estimate blend factor based on distance to edge endpoints
    let distance_1 = select(abs(uv.y - uv1.y), abs(uv.x - uv1.x), is_horizontal);
    let distance_2 = select(abs(uv2.y - uv.y), abs(uv2.x - uv.x), is_horizontal);

    let is_direction_1 = distance_1 < distance_2;
    let distance_final = min(distance_1, distance_2);
    let edge_thickness = distance_1 + distance_2;
    let pixel_offset = -distance_final / edge_thickness + 0.5;

    // Verify that luma variation at closer endpoint matches center pixel variation
    let is_luma_center_smaller = luma_center < luma_local_average;
    let luma_end_selected = select(luma_end_2, luma_end_1, is_direction_1);
    let correct_variation = (luma_end_selected < 0.0) != is_luma_center_smaller;
    var final_offset = select(0.0, pixel_offset, correct_variation);

    // 7. Sub-pixel aliasing test (for standalone dots/thin lines)
    let luma_average = (1.0 / 12.0) * (2.0 * (luma_down_up + luma_left_right) + luma_nw + luma_ne + luma_sw + luma_se);
    let subpixel_offset_1 = clamp(abs(luma_average - luma_center) / luma_range, 0.0, 1.0);
    let subpixel_offset_2 = (-2.0 * subpixel_offset_1 + 3.0) * subpixel_offset_1 * subpixel_offset_1;
    let subpixel_offset_final = subpixel_offset_2 * subpixel_offset_2 * FXAA_SUBPIXEL_QUALITY;

    final_offset = max(final_offset, subpixel_offset_final);

    // 8. Sample final color with offset along the gradient direction
    var final_uv = uv;
    if (is_horizontal) {
        final_uv.y += step_length * final_offset;
    } else {
        final_uv.x += step_length * final_offset;
    }

    return textureSampleLevel(color_texture, linear_sampler, final_uv, 0.0);
}
    )WGSL";

    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "FXAA pipeline";

    // shader
    wgpu::ShaderModuleDescriptor shader_desc{};
    shader_desc.label = "FXAA shader";
    wgpu::ShaderSourceWGSL shader_src{};
    shader_src.code = SHADER_SRC;
    shader_desc.nextInChain = &shader_src;
    auto shader_module = device_.CreateShaderModule(&shader_desc);

    // vertex
    desc.vertex.bufferCount = 0;
    desc.vertex.buffers = nullptr;
    desc.vertex.module = shader_module;
    desc.vertex.entryPoint = "vs_main";

    // assembly
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
    desc.primitive.frontFace = wgpu::FrontFace::CCW;
    desc.primitive.cullMode = wgpu::CullMode::None;

    // color
    wgpu::ColorTargetState color{};
    color.format = surface_format_;
    color.writeMask = wgpu::ColorWriteMask::All;

    // fragment
    wgpu::FragmentState fragment{};
    fragment.module = shader_module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 1;
    fragment.targets = &color;
    desc.fragment = &fragment;

    // layout
    wgpu::PipelineLayoutDescriptor layout_desc{};
    layout_desc.bindGroupLayoutCount = 1;
    layout_desc.bindGroupLayouts = &fxaa_bind_group_layout_;
    layout_desc.label = "FXAA pipeline layout";
    desc.layout = device_.CreatePipelineLayout(&layout_desc);

    fxaa_pipeline_ = device_.CreateRenderPipeline(&desc);
}

void gfx::RendererWGPU::CreateGuiResources()
{
    // GUI global bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 1> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.visibility = wgpu::ShaderStage::Vertex;
        uniforms_entry.buffer.type = wgpu::BufferBindingType::Uniform;
        uniforms_entry.buffer.minBindingSize = sizeof(GlobalGUIUniformData);

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "GUI global bind group layout";
        gui_global_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }

    // GUI global uniforms buffer
    {
        wgpu::BufferDescriptor desc{};
        desc.size = sizeof(GlobalGUIUniformData);
        desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        desc.label = "GUI global uniforms buffer";
        gui_global_buffer_ = device_.CreateBuffer(&desc);
    }

    // GUI global bind group
    {
        std::array<wgpu::BindGroupEntry, 1> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.buffer = gui_global_buffer_;

        wgpu::BindGroupDescriptor desc{};
        desc.layout = gui_global_bind_group_layout_;
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "GUI global bind group";
        gui_global_bind_group_ = device_.CreateBindGroup(&desc);
    }

    // GUI texture bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 2> entries;

        auto& sampler_entry = entries[0];
        sampler_entry.binding = 0;
        sampler_entry.visibility = wgpu::ShaderStage::Fragment;
        sampler_entry.sampler.type = wgpu::SamplerBindingType::Filtering;

        auto& texture_entry = entries[1];
        texture_entry.binding = 1;
        texture_entry.visibility = wgpu::ShaderStage::Fragment;
        texture_entry.texture.sampleType = wgpu::TextureSampleType::Float;
        texture_entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "GUI texture bind group layout";
        gui_texture_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }
}

void gfx::RendererWGPU::CreateGuiPipeline()
{
    gui_pipeline_ = nullptr;

    constexpr std::string_view SHADER_SRC = R"WGSL(
        struct VertexInput {
	        @location(0) position: vec3f,
	        @location(2) color: vec4f,
	        @location(3) uv: vec2f,
        };        

        struct VertexOutput {
	        @builtin(position) position: vec4f,
	        @location(0) color: vec4f,
	        @location(1) uv: vec2f,
        };

        struct GlobalData {
            matrix: mat3x3f,
        };

        @group(0) @binding(0) var<uniform> uGlobal: GlobalData;
        @group(1) @binding(0) var textureSampler: sampler;
        @group(1) @binding(1) var colorTexture: texture_2d<f32>;

        fn srgbToLinear(c: vec3<f32>) -> vec3<f32> {
            return c * c; // approx
        }

        @vertex
        fn vs_main(in: VertexInput) -> VertexOutput {
            var out: VertexOutput;
            let pos2d = uGlobal.matrix * vec3f(in.position.xy, 1.0);
            out.position = vec4f(pos2d.xy, in.position.z, 1.0);
            out.color = vec4f(srgbToLinear(in.color.rgb), in.color.a);
            out.uv = in.uv;
            return out;
        }

        @fragment
        fn fs_main(in: VertexOutput) -> @location(0) vec4f {
            let textureColor = textureSample(colorTexture, textureSampler, in.uv);
            return textureColor * in.color;
        }
    )WGSL";

    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "GUI pipeline";

    // shader
    wgpu::ShaderModuleDescriptor shader_desc{};
    shader_desc.label = "GUI shader";
    wgpu::ShaderSourceWGSL shader_src{};
    shader_src.code = SHADER_SRC;
    shader_desc.nextInChain = &shader_src;
    auto shader_module = device_.CreateShaderModule(&shader_desc);

    // vertex
    auto vert_layout = GetVertexBufferLayout(MESH_VERTEX_ATTR_POSITION | MESH_VERTEX_ATTR_COLOR | MESH_VERTEX_ATTR_UV0);    
    desc.vertex.bufferCount = 1;
    desc.vertex.buffers = &vert_layout.layout;
    desc.vertex.module = shader_module;
    desc.vertex.entryPoint = "vs_main";

    // assembly
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.frontFace = wgpu::FrontFace::CCW;
    desc.primitive.cullMode = wgpu::CullMode::None;

    // blending
    wgpu::BlendState blend{};
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.color.operation = wgpu::BlendOperation::Add;
    //
    blend.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blend.alpha.dstFactor = wgpu::BlendFactor::One;
    blend.alpha.operation = wgpu::BlendOperation::Add;

    // color
    wgpu::ColorTargetState color{};
    color.format = surface_format_;
    color.blend = &blend;
    color.writeMask = wgpu::ColorWriteMask::All;
        
    // depth
    //wgpu::DepthStencilState depth_state{};
    //depth_state.depthCompare = wgpu::CompareFunction::Always;
    //depth_state.depthWriteEnabled = wgpu::OptionalBool::False;
    //depth_state.format = wgpu::TextureFormat::Depth24Plus;
    //desc.depthStencil = &depth_state;

    // fragment
    wgpu::FragmentState fragment{};
    fragment.module = shader_module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 1;
    fragment.targets = &color;
    desc.fragment = &fragment;

    // multisampling
    desc.multisample.count = msaa_samples_;

    // layout
    wgpu::PipelineLayoutDescriptor layout_desc{};
    std::array<wgpu::BindGroupLayout, 2> bind_groups = {gui_global_bind_group_layout_, gui_texture_bind_group_layout_};
    layout_desc.bindGroupLayoutCount = bind_groups.size();
    layout_desc.bindGroupLayouts = bind_groups.data();
    layout_desc.label = "GUI pipeline layout";
    desc.layout = device_.CreatePipelineLayout(&layout_desc);
    
    gui_pipeline_ = device_.CreateRenderPipeline(&desc);
}

static wgpu::PresentMode GetDesiredPresentMode()
{
    if (r_vsync.Get() == 0)
        return wgpu::PresentMode::Immediate;
    else if (r_vsync.Get() == 1)
        return wgpu::PresentMode::Fifo;
    else
        return wgpu::PresentMode::Mailbox;
}

void gfx::RendererWGPU::ConfigureSurface(const glm::u32vec2& viewport_size)
{
    wgpu::SurfaceConfiguration config{};
    config.presentMode = GetDesiredPresentMode();
    config.width = viewport_size.x;
    config.height = viewport_size.y;
    config.device = device_;
    config.usage = wgpu::TextureUsage::RenderAttachment;    
    config.format = surface_real_format_;

    // view format - srgb
    config.viewFormatCount = 1;
    config.viewFormats = &surface_format_;

    surface_.Configure(&config);
}

void gfx::RendererWGPU::InvalidateSurface()
{
    setup_viewport_size_ = {0, 0};
}

void gfx::RendererWGPU::ProcessViewportSizeChange(const glm::u32vec2& viewport_size)
{
    ConfigureSurface(viewport_size);

    InvalidateLightCullingBindGroup(); // depends on depth texture
    InvalidateAOBindGroup();
    InvalidateFXAABindGroup();

    // create depth
    depth_texture_ = nullptr;
    depth_texture_view_ = nullptr;

    depth_format_ = wgpu::TextureFormat::Depth24Plus;
    wgpu::TextureDescriptor depth_desc{};
    depth_desc.dimension = wgpu::TextureDimension::e2D;
    depth_desc.format = depth_format_;
    depth_desc.mipLevelCount = 1;
    depth_desc.sampleCount = msaa_samples_;
    depth_desc.size = {viewport_size.x, viewport_size.y};
    depth_desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    depth_desc.viewFormatCount = 1;
    depth_desc.viewFormats = &depth_format_;
    depth_desc.label = "Main depth texture";
    depth_texture_ = device_.CreateTexture(&depth_desc);

    wgpu::TextureViewDescriptor depth_view_desc{};
    depth_view_desc.aspect = wgpu::TextureAspect::DepthOnly;
    depth_view_desc.baseArrayLayer = 0;
    depth_view_desc.arrayLayerCount = 1;
    depth_view_desc.baseMipLevel = 0;
    depth_view_desc.mipLevelCount = 1;
    depth_view_desc.dimension = wgpu::TextureViewDimension::e2D;
    depth_view_desc.format = depth_format_;
    depth_view_desc.label = "Main depth texture view";
    depth_texture_view_ = depth_texture_.CreateView(&depth_view_desc);

    // create back color texture if FXAA
    color_texture_ = nullptr;
    color_texture_view_ = nullptr;
    if (use_fxaa_)
    {
        // create multisample color texture
        wgpu::TextureDescriptor color_desc{};
        color_desc.dimension = wgpu::TextureDimension::e2D;
        color_desc.format = surface_format_;
        color_desc.size = {viewport_size.x, viewport_size.y};
        color_desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        color_texture_ = device_.CreateTexture(&color_desc);
        color_texture_view_ = color_texture_.CreateView();
    }

    // create normal texture for AO if enabled
    normal_texture_ = nullptr;
    normal_texture_view_ = nullptr;
    ao_output_texture_ = nullptr;
    ao_output_texture_view_ = nullptr;
    if (surface_shader_cfg_.ao)
    {
        wgpu::TextureDescriptor normal_texture_desc{};
        normal_texture_desc.dimension = wgpu::TextureDimension::e2D;
        normal_texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
        normal_texture_desc.size = {viewport_size.x, viewport_size.y};
        normal_texture_desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        normal_texture_ = device_.CreateTexture(&normal_texture_desc);
        normal_texture_view_ = normal_texture_.CreateView();

        wgpu::TextureDescriptor output_texture_desc{};
        output_texture_desc.dimension = wgpu::TextureDimension::e2D;
        output_texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
        output_texture_desc.size = {viewport_size.x, viewport_size.y};
        output_texture_desc.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding;
        ao_output_texture_ = device_.CreateTexture(&output_texture_desc);
        ao_output_texture_view_ = ao_output_texture_.CreateView();
    }

    // create buffer for visible lights per tile
    light_tiles_ = (viewport_size + (TILE_SIZE - 1)) / TILE_SIZE;
    CreateLightCullingVisibleLightsBuffer();

    setup_viewport_size_ = viewport_size;
}

constexpr std::string_view SHADER_SRC = R"WGSL(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
    var p = vec2f(0.0, 0.0);
    if (in_vertex_index == 0u) {
        p = vec2f(-0.5, -0.5);
    } else if (in_vertex_index == 1u) {
        p = vec2f(0.5, -0.5);
    } else {
        p = vec2f(0.0, 0.5);
    }
    return vec4f(p, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
    return vec4f(0.0, 0.4, 1.0, 1.0);
}
)WGSL";

gfx::SurfaceViewData gfx::RendererWGPU::GetNextSurfaceViewData()
{
    SurfaceViewData data;
    for (size_t i = 0; i < 2; ++i)
    {
        surface_.GetCurrentTexture(&data.surface_texture);

        if (data.surface_texture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal ||
            data.surface_texture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
        {
            break;
        }

        //if (data.surface_texture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
        //{
        //    // try reconfigure
        //    ConfigureSurface();
        //    continue;
        //}

        // failure?
        return data;
    }
    
    wgpu::TextureViewDescriptor view_desc{};
    view_desc.label = "Surface texture view";
    view_desc.format = surface_format_;
    view_desc.dimension = wgpu::TextureViewDimension::e2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = wgpu::TextureAspect::All;
    data.view = data.surface_texture.texture.CreateView(&view_desc);

    return data;
}

bool gfx::RendererWGPU::ReserveBufferCapacity(DynamicBuffer& buffer, size_t capacity)
{
    if (buffer.capacity >= capacity)
        return false; // already enough

    buffer.buffer = nullptr;

    wgpu::BufferDescriptor desc{};
    desc.size = capacity;
    desc.usage = buffer.usage;
    desc.label = "A dynamic buffer";
    buffer.buffer = device_.CreateBuffer(&desc);

    buffer.capacity = capacity;

    return true;
}

void gfx::RendererWGPU::SetBufferData(DynamicBuffer& buffer, std::span<const uint8_t> data)
{
    if (data.empty())
        return;

    ReserveBufferCapacity(buffer, data.size_bytes());
    queue_.WriteBuffer(buffer.buffer, 0, data.data(), data.size_bytes());
}

gfx::TextureID gfx::RendererWGPU::GetWhiteTexture()
{
    if (!white_tex_)
    {
        white_tex_ = assets::AssetManager::GetInstance().Get<Texture>("white");
    }

    return white_tex_->GetID();
}

gfx::MaterialID gfx::RendererWGPU::GetDummyMaterial()
{
    if (!dummy_material_)
    {
        GetWhiteTexture();

        MaterialInfo info{};
        info.texture = white_tex_;

        dummy_material_ = std::make_shared<Material>(info);
    }

    return dummy_material_->GetID();
}

wgpu::Sampler gfx::RendererWGPU::GetSampler(bool linear, bool mipmaps)
{
    uint8_t hash = 0;
    if (linear)
        hash |= 1;
    if (mipmaps)
        hash |= 2;

    auto it = samplers_.find(hash);
    if (it != samplers_.end())
    {
        return it->second;
    }

    wgpu::SamplerDescriptor desc{};
    desc.addressModeU = wgpu::AddressMode::Repeat;
    desc.addressModeV = wgpu::AddressMode::Repeat;
    desc.addressModeW = wgpu::AddressMode::Repeat;
    desc.magFilter = linear ? wgpu::FilterMode::Linear : wgpu::FilterMode::Nearest;
    desc.minFilter = mipmaps ? wgpu::FilterMode::Linear : wgpu::FilterMode::Nearest;
    desc.lodMinClamp = 0.0f;
    desc.lodMaxClamp = 32.0f;
    desc.label = "A cached sampler";
    desc.mipmapFilter = mipmaps ? wgpu::MipmapFilterMode::Linear : wgpu::MipmapFilterMode::Undefined;

    auto sampler = device_.CreateSampler(&desc);

    samplers_[hash] = sampler;
    
    return sampler;
}

wgpu::Sampler gfx::RendererWGPU::GetSamplerForTexture(const TextureWGPU& texture)
{
    return GetSampler(texture.desc.filter == TEXTURE_FILTER_LINEAR, texture.mip_levels > 1);
}

void gfx::RendererWGPU::CreateTextureGuiBindGroup(TextureWGPU& texture)
{
    std::array<wgpu::BindGroupEntry, 2> entries;

    auto sampler = GetSamplerForTexture(texture);

    auto& sampler_entry = entries[0];
    sampler_entry.binding = 0;
    sampler_entry.sampler = sampler;

    auto& color_texture_entry = entries[1];
    color_texture_entry.binding = 1;
    color_texture_entry.textureView = texture.view;

    wgpu::BindGroupDescriptor group_desc{};
    group_desc.layout = gui_texture_bind_group_layout_;
    group_desc.entryCount = entries.size();
    group_desc.entries = entries.data();
    group_desc.label = "A texture GUI bind group";
    texture.gui_bind_group = device_.CreateBindGroup(&group_desc);
}

gfx::VertexBufferLayout gfx::RendererWGPU::GetVertexBufferLayout(MeshVertexAttributeFlags attrs)
{
    VertexBufferLayout l{};
    auto o = GetVertexPackOffsets(attrs);

    if (attrs & MESH_VERTEX_ATTR_POSITION)
    {
        auto& attr = l.attrs.emplace_back();
        attr.shaderLocation = 0;
        attr.format = wgpu::VertexFormat::Float32x3;
        attr.offset = o.offset_pos;
    }

    if (attrs & MESH_VERTEX_ATTR_NORMAL)
    {
        auto& attr = l.attrs.emplace_back();
        attr.shaderLocation = 1;
        attr.format = wgpu::VertexFormat::Float32x3;
        attr.offset = o.offset_normal;
    }

    if (attrs & MESH_VERTEX_ATTR_COLOR)
    {
        auto& attr = l.attrs.emplace_back();
        attr.shaderLocation = 2;
        attr.format = wgpu::VertexFormat::Unorm8x4;
        attr.offset = o.offset_color;
    }

    if (attrs & MESH_VERTEX_ATTR_UV0)
    {
        auto& attr = l.attrs.emplace_back();
        attr.shaderLocation = 3;
        attr.format = wgpu::VertexFormat::Float32x2;
        attr.offset = o.offset_uv0;
    }

    if (attrs & MESH_VERTEX_ATTR_UV1)
    {
        auto& attr = l.attrs.emplace_back();
        attr.shaderLocation = 4;
        attr.format = wgpu::VertexFormat::Float32x2;
        attr.offset = o.offset_uv1;
    }

    if (attrs & MESH_VERTEX_ATTR_BONE_DATA)
    {
        // indices
        {
            auto& attr = l.attrs.emplace_back();
            attr.shaderLocation = 5;
            attr.format = wgpu::VertexFormat::Uint8x4;
            attr.offset = o.offset_bone;
        }

        // weights
        {
            auto& attr = l.attrs.emplace_back();
            attr.shaderLocation = 6;
            attr.format = wgpu::VertexFormat::Float32x4;
            attr.offset = o.offset_bone + 4;
        }
    }

    l.layout.arrayStride = o.stride;
    l.layout.attributeCount = l.attrs.size();
    l.layout.attributes = l.attrs.data();
    l.layout.stepMode = wgpu::VertexStepMode::Vertex;

    return l;
}

const wgpu::RenderPipeline& gfx::RendererWGPU::GetSurfacePipeline(SurfacePipelineFlags flags)
{
    // check if cached
    auto& pipeline = surface_pipelines_[flags];
    if (pipeline)
        return pipeline;

    // check if shader cached
    auto& shader_module = surface_shaders_[flags];
    if (!shader_module)
    {
        shader_module = CreateSurfaceShaderWGPU(device_, flags, surface_shader_cfg_);
    }

    // PIPELINE
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "A surface pipeline";

    // vertex
    MeshVertexAttributeFlags attrs = MESH_VERTEX_ATTR_POSITION | MESH_VERTEX_ATTR_NORMAL | MESH_VERTEX_ATTR_UV0;
    if (flags & SPF_SKELETAL)
    {
        attrs |= MESH_VERTEX_ATTR_BONE_DATA;
    }

    auto vert_layout = GetVertexBufferLayout(attrs);
    desc.vertex.bufferCount = 1;
    desc.vertex.buffers = &vert_layout.layout;
    desc.vertex.module = shader_module;
    desc.vertex.entryPoint = "vs_main";

    // assembly
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.frontFace = wgpu::FrontFace::CCW;
    //desc.primitive.cullMode = (flags & SPF_2SIDED)       ? wgpu::CullMode::None
    //                          : (flags & SPF_SHADOW_MAP) ? wgpu::CullMode::Front
    //                                                     : wgpu::CullMode::Back;

    desc.primitive.cullMode = (flags & SPF_2SIDED) ? wgpu::CullMode::None : wgpu::CullMode::Back;

    // depth
    wgpu::DepthStencilState depth_state{};
    depth_state.format =
        (flags & SPF_SHADOW_MAP) ? wgpu::TextureFormat::Depth32Float : wgpu::TextureFormat::Depth24Plus;

    if (flags & SPF_DEPTH_ONLY)
    {
        depth_state.depthCompare = wgpu::CompareFunction::Less;
        depth_state.depthWriteEnabled = true;
    }
    else
    {
        depth_state.depthCompare = (flags & SPF_BLEND) ? wgpu::CompareFunction::Less : wgpu::CompareFunction::Equal;
        depth_state.depthWriteEnabled = false;
    }

    if (flags & SPF_SHADOW_MAP)
    {
        depth_state.depthBias = 8;
        depth_state.depthBiasSlopeScale = 1.5f;
        depth_state.depthBiasClamp = 0.05f;
    }

    desc.depthStencil = &depth_state;

    // fragment
    wgpu::FragmentState fragment{};
    fragment.module = shader_module;
    fragment.entryPoint = "fs_main";

    // color target
    wgpu::ColorTargetState color{};
    wgpu::BlendState blend{};
    if ((flags & SPF_DEPTH_ONLY) == 0)
    {
        color.format = surface_format_;
        color.writeMask = wgpu::ColorWriteMask::All;

        // blending
        if (flags & SPF_BLEND)
        {
            if (flags & SPF_BLEND_ADDITIVE)
            {
                blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
                blend.color.dstFactor = wgpu::BlendFactor::One;
            }
            else
            {
                blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
                blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
            }

            blend.color.operation = wgpu::BlendOperation::Add;
            //
            blend.alpha.srcFactor = wgpu::BlendFactor::Zero;
            blend.alpha.dstFactor = wgpu::BlendFactor::One;
            blend.alpha.operation = wgpu::BlendOperation::Add;

            color.blend = &blend;
        }

        fragment.targetCount = 1;
        fragment.targets = &color;
    }
    else if ((flags & SPF_DEPTH_ONLY) > 0 && (flags & SPF_SHADOW_MAP) == 0 &&
             surface_shader_cfg_.ao) // depth prepass, write normal for AO
    {
        color.format = normal_texture_.GetFormat();
        color.writeMask = wgpu::ColorWriteMask::All;

        fragment.targetCount = 1;
        fragment.targets = &color;
    }

    // only use fragment shader if color attachment or alpha culling in depth pass
    if (fragment.targets || flags & SPF_CULL_ALPHA)
    {
        desc.fragment = &fragment;
    }

    // multisampling
    desc.multisample.count = (flags & SPF_SHADOW_MAP) ? 1 : msaa_samples_;

    // layout
    wgpu::PipelineLayoutDescriptor layout_desc{};
    layout_desc.label = "A surface pipeline layout";

    size_t num_bind_groups = 3;
    std::array<wgpu::BindGroupLayout, 4> bind_groups = {global_bind_group_layout_, material_bind_group_layout_, instance_bind_group_layout_, nullptr};
    if (flags & SPF_DEPTH_ONLY)
    {
        bind_groups[0] = global_depth_bind_group_layout_;
    }

    if (flags & SPF_SKELETAL)
    {
        bind_groups[3] = skeletal_bind_group_layout_;
        num_bind_groups = 4;
    }
    else if (flags & SPF_DEFORM)
    {
        bind_groups[3] = deform_bind_group_layout_;
        num_bind_groups = 4;
    }
    
    layout_desc.bindGroupLayoutCount = num_bind_groups;
    layout_desc.bindGroupLayouts = bind_groups.data();
    desc.layout = device_.CreatePipelineLayout(&layout_desc);

    pipeline = device_.CreateRenderPipeline(&desc);
    return pipeline;
}

void gfx::RendererWGPU::InvalidateSurfacePipelines()
{
    surface_pipelines_.clear();
    surface_shaders_.clear();
}

void gfx::RendererWGPU::CreateInstanceBufferBindGroup()
{
    std::array<wgpu::BindGroupEntry, 1> entries;

    auto& uniforms_entry = entries[0];
    uniforms_entry.binding = 0;
    uniforms_entry.buffer = instance_buffer_.buffer;

    wgpu::BindGroupDescriptor desc{};
    desc.layout = instance_bind_group_layout_;
    desc.entryCount = entries.size();
    desc.entries = entries.data();
    desc.label = "Instance bind group";
    instance_bind_group_ = device_.CreateBindGroup(&desc);
}

void gfx::RendererWGPU::UpdateSettings()
{
    //if (r_msaa.IsModified())
    //{
    //    msaa_samples_ = GetMultisampleCount();
    //    CreateGuiPipeline();
    //    InvalidateSurfacePipelines();
    //    InvalidateSurface();
    //    r_msaa.ClearModified();
    //}

    if (r_csm_resolution.IsModified() || r_shadow_resolution.IsModified() || r_csm_cascades.IsModified() ||
        r_shadow_count.IsModified() || r_shadow_quality.IsModified())
    {
        InvalidateShadowResources();

        r_csm_resolution.ClearModified();
        r_shadow_resolution.ClearModified();
        r_csm_cascades.ClearModified();
        r_shadow_count.ClearModified();
        r_shadow_quality.ClearModified();
    }

    if (r_vsync.IsModified())
    {
        InvalidateSurface();
        r_vsync.ClearModified();
    }

    if (r_tile_debug.IsModified())
    {
        surface_shader_cfg_.debug_tiles = r_tile_debug.Get() > 0;
        InvalidateSurfacePipelines();
        r_tile_debug.ClearModified();
    }

    if (r_ao.IsModified())
    {
        surface_shader_cfg_.ao = r_ao.Get() > 0;
        InvalidateSurface();
        InvalidateSurfacePipelines();
        InvalidateSurfaceGlobalBindGroup();
        InvalidateAOPipeline();
        r_ao.ClearModified();
    }

    if (r_fxaa.IsModified())
    {
        use_fxaa_ = r_fxaa.Get() > 0;
        InvalidateSurface();
        r_fxaa.ClearModified();
    }
}

void gfx::RendererWGPU::Render(Scene& scene, const CameraParams& camera)
{
    ++frame_index_;

    if (!shadow_resources_setup_)
    {
        CreateShadowResources();
    }

    auto viewport_size = GetViewportSize();
    if (setup_viewport_size_ != viewport_size)
    {
        ProcessViewportSizeChange(viewport_size);
    }

    auto encoder = device_.CreateCommandEncoder();

    // compute matrices
    float aspect = static_cast<float>(viewport_size.x) / static_cast<float>(viewport_size.y);

    float min_distance = 0.0f;
    float max_distance = static_cast<float>(r_distance.Get());
    const float farplane = max_distance * 2.0f + 500.0f;

    auto proj = glm::perspectiveRH_ZO(glm::radians(camera.fov * 0.5f), aspect, 0.1f, farplane);
    auto view = glm::lookAt(camera.eye, camera.eye + camera.dir, glm::vec3(0.0f, 0.0f, 1.0f));


    // main capture
    main_dlist_.Clear();
    DrawContext main_ctx{main_dlist_, DRAW_PASS_MAIN, camera.eye, view, proj, min_distance, max_distance, viewport_size};
    scene.Draw(main_ctx);
    auto env = scene.GetSceneEnvironment();

    // prepare prepass & main cmds
    PrepareSurfaceCmds(main_dlist_.surfaces, pcmds_prepass_, SPF_DEPTH_ONLY);
    PrepareSurfaceCmds(main_dlist_.surfaces, pcmds_main_, 0);

    size_t total_instances = pcmds_prepass_.size() + pcmds_main_.size();

    // prepare CSM cmds
    ComputeCSMMatrices(main_ctx.view, camera.fov, aspect, env.sun_direction);

    for (uint32_t i = 0; i < csm_num_cascades_; ++i)
    {
        auto& cascade = csm_cascades_[i];
        cascade.dlist.Clear();
        
        DrawContext cascade_ctx(cascade.dlist, DRAW_PASS_SHADOW_MAP, camera.eye, cascade.view, cascade.proj,
                                0.0f, 150.0f, glm::u32vec2(csm_resolution_));

        scene.Draw(cascade_ctx);

        PrepareSurfaceCmds(cascade.dlist.surfaces, cascade.pcmds, SPF_DEPTH_ONLY | SPF_SHADOW_MAP);
        total_instances += cascade.pcmds.size();
    }

    // prepare spotlight shadow cmds
    PrepareLights(main_dlist_.lights, main_ctx);

    for (uint32_t i = 0; i < spotlight_shadow_current_count_; ++i)
    {
        auto& shadowmap = spotlight_shadows_[i];
        shadowmap.dlist.Clear();

        DrawContext shadowmap_ctx(shadowmap.dlist, DRAW_PASS_SHADOW_MAP, camera.eye, shadowmap.view, shadowmap.proj,
                                  0.0f, 150.0f, glm::u32vec2(spotlight_shadow_resolution_));

        scene.Draw(shadowmap_ctx);

        PrepareSurfaceCmds(shadowmap.dlist.surfaces, shadowmap.pcmds, SPF_DEPTH_ONLY | SPF_SHADOW_MAP);
        total_instances += shadowmap.pcmds.size();
    }

    // setup instance buffer
    instances_.clear();
    instances_.reserve(total_instances);
    if (ReserveBufferCapacity(instance_buffer_, total_instances * sizeof(instances_[0])))
    {
        // need to recreate bind group if reallocated
        CreateInstanceBufferBindGroup();
    }

    uint32_t pass_index = 0;

    // render CSM cascades
    for (uint32_t i = 0; i < csm_num_cascades_; ++i)
    {
        auto& cascade = csm_cascades_[i];

        GlobalUniformData globals{};
        globals.view = cascade.view;
        globals.proj = cascade.proj;
        globals.view_proj = cascade.view_proj;

        // setup cascade depth attachment
        wgpu::RenderPassDepthStencilAttachment depth_attachment{};
        depth_attachment.view = cascade.texture_view;
        depth_attachment.depthLoadOp = wgpu::LoadOp::Clear;
        depth_attachment.depthStoreOp = wgpu::StoreOp::Store;
        depth_attachment.depthClearValue = 1.0f;

        wgpu::RenderPassDescriptor pass_desc{};
        pass_desc.depthStencilAttachment = &depth_attachment;
        auto pass = encoder.BeginRenderPass(&pass_desc);
        EncodePreparedCmds(pass, cascade.pcmds, globals, pass_index++, SPF_DEPTH_ONLY | SPF_SHADOW_MAP);
        pass.End();
    }

    // render spotlight shadowmaps
    for (uint32_t i = 0; i < spotlight_shadow_current_count_; ++i)
    {
        auto& shadowmap = spotlight_shadows_[i];

        GlobalUniformData globals{};
        globals.view = shadowmap.view;
        globals.proj = shadowmap.proj;
        globals.view_proj = shadowmap.view_proj;

        // setup cascade depth attachment
        wgpu::RenderPassDepthStencilAttachment depth_attachment{};
        depth_attachment.view = shadowmap.texture_view;
        depth_attachment.depthLoadOp = wgpu::LoadOp::Clear;
        depth_attachment.depthStoreOp = wgpu::StoreOp::Store;
        depth_attachment.depthClearValue = 1.0f;

        wgpu::RenderPassDescriptor pass_desc{};
        pass_desc.depthStencilAttachment = &depth_attachment;
        auto pass = encoder.BeginRenderPass(&pass_desc);
        EncodePreparedCmds(pass, shadowmap.pcmds, globals, pass_index++, SPF_DEPTH_ONLY | SPF_SHADOW_MAP);
        pass.End();
    }

    // setup main globals
    GlobalUniformData globals{};
    globals.view = main_ctx.view;
    globals.proj = main_ctx.proj;
    globals.view_proj = main_ctx.view_proj;
    globals.ambient_color = LinearizeColor(env.ambient_light);
    globals.sun_color = LinearizeColor(env.sun_color) * 1.5f;
    globals.sun_direction = glm::mat3(main_ctx.view) * env.sun_direction;
    globals.camera_pos = main_ctx.eye;
    globals.fog = glm::vec4(LinearizeColor(env.fog), env.fog.a);
    globals.tile_count_x = light_tiles_.x;

    globals.csm_texel_size = 1.0f / static_cast<float>(csm_resolution_);
    globals.csm_cascade_count = csm_num_cascades_;

    auto inv_view = glm::inverse(main_ctx.view);
    for (uint32_t i = 0; i < csm_num_cascades_; ++i)
    {
        globals.csm_splits[i] = csm_splits_[i];
        globals.csm_matrices[i] = csm_cascades_[i].view_proj * inv_view;
    }

    globals.spotlight_texel_size = 1.0f / static_cast<float>(spotlight_shadow_resolution_);

    for (uint32_t i = 0; i < spotlight_shadow_current_count_; ++i)
    {
        globals.spotlight_matrices[i] = spotlight_shadows_[i].view_proj * inv_view;
    }

    // setup depth attachment
    wgpu::RenderPassDepthStencilAttachment depth_attachment{};
    depth_attachment.view = depth_texture_view_;
    depth_attachment.depthLoadOp = wgpu::LoadOp::Clear;
    depth_attachment.depthStoreOp = wgpu::StoreOp::Store;
    depth_attachment.depthClearValue = 1.0f;

    // encode depth prepass
    {        
        wgpu::RenderPassDescriptor pass_desc{};
        pass_desc.depthStencilAttachment = &depth_attachment;
        
        // normal texture for AO
        wgpu::RenderPassColorAttachment color_attachment{};
        if (surface_shader_cfg_.ao)
        {
            color_attachment.view = normal_texture_view_;
            color_attachment.loadOp = wgpu::LoadOp::Clear;
            color_attachment.storeOp = wgpu::StoreOp::Store;
            color_attachment.clearValue = {0.0, 0.0, 0.0, 1.0};
            pass_desc.colorAttachmentCount = 1;
            pass_desc.colorAttachments = &color_attachment;
        }

        auto pass = encoder.BeginRenderPass(&pass_desc);
        EncodePreparedCmds(pass, pcmds_prepass_, globals, pass_index++, SPF_DEPTH_ONLY);
        pass.End();
    }

    // cull lights using depth buffer
    EncodeLightCullingPass(encoder, main_ctx);

    // compute AO
    if (surface_shader_cfg_.ao)
    {
        EncodeAO(encoder, main_ctx);
    }

    // setup global bind group
    if (!global_bind_group_)
    {
        CreateSurfaceGlobalBindGroup();
    }

    // setup surface texture
    auto surface_view = GetNextSurfaceViewData();

    // setup color attachment
    wgpu::RenderPassColorAttachment color_attachment{};
    color_attachment.view = use_fxaa_ ? color_texture_view_ : surface_view.view;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    color_attachment.loadOp = wgpu::LoadOp::Clear;
    //auto clear_color = env.clear_color;
    auto clear_color = LinearizeColor(env.clear_color);
    color_attachment.clearValue = {clear_color.r, clear_color.g, clear_color.b, 1.0};

    // setup depth for main pass
    depth_attachment.depthLoadOp = wgpu::LoadOp::Load;
    depth_attachment.depthStoreOp = wgpu::StoreOp::Discard;

    // encode main pass
    {
        wgpu::RenderPassDescriptor pass_desc{};
        pass_desc.colorAttachmentCount = 1;
        pass_desc.colorAttachments = &color_attachment;
        pass_desc.depthStencilAttachment = &depth_attachment;
        auto pass = encoder.BeginRenderPass(&pass_desc);

        // draw surfaces
        EncodePreparedCmds(pass, pcmds_main_, globals, pass_index++, 0);

        // also draw coronas in this pass
        EncodeCoronaCmds(pass, main_dlist_.coronas, main_ctx);

        pass.End();
    }

    // setup FXAA/HUD pass
    color_attachment.view = surface_view.view;
    color_attachment.loadOp = use_fxaa_ ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load; // load directly if FXAA disabled
    color_attachment.clearValue = {0.0, 0.0, 0.0, 1.0};

    // encode FXAA/HUD pass
    {
        wgpu::RenderPassDescriptor pass_desc{};
        pass_desc.colorAttachmentCount = 1;
        pass_desc.colorAttachments = &color_attachment;
        auto pass = encoder.BeginRenderPass(&pass_desc);

        if (use_fxaa_)
        {
            EncodeFXAA(pass, main_ctx);
        }

        // hud
        EncodeHudCmds(pass, main_dlist_.huds, main_ctx);

        pass.End();

    }


    // upload instance data from all passes
    std::span<const uint8_t> instance_data_view = {reinterpret_cast<const uint8_t*>(instances_.data()),
                                                   instances_.size() * sizeof(instances_[0])};
    assert(instance_data_view.size_bytes() <= instance_buffer_.capacity);
    SetBufferData(instance_buffer_, instance_data_view); 

    // submit
    wgpu::CommandBufferDescriptor cmd_desc{};
    cmd_desc.label = "Command buffer";
    auto command = encoder.Finish(&cmd_desc);
    queue_.Submit(1, &command);

#if !defined(__EMSCRIPTEN__)
    surface_.Present();
#endif
}

void gfx::RendererWGPU::ComputeCSMMatrices(const glm::mat4& view, float fov, float aspect, const glm::vec3& sun_dir)
{
    //auto inv_view = glm::inverse(view);

    for (uint32_t i = 0; i < csm_num_cascades_; ++i)
    {
        float near_plane = i == 0 ? 0.01f : csm_splits_[i - 1];
        float far_plane = csm_splits_[i];

        auto clipped_proj = glm::perspectiveRH_ZO(glm::radians(fov * 0.5f), aspect, near_plane, far_plane);
        auto clipped_vp = clipped_proj * view;

        auto inv = glm::inverse(clipped_vp);

        glm::vec4 frustum_corners_ws[8];
        int j = 0;
        for (int x = 0; x < 2; ++x)
        {
            for (int y = 0; y < 2; ++y)
            {
                for (int z = 0; z < 2; ++z)
                {
                    glm::vec4 clip(x ? 1.0f : -1.0f, y ? 1.0f : -1.0f, z ? 1.0f : 0.0f, 1.0f);

                    auto pt = inv * clip;
                    frustum_corners_ws[j++] = pt / pt.w;
                }
            }
        }

        glm::vec3 center = glm::vec3(0, 0, 0);
        for (int j = 0; j < 8; j++)
            center += glm::vec3(frustum_corners_ws[j]);
        center /= 8.0f;

        auto light_view = glm::lookAt(center, center + sun_dir, glm::vec3(0.0f, 0.0f, 1.0f));

        glm::vec3 min_extents(FLT_MAX);
        glm::vec3 max_extents(-FLT_MAX);

        for (int j = 0; j < 8; j++)
        {
            glm::vec3 corner = glm::vec3(light_view * frustum_corners_ws[j]);
            min_extents = glm::min(min_extents, corner);
            max_extents = glm::max(max_extents, corner);
        }

        min_extents.z -= 100.0f;
        max_extents.z += 10.0f;

        glm::mat4 light_proj =
            glm::orthoRH_ZO(min_extents.x, max_extents.x, min_extents.y, max_extents.y, min_extents.z, max_extents.z);

        auto light_vp = light_proj * light_view;

        auto& cascade = csm_cascades_[i];
        cascade.proj = light_proj;
        cascade.view = light_view;
        cascade.view_proj = light_vp;
    }
}

void gfx::RendererWGPU::PrepareSurfaceCmds(std::span<DrawSurfaceCmd> cmds, std::vector<PreparedCmd>& pcmds,
                                           SurfacePipelineFlags pflags)
{
    pcmds.clear();

    bool depth_only = pflags & SPF_DEPTH_ONLY;
    auto dummy_material = GetDummyMaterial();

    for (const auto& cmd : cmds)
    {
        assert(cmd.mesh > 0);
        assert(cmd.tri_count > 0);
        assert(cmd.material > 0);

        auto& material = materials_.Get(cmd.material);
        auto& mat_vals = material.desc.properties;

        if (depth_only && mat_vals.blend != MATERIAL_BLEND_TYPE_NONE)
            continue; // ignore blended in depth only pass

        auto& mesh = meshes_.Get(cmd.mesh);

        auto& pcmd = pcmds.emplace_back();
        pcmd.cmd = &cmd;
        pcmd.mesh_id = cmd.mesh;
        pcmd.tri_offset = cmd.tri_offset;
        pcmd.tri_count = cmd.tri_count;
        pcmd.material_id = cmd.material;
        pcmd.dist = cmd.dist;
        pcmd.pflags = pflags;

        // MESH
        if (cmd.pose > 0)
        {
            // skeletal
            // require bone vert attrs for this
            if ((mesh.desc.attributes & MESH_VERTEX_ATTR_BONE_DATA) > 0)
            {
                pcmd.pflags |= SPF_SKELETAL;
            }
        }
        else if (cmd.deform_tex > 0)
        {
            // deform grid
            pcmd.pflags |= SPF_DEFORM;
        }

        // MATERIAL
        // 2sided
        if (mat_vals.twosided)
        {
            pcmd.pflags |= SPF_2SIDED;
        }

        // alpha culling
        if (material.alpha_culling)
        {
            pcmd.pflags |= SPF_CULL_ALPHA;
        }

        if (depth_only)
        {
            // texture only required if alpha cull

            if (material.desc.texture > 0 && (pcmd.pflags & SPF_CULL_ALPHA))
            {
                pcmd.pflags |= SPF_TEXTURE;
            }
            else
            {
                // no need to bind the specific material, just render depth
                pcmd.material_id = dummy_material;
            }

            continue; // other stuff not required in depth only passes
        }

        // texture
        if (material.desc.texture > 0)
        {
            pcmd.pflags |= SPF_TEXTURE;
        }

        // blending type
        if (mat_vals.blend != MATERIAL_BLEND_TYPE_NONE)
        {
            pcmd.pflags |= SPF_BLEND;
            if (mat_vals.blend == MATERIAL_BLEND_TYPE_ADDITIVE)
                pcmd.pflags |= SPF_BLEND_ADDITIVE;
        }
        else
        {
            // fog only if not blended
            pcmd.pflags |= SPF_FOG;
        }

        // color
        if (!cmd.colors.empty())
        {
            if (mat_vals.color == MATERIAL_OBJECT_COLOR_TYPE_MULTIPLY)
                pcmd.pflags |= SPF_OBJECT_COLOR;
            else if (mat_vals.color == MATERIAL_OBJECT_COLOR_TYPE_BACKGROUND)
                pcmd.pflags |= SPF_OBJECT_COLOR | SPF_OBJECT_COLOR_BACKGROUND;
            else if (mat_vals.color == MATERIAL_OBJECT_COLOR_TYPE_MULTICOLOR)
                pcmd.pflags |= SPF_MULTICOLOR;
        }

        if (mat_vals.lighting != MATERIAL_LIGHTING_TYPE_UNLIT)
        {
            pcmd.pflags |= SPF_LIT;

            // translucent
            if (mat_vals.translucent)
            {
                pcmd.pflags |= SPF_TRANSLUCENT;
            }
        }
    }

    if (depth_only)
    {
        // no need to care about blended surfaces in depth only pass
        std::ranges::sort(pcmds, [](const PreparedCmd& a, const PreparedCmd& b) {
            return std::tie(a.pflags, a.material_id, a.mesh_id, a.tri_offset, a.tri_count) <
                   std::tie(b.pflags, b.material_id, b.mesh_id, b.tri_offset, b.tri_count);
        });
    }
    else
    {
        std::ranges::sort(pcmds, [](const PreparedCmd& a, const PreparedCmd& b) {
            const auto blend_a = a.pflags & SPF_BLEND;
            const auto blend_b = b.pflags & SPF_BLEND;

            if (blend_a != blend_b)
                return blend_b > 0; // opaque first

            if (blend_a) // both blended
            {
                return a.dist > b.dist; // do not optimize blended, sort by distance instead
            }

            return std::tie(a.pflags, a.material_id, a.mesh_id, a.tri_offset, a.tri_count) <
                   std::tie(b.pflags, b.material_id, b.mesh_id, b.tri_offset, b.tri_count);
        });
    }
}

void gfx::RendererWGPU::EncodePreparedCmds(wgpu::RenderPassEncoder& pass, std::span<PreparedCmd> pcmds,
                                           const GlobalUniformData& globals, uint32_t globals_index,
                                           SurfacePipelineFlags pflags)
{
    if (pcmds.empty())
        return;

    queue_.WriteBuffer(global_buffer_, globals_index * GLOBAL_BUFFER_STRIDE, &globals, sizeof(globals));

    SurfacePipelineFlags batch_pflags = -1;
    MeshID batch_mesh_id = 0;
    uint32_t batch_tri_offset = 0;
    uint32_t batch_tri_count = 0;
    MaterialID batch_material_id = 0;
    SkeletonPoseID batch_pose_id = 0;
    DeformTextureID batch_deform_id = 0;

    SurfacePipelineFlags current_pflags = -1;
    MeshID current_mesh_id = 0;
    MaterialID current_material_id = 0;
    SkeletonPoseID current_pose_id = 0;
    DeformTextureID current_deform_id = 0;

    uint32_t first_instance = static_cast<uint32_t>(instances_.size());
    uint32_t num_instances = 0;

    auto flush_batch = [&]() {
        if (num_instances == 0)
            return;

        // bind pipeline
        if (batch_pflags != current_pflags)
        {
            auto& pipeline = GetSurfacePipeline(batch_pflags);
            pass.SetPipeline(pipeline);
            current_pflags = batch_pflags;
        }

        // bind vertex/index buffer
        if (batch_mesh_id != current_mesh_id)
        {
            auto& mesh = meshes_.Get(batch_mesh_id);
            pass.SetVertexBuffer(0, mesh.vertex_buffer.buffer);
            pass.SetIndexBuffer(mesh.index_buffer.buffer, wgpu::IndexFormat::Uint32);
            current_mesh_id = batch_mesh_id;
        }

        // bind material group
        if (batch_material_id != current_material_id)
        {
            auto& material = materials_.Get(batch_material_id);
            pass.SetBindGroup(1, material.bind_group);
            current_material_id = batch_material_id;
        }

        // bind special group
        if (batch_pflags & SPF_SKELETAL)
        {
            if (batch_pose_id != current_pose_id)
            {
                auto& pose = poses_.Get(batch_pose_id);
                pass.SetBindGroup(3, pose.bind_group);
                current_pose_id = batch_pose_id;
                current_deform_id = 0;
            }
        }
        else if (batch_pflags & SPF_DEFORM)
        {
            if (batch_deform_id != current_deform_id)
            {
                auto& deform = deforms_.Get(batch_deform_id);
                pass.SetBindGroup(3, deform.bind_group);
                current_deform_id = batch_deform_id;
                current_pose_id = 0;
            }
        }

        pass.DrawIndexed(batch_tri_count * 3, num_instances, batch_tri_offset * 3, 0, first_instance);
        first_instance += num_instances;
        num_instances = 0;
    
    };

    bool depth_only = (pflags & SPF_DEPTH_ONLY);

    uint32_t global_offset = GLOBAL_BUFFER_STRIDE * globals_index;
    pass.SetBindGroup(0, depth_only ? global_depth_bind_group_ : global_bind_group_, 1, &global_offset);
    pass.SetBindGroup(2, instance_bind_group_);

    for (const auto& pcmd : pcmds)
    {
        auto& cmd = *pcmd.cmd;

        if (pcmd.pflags != batch_pflags || pcmd.material_id != batch_material_id || pcmd.mesh_id != batch_mesh_id ||
            pcmd.tri_offset != batch_tri_offset || pcmd.tri_count != batch_tri_count ||
            (pcmd.pflags & (SPF_SKELETAL | SPF_DEFORM)) > 0)
        {
            flush_batch();

            batch_pflags = pcmd.pflags;
            batch_material_id = pcmd.material_id;
            batch_mesh_id = pcmd.mesh_id;
            batch_tri_offset = pcmd.tri_offset;
            batch_tri_count = pcmd.tri_count;

            if (pcmd.pflags & SPF_SKELETAL)
            {
                batch_pose_id = cmd.pose;
            }
            else if (pcmd.pflags & SPF_DEFORM)
            {
                batch_deform_id = cmd.deform_tex;
            }
        }

        auto& instance_data = instances_.emplace_back();
        instance_data.matrix = cmd.matrix ? *cmd.matrix : glm::mat4(1.0f);

        if (!depth_only && (pcmd.pflags & (SPF_OBJECT_COLOR | SPF_MULTICOLOR)))
        {
            for (size_t i = 0; i < cmd.colors.size(); ++i)
            {
                auto color = cmd.colors[i];
                // linearize
                color = glm::vec4(LinearizeColor(color), color.a);
                instance_data.colors[i] = glm::packUnorm4x8(color);
            }
        }

        ++num_instances;
    }

    flush_batch();
}

void gfx::RendererWGPU::PrepareLights(std::span<DrawLightCmd> cmds, const DrawContext& ctx)
{
    lights_.clear();
    spotlight_shadow_current_count_ = 0;

    // calc distance
    for (auto& cmd : cmds)
    {
        auto d = cmd.light.position - ctx.eye;
        cmd.dist2 = glm::dot(d, d);
    }

    std::ranges::sort(cmds, [](const DrawLightCmd& a, const DrawLightCmd& b) { return a.dist2 < b.dist2; });

    size_t num_lights = std::min(static_cast<size_t>(MAX_LIGHTS), cmds.size());

    for (size_t i = 0; i < num_lights; ++i)
    {
        auto& cmd = cmds[i];
        auto& light = cmd.light;

        auto& entry = lights_.emplace_back();
        entry.view_pos = ctx.view * glm::vec4(light.position, 1.0f);
        entry.radius = light.radius;
        entry.color = light.color;
        entry.cos_inner = light.cos_inner;
        entry.cos_outer = light.cos_outer;
        entry.shadow_idx = 0xFFFFFFFF;

        if (light.cos_inner > light.cos_outer)
        {
            // spotlight
            entry.view_dir = glm::mat3(ctx.view) * light.dir;

            if (light.cos_outer > 0.70710678f)
            {
                // narrow
                entry.bounding_radius = light.radius / (2.0f * light.cos_outer);
                entry.view_bounding_pos = entry.view_pos + entry.view_dir * entry.bounding_radius;
            }
            else
            {
                //wide
                float sin_outer = glm::sqrt(1.0f - light.cos_outer * light.cos_outer);
                entry.bounding_radius = light.radius * sin_outer;
                entry.view_bounding_pos = entry.view_pos + entry.view_dir * (entry.radius * light.cos_outer);
            }

            // shadow map candidate
            constexpr float MAX_SHADOWMAP_LIGHT_DISTANCE = 150.0f;
            if (spotlight_shadow_current_count_ < spotlight_shadow_count_ && (cmd.flags & LF_SHADOWS) &&
                cmd.dist2 < (MAX_SHADOWMAP_LIGHT_DISTANCE * MAX_SHADOWMAP_LIGHT_DISTANCE))
            {
                uint32_t i = spotlight_shadow_current_count_++;

                // setup matrices
                auto& shadowmap = spotlight_shadows_[i];
                shadowmap.proj = glm::perspectiveRH_ZO(2.0f * glm::acos(light.cos_outer), 1.0f, 0.2f, light.radius + 0.1f);
                glm::vec3 up =
                    (std::abs(light.dir.z) < 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
                shadowmap.view = glm::lookAt(light.position, light.position + light.dir, glm::vec3(0.0f, 0.0f, 1.0f));
                shadowmap.view_proj = shadowmap.proj * shadowmap.view;

                entry.shadow_idx = i;
            }

        }
        else
        {
            // pointlight
            entry.view_dir = glm::vec3(1.0f);
            // identical
            entry.bounding_radius = entry.radius;
            entry.view_bounding_pos = entry.view_pos;
        }

    }
}

void gfx::RendererWGPU::EncodeLightCullingPass(wgpu::CommandEncoder& encoder, const DrawContext& ctx)
{
    // ensure bind group
    if (!light_culling_bind_group_)
    {
        CreateLightCullingBindGroup();
    }

    // setup globals
    LightCullingGlobalData globals{};
    globals.screen_size = ctx.viewport_size;
    globals.tile_count = light_tiles_;
    globals.proj = ctx.proj;
    globals.inv_proj = glm::inverse(ctx.proj);
    globals.light_count = lights_.size();
    queue_.WriteBuffer(light_culling_global_buffer_, 0, &globals, sizeof(globals));

    // upload light buffer
    if (!lights_.empty())
    {
        queue_.WriteBuffer(light_buffer_, 0, lights_.data(), lights_.size() * sizeof(lights_[0]));
    }

    // dispatch
    auto pass = encoder.BeginComputePass();
    pass.SetPipeline(light_culling_pipeline_);
    pass.SetBindGroup(0, light_culling_bind_group_);
    pass.DispatchWorkgroups(light_tiles_.x, light_tiles_.y);
    pass.End();
}

void gfx::RendererWGPU::EncodeAO(wgpu::CommandEncoder& encoder, const DrawContext& ctx)
{
    if (!ao_bind_group_)
    {
        CreateAOBindGroup();
    }

    if (!ao_pipeline_)
    {
        CreateAOPipeline();
    }

    // setup globals
    AOGlobalData globals{};
    globals.inv_proj = glm::inverse(ctx.proj);
    globals.proj = ctx.proj;
    globals.viewport_size = ctx.viewport_size;
    globals.radius = 0.25f;
    globals.sample_count = 12;
    globals.steps_per_slice = 4;
    globals.frame_index = static_cast<uint32_t>(frame_index_);
    globals.bias = 0.025f;
    globals.intensity = 1.0f;
    queue_.WriteBuffer(ao_global_buffer_, 0, &globals, sizeof(globals));

    auto count_x = AlignUp(ctx.viewport_size.x, uint32_t(16));
    auto count_y = AlignUp(ctx.viewport_size.y, uint32_t(16));

    auto pass = encoder.BeginComputePass();
    pass.SetPipeline(ao_pipeline_);
    pass.SetBindGroup(0, ao_bind_group_);
    pass.DispatchWorkgroups(count_x, count_y);
    pass.End();
}

void gfx::RendererWGPU::EncodeFXAA(wgpu::RenderPassEncoder& pass, const DrawContext& ctx)
{
    if (!fxaa_bind_group_)
    {
        CreateFXAABindGroup();
    }

    pass.SetPipeline(fxaa_pipeline_);
    pass.SetBindGroup(0, fxaa_bind_group_);
    pass.Draw(4);
}

void gfx::RendererWGPU::EncodeCoronaCmds(wgpu::RenderPassEncoder& pass, std::span<DrawCoronaCmd> cmds,
                                         const DrawContext& ctx)
{
    if (cmds.empty())
    {
        return; // no coronas
    }

    // setup globals
    auto aspect = static_cast<float>(ctx.viewport_size.x) / static_cast<float>(ctx.viewport_size.y);
    
    CoronaGlobalData globals{};
    globals.proj = ctx.proj;
    globals.scale_xy = glm::vec2(1.0f, aspect);
    queue_.WriteBuffer(corona_global_buffer_, 0, &globals, sizeof(globals));

    // prepare coronas
    coronas_.clear();
    for (const auto& cmd : cmds)
    {
        auto& corona = coronas_.emplace_back();
        corona.view_dir = glm::mat3(ctx.view) * cmd.dir;

        if (corona.view_dir.z < 0.0f)
        {
            continue; // not facing camera
        }

        corona.view_dir = glm::normalize(corona.view_dir);

        corona.view_pos = ctx.view * glm::vec4(cmd.pos, 1.0f);

        if (corona.view_pos.z > 0.0f)
        {
            continue; // behind camera
        }

        //corona.size = cmd.size;
        corona.size = 1.0f;
        corona.color = glm::vec4(cmd.color, 1.0f);
    }

    if (coronas_.empty())
    {
        return;
    }

    // setup buffer
    std::span<const uint8_t> corona_buffer_data(reinterpret_cast<const uint8_t*>(coronas_.data()),
                                                coronas_.size() * sizeof(coronas_[0]));
    
    if (ReserveBufferCapacity(corona_buffer_, corona_buffer_data.size_bytes()))
    {
        // need to recreate bind group if buffer relocated
        CreateCoronaBindGroup();
    }

    SetBufferData(corona_buffer_, corona_buffer_data);
    
    // draw
    pass.SetPipeline(corona_pipeline_);
    pass.SetBindGroup(0, corona_bind_group_);
    pass.Draw(4, coronas_.size());
}

void gfx::RendererWGPU::EncodeHudCmds(wgpu::RenderPassEncoder& pass, std::span<DrawHudCmd> queue, const DrawContext& ctx)
{
    // update globals
    GlobalGUIUniformData globals{};
    globals.matrix = GetHudMatrix(ctx.viewport_size);
    queue_.WriteBuffer(gui_global_buffer_, 0, &globals, sizeof(globals));

    pass.SetPipeline(gui_pipeline_);
    pass.SetBindGroup(0, gui_global_bind_group_);

    MeshID last_mesh = 0;
    TextureID last_texture = 0;

    for (const auto& cmd : queue)
    {
        assert(cmd.mesh > 0);
        assert(cmd.texture > 0);

        if (cmd.mesh != last_mesh)
        {
            auto& mesh = meshes_.Get(cmd.mesh);
            pass.SetVertexBuffer(0, mesh.vertex_buffer.buffer);
            pass.SetIndexBuffer(mesh.index_buffer.buffer, wgpu::IndexFormat::Uint32);
            last_mesh = cmd.mesh;
        }

        if (cmd.texture != last_texture)
        {
            auto& texture = textures_.Get(cmd.texture);
            if (!texture.gui_bind_group)
                CreateTextureGuiBindGroup(texture);

            pass.SetBindGroup(1, texture.gui_bind_group);
            last_texture = cmd.texture;
        }

        pass.DrawIndexed(cmd.tri_count * 3, 1, cmd.tri_offset * 3);
    }
}

void gfx::RendererWGPU::Unload()
{
    white_tex_.reset();
}
