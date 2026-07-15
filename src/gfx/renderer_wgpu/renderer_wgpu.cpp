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

static std::vector<uint8_t> temp_buffer;

gfx::RendererWGPU::RendererWGPU(SDL_Window* window) : Renderer(window)
{
    InitWGPU();
    ConfigureSurface();
    CreateGlobalResources();
    CreateGuiPipeline();
    SetupPipeline();
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

    wgpu::TextureDescriptor tex_desc{};
    tex_desc.dimension = wgpu::TextureDimension::e2D;
    tex_desc.size = {desc.width, desc.height, 1};
    tex_desc.format = wgpu::TextureFormat::RGBA8Unorm;
    tex_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
    tex_desc.label = "Game 2D texture";
    texture.texture = device_.CreateTexture(&tex_desc);

    texture.view = texture.texture.CreateView();

    return textures_.Alloc(std::move(texture));
}

void gfx::RendererWGPU::SetTextureData(TextureID texture_id, std::span<const uint8_t> data)
{
    auto& texture = textures_.Get(texture_id);

    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = texture.texture;

    wgpu::TexelCopyBufferLayout layout{};
    layout.offset = 0;
    layout.bytesPerRow = texture.desc.width * 4;
    layout.rowsPerImage = texture.desc.height;

    wgpu::Extent3D size{texture.desc.width, texture.desc.height, 1};

    queue_.WriteTexture(&dst, data.data(), data.size_bytes(), &layout, &size);
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

    wgpu::TextureDescriptor tex_desc{};
    tex_desc.dimension = wgpu::TextureDimension::e3D;
    tex_desc.size = {tex_size.x, tex_size.y, tex_size.z};
    tex_desc.format = wgpu::TextureFormat::RGBA8Snorm;
    tex_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
    tex_desc.label = "Deform texture";
    deform.texture = device_.CreateTexture(&tex_desc);

    deform.view = deform.texture.CreateView();

    // setup bind group
    {
        std::array<wgpu::BindGroupEntry, 2> entries;

        auto& sampler_entry = entries[0];
        sampler_entry.binding = 0;
        sampler_entry.sampler = GetSampler(true, false);

        auto& color_texture_entry = entries[1];
        color_texture_entry.binding = 1;
        color_texture_entry.textureView = deform.view;

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

    RenderMainPass(scene, camera);
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
    // SURFACE

    // global bind group layout
    {
        std::array<wgpu::BindGroupLayoutEntry, 1> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
        uniforms_entry.buffer.type = wgpu::BufferBindingType::Uniform;
        uniforms_entry.buffer.minBindingSize = sizeof(GlobalUniformData);

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Global bind group layout";
        global_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }

    // global uniforms buffer
    {
        wgpu::BufferDescriptor desc{};
        desc.size = sizeof(GlobalUniformData);
        desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        desc.label = "Global uniform buffer";
        global_buffer_ = device_.CreateBuffer(&desc);
    }

    // global bind group
    {
        std::array<wgpu::BindGroupEntry, 1> entries;

        auto& uniforms_entry = entries[0];
        uniforms_entry.binding = 0;
        uniforms_entry.buffer = global_buffer_;

        wgpu::BindGroupDescriptor desc{};
        desc.layout = global_bind_group_layout_;
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Global bind group";
        global_bind_group_ = device_.CreateBindGroup(&desc);
    }

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
        std::array<wgpu::BindGroupLayoutEntry, 2> entries;

        auto& sampler_entry = entries[0];
        sampler_entry.binding = 0;
        sampler_entry.visibility = wgpu::ShaderStage::Vertex;
        sampler_entry.sampler.type = wgpu::SamplerBindingType::Filtering;

        auto& texture_entry = entries[1];
        texture_entry.binding = 1;
        texture_entry.visibility = wgpu::ShaderStage::Vertex;
        texture_entry.texture.sampleType = wgpu::TextureSampleType::Float;
        texture_entry.texture.viewDimension = wgpu::TextureViewDimension::e3D;

        wgpu::BindGroupLayoutDescriptor desc{};
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        desc.label = "Deform bind group layout";
        deform_bind_group_layout_ = device_.CreateBindGroupLayout(&desc);
    }


    // GUI

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

        @vertex
        fn vs_main(in: VertexInput) -> VertexOutput {
            var out: VertexOutput;
            let pos2d = uGlobal.matrix * vec3f(in.position.xy, 1.0);
            out.position = vec4f(pos2d.xy, in.position.z, 1.0);
            out.color = in.color;
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
    desc.multisample.count = 1;
    desc.multisample.mask = ~0u;

    // layout
    wgpu::PipelineLayoutDescriptor layout_desc{};
    std::array<wgpu::BindGroupLayout, 2> bind_groups = {gui_global_bind_group_layout_, gui_texture_bind_group_layout_};
    layout_desc.bindGroupLayoutCount = bind_groups.size();
    layout_desc.bindGroupLayouts = bind_groups.data();
    layout_desc.label = "GUI pipeline layout";
    desc.layout = device_.CreatePipelineLayout(&layout_desc);
    
    gui_pipeline_ = device_.CreateRenderPipeline(&desc);
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
    surface_format_ = caps.formats[0];
    config.format = surface_format_;

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
    depth_desc.sampleCount = 1;
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

void gfx::RendererWGPU::SetupPipeline()
{
    wgpu::ShaderModuleDescriptor shader_desc{};
    shader_desc.label = "my sard";

    wgpu::ShaderSourceWGSL shader_src{};
    shader_src.code = SHADER_SRC;

    shader_desc.nextInChain = &shader_src;

    auto shader_module = device_.CreateShaderModule(&shader_desc);

    wgpu::RenderPipelineDescriptor desc{};
    desc.vertex.bufferCount = 0;
    desc.vertex.buffers = nullptr;
    desc.vertex.module = shader_module;
    desc.vertex.entryPoint = "vs_main";

    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.frontFace = wgpu::FrontFace::CCW;
    desc.primitive.cullMode = wgpu::CullMode::None;

    wgpu::BlendState blend{};
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.color.operation = wgpu::BlendOperation::Add;
    //
    blend.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blend.alpha.dstFactor = wgpu::BlendFactor::One;
    blend.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState color{};
    color.format = surface_format_;
    color.blend = &blend;
    color.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment{};
    fragment.module = shader_module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 1;
    fragment.targets = &color;
    desc.fragment = &fragment;

    wgpu::DepthStencilState depth_stencil{};

    desc.depthStencil = nullptr;

    desc.multisample.count = 1;
    desc.multisample.mask = ~0u;

    desc.layout = nullptr;

    pipeline_ = device_.CreateRenderPipeline(&desc);
}

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
    view_desc.format = data.surface_texture.texture.GetFormat();
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
    desc.minFilter = wgpu::FilterMode::Linear;
    desc.lodMinClamp = 0.0f;
    desc.lodMaxClamp = 1.0f;
    desc.label = "A cached sampler";
    auto sampler = device_.CreateSampler(&desc);

    samplers_[hash] = sampler;
    
    return sampler;
}

wgpu::Sampler gfx::RendererWGPU::GetSamplerForTexture(const TextureWGPU& texture)
{
    return GetSampler(texture.desc.filter == TEXTURE_FILTER_LINEAR, texture.desc.mipmaps == TEXTURE_MIPMAP_TYPE_LINEAR);
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
    desc.primitive.cullMode = (flags & SPF_2SIDED) ? wgpu::CullMode::None : wgpu::CullMode::Back;

    // color target
    wgpu::ColorTargetState color{};
    color.format = surface_format_;
    color.writeMask = wgpu::ColorWriteMask::All;

    // blending
    wgpu::BlendState blend{};
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

    // depth
    wgpu::DepthStencilState depth_state{};
    depth_state.depthCompare = wgpu::CompareFunction::Less;
    depth_state.depthWriteEnabled = (flags & SPF_BLEND) ? wgpu::OptionalBool::False : wgpu::OptionalBool::True;
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
    desc.multisample.count = 1;
    desc.multisample.mask = ~0u;

    // layout
    wgpu::PipelineLayoutDescriptor layout_desc{};
    layout_desc.label = "A surface pipeline layout";

    size_t num_bind_groups = 3;
    std::array<wgpu::BindGroupLayout, 4> bind_groups = {global_bind_group_layout_, material_bind_group_layout_, instance_bind_group_layout_, nullptr};
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

void gfx::RendererWGPU::RenderMainPass(Scene& scene, const CameraParams& camera)
{
    auto viewport_size = GetViewportSize();

    // compute matrices
    float aspect = static_cast<float>(viewport_size.x) / static_cast<float>(viewport_size.y);

    const float farplane = 3000.0f;

    auto proj = glm::perspective(glm::radians(camera.fov * 0.5f), aspect, 0.1f, farplane);
    auto view = glm::lookAt(camera.eye, camera.eye + camera.dir, glm::vec3(0.0f, 0.0f, 1.0f));

    float min_distance = 0.0f;
    float max_distance = 500.0f;

    // capture scene
    main_dlist_.Clear();
    DrawContext draw_ctx{main_dlist_, DRAW_PASS_MAIN, camera.eye, view, proj, min_distance, max_distance, viewport_size};
    scene.Draw(draw_ctx);

    if (surface_size_ != viewport_size)
    {
        ConfigureSurface();
    }

    auto surface_view = GetNextSurfaceViewData();

    static float color = 0.0f;
    color += 0.01f;
    color = glm::mod(color, 1.0f);

    wgpu::RenderPassColorAttachment color_attachment{};
    color_attachment.view = surface_view.view;
    color_attachment.loadOp = wgpu::LoadOp::Clear;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    color_attachment.clearValue = {(glm::sin(color * glm::two_pi<float>() * 2.0f) + 1.0f) * 0.1f, 0.1, 0.2, 1.0};

    wgpu::RenderPassDepthStencilAttachment depth_attachment{};
    depth_attachment.view = depth_texture_view_;
    depth_attachment.depthLoadOp = wgpu::LoadOp::Clear;
    depth_attachment.depthStoreOp = wgpu::StoreOp::Store;
    depth_attachment.depthClearValue = 1.0f;

    wgpu::RenderPassDescriptor pass_desc{};
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color_attachment;
    pass_desc.depthStencilAttachment = &depth_attachment;

    auto encoder = device_.CreateCommandEncoder();
    auto pass = encoder.BeginRenderPass(&pass_desc);

    //auto env = scene.GetSceneEnvironment();
    DrawSurfaceList(pass, main_dlist_.surfaces, draw_ctx);
    DrawHudList(pass, main_dlist_.huds, draw_ctx);

    pass.End();

    wgpu::CommandBufferDescriptor cmd_desc{};
    cmd_desc.label = "Command buffer";
    auto command = encoder.Finish(&cmd_desc);
    queue_.Submit(1, &command);

#if !defined(__EMSCRIPTEN__)
    surface_.Present();
#endif
}

void gfx::RendererWGPU::DrawSurfaceList(wgpu::RenderPassEncoder& pass, std::span<DrawSurfaceCmd> queue,
                                        const DrawContext& ctx)
{
    if (queue.empty())
        return;

    GlobalUniformData globals{};
    globals.view_proj = ctx.view_proj;
    queue_.WriteBuffer(global_buffer_, 0, &globals, sizeof(globals));

    struct PreparedCmd
    {
        const DrawSurfaceCmd* cmd = nullptr;
        MeshID mesh_id = 0;
        uint32_t tri_offset = 0;
        uint32_t tri_count = 0;
        MaterialID material_id = 0;
        float dist = 0.0f;
        SurfacePipelineFlags pflags = 0;
    };

    static std::vector<PreparedCmd> pcmds;
    pcmds.clear();

    for (const auto& cmd : queue)
    {
        assert(cmd.mesh > 0);
        assert(cmd.tri_count > 0);
        assert(cmd.material > 0);

        auto& pcmd = pcmds.emplace_back();
        pcmd.cmd = &cmd;
        pcmd.mesh_id = cmd.mesh;
        pcmd.tri_offset = cmd.tri_offset;
        pcmd.tri_count = cmd.tri_count;
        pcmd.material_id = cmd.material;
        pcmd.dist = cmd.dist;

        auto& mesh = meshes_.Get(cmd.mesh);
        auto& material = materials_.Get(cmd.material);

        auto& mat_vals = material.desc.properties;

        // MESH
        if ((mesh.desc.attributes & MESH_VERTEX_ATTR_BONE_DATA) > 0 && cmd.pose > 0)
        {
            // skeletal
            pcmd.pflags |= SPF_SKELETAL;
        }
        else if (cmd.deform_tex > 0)
        {
            // deform grid
            // TODO: implement uniform data for this
            //pcmd.pflags |= SPF_DEFORM;
        }

        // MATERIAL
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

        // 2sided
        if (mat_vals.twosided)
        {
            pcmd.pflags |= SPF_2SIDED;
        }        
         
        // alpha culling
        if (mat_vals.blend == MATERIAL_BLEND_TYPE_NONE && (mat_vals.color == MATERIAL_OBJECT_COLOR_TYPE_NONE ||
                                                           mat_vals.color == MATERIAL_OBJECT_COLOR_TYPE_MULTIPLY))
        {
            pcmd.pflags |= SPF_CULL_ALPHA;
        }
    }

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

    uint32_t first_instance = 0;
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

    static std::vector<InstanceUniformData> instance_data_;
    instance_data_.clear();
    instance_data_.reserve(pcmds.size());

    if (ReserveBufferCapacity(instance_buffer_, pcmds.size() * sizeof(instance_data_[0])))
    {
        // need to recreate bind group if reallocated
        CreateInstanceBufferBindGroup();
    }

    pass.SetBindGroup(0, global_bind_group_);
    pass.SetBindGroup(2, instance_bind_group_);
    
    for (const auto& pcmd : pcmds)
    {
        auto& cmd = *pcmd.cmd;

        if (pcmd.material_id != batch_material_id || pcmd.mesh_id != batch_mesh_id ||
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

        auto& instance_data = instance_data_.emplace_back();
        instance_data.matrix = cmd.matrix ? *cmd.matrix : glm::mat4(1.0f);

        if (pcmd.pflags & (SPF_OBJECT_COLOR | SPF_MULTICOLOR))
        {
            for (size_t i = 0; i < cmd.colors.size(); ++i)
            {
                instance_data.colors[i] = glm::packUnorm4x8(cmd.colors[i]);
            }
        }

        ++num_instances;
    }

    flush_batch();

    // upload instance data
    SetBufferData(instance_buffer_, {reinterpret_cast<const uint8_t*>(instance_data_.data()),
                                     instance_data_.size() * sizeof(instance_data_[0])}); 

}

void gfx::RendererWGPU::DrawHudList(wgpu::RenderPassEncoder& pass, std::span<DrawHudCmd> queue, const DrawContext& ctx)
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
