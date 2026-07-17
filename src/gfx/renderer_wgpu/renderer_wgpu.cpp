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

CVAR_CL(uint8_t, r_msaa, CV_SAVE, 0, 0, 1);

static std::vector<uint8_t> temp_buffer;

static inline glm::vec3 LinearizeColor(const glm::vec3 color_srgb)
{
    return color_srgb * color_srgb;
}

static inline uint32_t GetMultisampleCount()
{
    return r_msaa.Get() > 0 ? 4 : 1;
}

gfx::RendererWGPU::RendererWGPU(SDL_Window* window) : Renderer(window)
{
    InitWGPU();

    msaa_samples_ = GetMultisampleCount();
 
    csm_resolution_ = 2048;
    csm_num_cascades_ = 4;
    csm_splits_[0] = 15.0f;
    csm_splits_[1] = 45.0f;
    csm_splits_[2] = 100.0f;
    csm_splits_[3] = 200.0f;

    ConfigureSurface();
    CreateGlobalResources();
    CreateMipmapPipeline();
    CreateGuiPipeline();
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
    if (desc.mipmaps)
    {
        texture.mip_levels = std::max(
            1U, std::min(desc.max_mipmap_level, uint32_t(std::floor(std::log2(std::max(desc.width, desc.height))))));
    }

    wgpu::TextureDescriptor tex_desc{};
    tex_desc.dimension = wgpu::TextureDimension::e2D;
    tex_desc.size = {desc.width, desc.height, 1};
    tex_desc.format = wgpu::TextureFormat::RGBA8UnormSrgb;
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

void gfx::RendererWGPU::CreateShadowResources()
{
    // reset first to avoid consuming too much memory
    csm_texture_ = nullptr;
    csm_texture_view_ = nullptr;
    for (auto& cascade : csm_cascades_)
    {
        cascade.texture_view = nullptr;
    }

    // create depth texture array
    {
        wgpu::TextureDescriptor tex_desc{};
        tex_desc.dimension = wgpu::TextureDimension::e2D;
        tex_desc.size = {csm_resolution_, csm_resolution_, csm_num_cascades_};
        tex_desc.format = wgpu::TextureFormat::Depth32Float;
        tex_desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        tex_desc.label = "CSM texture array";
        csm_texture_ = device_.CreateTexture(&tex_desc);

        wgpu::TextureViewDescriptor view_desc{};
        view_desc.dimension = wgpu::TextureViewDimension::e2DArray;
        view_desc.arrayLayerCount = csm_num_cascades_;
        view_desc.label = "CSM texture array view";
        csm_texture_view_ = csm_texture_.CreateView(&view_desc);
    }

    // create individual views
    for (uint32_t i = 0; i < csm_num_cascades_; ++i)
    {
        wgpu::TextureViewDescriptor view_desc{};
        view_desc.dimension = wgpu::TextureViewDimension::e2D;
        view_desc.baseArrayLayer = i;
        view_desc.arrayLayerCount = 1;
        view_desc.label = "CSM texture array layer view";
        csm_cascades_[i].texture_view = csm_texture_.CreateView(&view_desc);
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
}

void gfx::RendererWGPU::CreateSurfaceGlobalResources()
{
    InvalidateSurfacePipelines();
    global_bind_group_layout_ = nullptr;
    global_bind_group_ = nullptr;

    // global bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 3> entries;

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

    // global bind group
    {
        std::array<wgpu::BindGroupEntry, 3> entries;

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

        wgpu::BindGroupDescriptor desc{};
        desc.layout = global_bind_group_layout_;
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Global bind group";
        global_bind_group_ = device_.CreateBindGroup(&desc);
    }
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
    wgpu::DepthStencilState depth_state{};
    depth_state.depthCompare = wgpu::CompareFunction::Always;
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
    std::array<wgpu::BindGroupLayout, 2> bind_groups = {gui_global_bind_group_layout_, gui_texture_bind_group_layout_};
    layout_desc.bindGroupLayoutCount = bind_groups.size();
    layout_desc.bindGroupLayouts = bind_groups.data();
    layout_desc.label = "GUI pipeline layout";
    desc.layout = device_.CreatePipelineLayout(&layout_desc);
    
    gui_pipeline_ = device_.CreateRenderPipeline(&desc);
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

void gfx::RendererWGPU::ConfigureSurface()
{
    auto viewport_size = GetViewportSize();

    wgpu::SurfaceConfiguration config{};
    config.presentMode = wgpu::PresentMode::Immediate;
    config.width = viewport_size.x;
    config.height = viewport_size.y;
    config.device = device_;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    
    wgpu::SurfaceCapabilities caps{};
    surface_.GetCapabilities(adapter_, &caps);
    config.format = GetBestSurfaceFormat({caps.formats, caps.formatCount});
    
    // view format - srgb
    surface_format_ = GetSurfaceSrgbViewFormat(config.format);
    config.viewFormatCount = 1;
    config.viewFormats = &surface_format_;

    surface_.Configure(&config);
    surface_size_ = viewport_size;

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
    depth_desc.usage = wgpu::TextureUsage::RenderAttachment;
    depth_desc.viewFormatCount = 1;
    depth_desc.viewFormats = &depth_format_;
    depth_texture_ = device_.CreateTexture(&depth_desc);

    wgpu::TextureViewDescriptor depth_view_desc{};
    depth_view_desc.aspect = wgpu::TextureAspect::DepthOnly;
    depth_view_desc.baseArrayLayer = 0;
    depth_view_desc.arrayLayerCount = 1;
    depth_view_desc.baseMipLevel = 0;
    depth_view_desc.mipLevelCount = 1;
    depth_view_desc.dimension = wgpu::TextureViewDimension::e2D;
    depth_view_desc.format = depth_format_;
    depth_texture_view_ = depth_texture_.CreateView(&depth_view_desc);

    if (msaa_samples_ > 1)
    {
        // create multisample color texture
        wgpu::TextureDescriptor color_desc{};
        color_desc.dimension = wgpu::TextureDimension::e2D;
        color_desc.format = surface_format_;
        color_desc.mipLevelCount = 1;
        color_desc.sampleCount = msaa_samples_;
        color_desc.size = {viewport_size.x, viewport_size.y};
        color_desc.usage = wgpu::TextureUsage::RenderAttachment;
        color_desc.viewFormatCount = 1;
        color_desc.viewFormats = &surface_format_;
        color_texture_ = device_.CreateTexture(&color_desc);
        color_texture_view_ = color_texture_.CreateView();
    }
    else
    {
        color_texture_ = nullptr;
        color_texture_view_ = nullptr;
    }
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

        if (data.surface_texture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal)
        {
            break;
        }

        if (data.surface_texture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
        {
            // try reconfigure
            ConfigureSurface();
            continue;
        }

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
        shader_module = CreateSurfaceShaderWGPU(device_, flags);
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
    if (r_msaa.IsModified())
    {
        msaa_samples_ = GetMultisampleCount();
        CreateGuiPipeline();
        InvalidateSurfacePipelines();
        ConfigureSurface();
        r_msaa.ClearModified();
    }
}


void gfx::RendererWGPU::Render(Scene& scene, const CameraParams& camera)
{
    auto viewport_size = GetViewportSize();
    if (surface_size_ != viewport_size)
    {
        ConfigureSurface();
    }

    auto encoder = device_.CreateCommandEncoder();

    // compute matrices
    float aspect = static_cast<float>(viewport_size.x) / static_cast<float>(viewport_size.y);

    const float farplane = 3000.0f;

    auto proj = glm::perspectiveRH_ZO(glm::radians(camera.fov * 0.5f), aspect, 0.1f, farplane);
    auto view = glm::lookAt(camera.eye, camera.eye + camera.dir, glm::vec3(0.0f, 0.0f, 1.0f));

    float min_distance = 0.0f;
    float max_distance = 500.0f;

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

    // setup main globals
    GlobalUniformData globals{};
    globals.view = main_ctx.view;
    globals.view_proj = main_ctx.view_proj;
    globals.ambient_color = LinearizeColor(env.ambient_light);
    globals.sun_color = LinearizeColor(env.sun_color) * 1.3f;
    globals.sun_direction = env.sun_direction;
    globals.camera_pos = main_ctx.eye;
    globals.fog = glm::vec4(LinearizeColor(env.fog), env.fog.a);

    globals.csm_texel_size = 1.0f / static_cast<float>(csm_resolution_);
    globals.csm_cascade_count = csm_num_cascades_;
    for (uint32_t i = 0; i < csm_num_cascades_; ++i)
    {
        globals.csm_splits[i] = csm_splits_[i];
        globals.csm_matrices[i] = csm_cascades_[i].view_proj;
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
        auto pass = encoder.BeginRenderPass(&pass_desc);
        EncodePreparedCmds(pass, pcmds_prepass_, globals, pass_index++, SPF_DEPTH_ONLY);
        pass.End();
    }

    // setup surface texture
    auto surface_view = GetNextSurfaceViewData();

    // setup color attachment
    wgpu::RenderPassColorAttachment color_attachment{};

    if (msaa_samples_ <= 1)
    {
        color_attachment.view = surface_view.view;
        color_attachment.storeOp = wgpu::StoreOp::Store;
    }
    else
    {
        color_attachment.view = color_texture_view_;
        color_attachment.resolveTarget = surface_view.view;
        color_attachment.storeOp = wgpu::StoreOp::Discard;
    }

    color_attachment.loadOp = wgpu::LoadOp::Clear;
    auto clear_color = env.clear_color;
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

        // also draw hud in this pass
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
