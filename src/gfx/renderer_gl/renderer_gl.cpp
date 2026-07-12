#include "renderer_gl.hpp"

#include <algorithm>
#include <ranges>
#include <stdexcept>

#include <SDL.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include "shader_sources.hpp"
#include "assets/asset_manager.hpp"
#include "utils/sdl_utils.hpp"
#include "utils/cvars.hpp"

CVAR_CL(uint8_t, r_vertex_lighting, CV_SAVE, 0, 0, 1);

#ifndef PG_GLES
static void APIENTRY GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                     const GLchar* message, const void* userParam)
{
    if (severity == 0x826b)
        return;

    ////std::cout << message << std::endl;
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}

#endif // PG_GLES

gfx::GLRenderer::GLRenderer(SDL_Window* window) : Renderer(window)
{
    std::cout << "Initializing GL renderer" << std::endl;

    std::cout << "Creating OpenGL context..." << std::endl;
    gl_context_ = SDL_GL_CreateContext(window);
    if (!gl_context_)
    {
        ThrowSDLError("SDL_GL_CreateContext");
    }

    // Make context current
    if (SDL_GL_MakeCurrent(window, gl_context_) != 0)
    {
        SDL_GL_DeleteContext(gl_context_);
        ThrowSDLError("SDL_GL_MakeCurrent");
    }

#ifndef PG_GLES
    // Initialize GLAD
    std::cout << "Initializing GLAD..." << std::endl;
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        SDL_GL_DeleteContext(gl_context_);
        throw std::runtime_error("Failed to initialize GLAD");
    }
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(GLDebugCallback, 0);

    SDL_GL_SetSwapInterval(0);
#endif // PG_GLES
}

gfx::MeshID gfx::GLRenderer::CreateMesh(const MeshDescriptor& desc)
{
    auto id = meshes_.Alloc(desc);
    return id;
}

void gfx::GLRenderer::SetMeshVertexData(MeshID mesh_id, const MeshVertexData& data)
{
    auto& mesh = meshes_.Get(mesh_id);
    mesh.SetVertexData(data);
}

void gfx::GLRenderer::SetMeshTriangleData(MeshID mesh_id, const MeshTriangleData& data)
{
    auto& mesh = meshes_.Get(mesh_id);
    mesh.SetTriangleData(data);
}

void gfx::GLRenderer::ReleaseMesh(MeshID mesh_id)
{
    meshes_.Free(mesh_id);
}

gfx::TextureID gfx::GLRenderer::CreateTexture(const TextureDescriptor& desc)
{
    bool linear = desc.filter == TEXTURE_FILTER_LINEAR;
    bool mipmaps = desc.mipmaps == TEXTURE_MIPMAP_TYPE_LINEAR;

    auto id = textures_.Alloc(desc.width, desc.height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, linear, mipmaps);
    return id;
}

void gfx::GLRenderer::SetTextureData(TextureID texture_id, std::span<const uint8_t> data)
{
    auto& texture = textures_.Get(texture_id);
    texture.SetData(data);
}

void gfx::GLRenderer::ReleaseTexture(TextureID texture_id)
{
    textures_.Free(texture_id);
}

gfx::MaterialID gfx::GLRenderer::CreateMaterial(const MaterialDescriptor& desc)
{
    auto id = materials_.Alloc(desc);
    return id;
}

void gfx::GLRenderer::ReleaseMaterial(MaterialID material_id)
{
    materials_.Free(material_id);
}

gfx::SkeletonPoseID gfx::GLRenderer::CreateSkeletonPose(const SkeletonPoseDescriptor& desc)
{
    auto id = poses_.Alloc(desc.num_bones);
    return id;
}

void gfx::GLRenderer::SetSkeletonPoseTransforms(SkeletonPoseID pose_id, std::span<const glm::mat4> transforms)
{
    auto& pose = poses_.Get(pose_id);
    pose.SetData(transforms);
}

void gfx::GLRenderer::ReleaseSkeletonPose(SkeletonPoseID pose_id)
{
    poses_.Free(pose_id);
}

gfx::DeformTextureID gfx::GLRenderer::CreateDeformTexture(const DeformTextureDescriptor& desc)
{
    auto id = deform_textures_.Alloc(desc.grid);
    return id;
}

void gfx::GLRenderer::SetDeformTextureData(DeformTextureID deform_id, std::span<const glm::i8vec3> data)
{
    auto& deform_tex = deform_textures_.Get(deform_id);
    deform_tex.SetData(data);
}

void gfx::GLRenderer::ReleaseDeformTexture(DeformTextureID deform_id)
{
    deform_textures_.Free(deform_id);
}

void gfx::GLRenderer::Draw(Scene& scene, const CameraParams& camera)
{
    Load();

    auto viewport_size = GetViewportSize();

    // compute matrices
    float aspect = static_cast<float>(viewport_size.x) / static_cast<float>(viewport_size.y);

    const float farplane = 3000.0f;

    auto proj = glm::perspective(glm::radians(camera.fov * 0.5f), aspect, 0.1f, farplane);
    auto view = glm::lookAt(camera.eye, camera.eye + camera.dir, glm::vec3(0.0f, 0.0f, 1.0f));

    float min_distance = 0.0f;
    float max_distance = 500.0f;

    // capture scene
    dlist_.Clear();
    DrawContext draw_ctx{dlist_, DRAW_PASS_MAIN, camera.eye, view, proj, min_distance, max_distance, viewport_size};
    scene.Draw(draw_ctx);

    DrawInfo info{draw_ctx};
    info.env = scene.GetSceneEnvironment();

    current_shader_ = nullptr;
    glViewport(0, 0, viewport_size.x, viewport_size.y);
    glClearColor(info.env.clear_color.r, info.env.clear_color.g, info.env.clear_color.b, 1.0f);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto& list = dlist_;

    light_grid_chunks_size_ = glm::max(scene.GetMapChunkSize(), 50.0f); // this causes lags if too small so clamp at 50 min
    CreateLightGrid(list.lights, info);

    DrawSurfaceList(list.surfaces, info);
    DrawBeamList(list.beams, info);
    DrawCoronaList(list.coronas, info);
    DrawHudList(list.huds, info);
}

gfx::GLRenderer::~GLRenderer()
{
    Unload();

    SDL_GL_DeleteContext(gl_context_);

}

void gfx::GLRenderer::Load()
{
    if (loaded_)
        return;

    loaded_ = true;

    SetupShaders();
    SetupBeamVA();
    SetupCoronaVA();
}

void gfx::GLRenderer::SetupShaders()
{
    ShaderSources::MakeShader(solid_shader_, SS_SOLID_VERT, SS_SOLID_FRAG);
    ShaderSources::MakeShader(hud_shader_, SS_HUD_VERT, SS_HUD_FRAG);
    ShaderSources::MakeShader(beam_shader_, SS_BEAM_VERT, SS_BEAM_FRAG);
}

struct BeamSegment
{
    glm::vec3 p0;
    uint32_t color;
    glm::vec3 p1;
    float radius;
};

void gfx::GLRenderer::SetupBeamVA()
{
    beam_va_ = std::make_unique<VertexArray>(VA_POSITION, 0);

    static const float quad_points[] = {
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
    };

    beam_va_->SetVBOData(quad_points, sizeof(quad_points));

    // create points buffer
    glBindVertexArray(beam_va_->GetVAOId());
    beam_segments_vbo_ = std::make_unique<BufferObject>(GL_ARRAY_BUFFER, GL_STREAM_DRAW);
    beam_segments_vbo_->Bind();

    constexpr size_t STRIDE = sizeof(BeamSegment);

    // p0
    glEnableVertexAttribArray(10);
    glVertexAttribPointer(10, 3, GL_FLOAT, GL_FALSE, STRIDE, (const void*)offsetof(BeamSegment, p0));
    glVertexAttribDivisor(10, 1);

    // color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, STRIDE, (const void*)offsetof(BeamSegment, color));
    glVertexAttribDivisor(2, 1);

    // p1
    glEnableVertexAttribArray(11);
    glVertexAttribPointer(11, 3, GL_FLOAT, GL_FALSE, STRIDE, (const void*)offsetof(BeamSegment, p1));
    glVertexAttribDivisor(11, 1);

    // radius
    glEnableVertexAttribArray(12);
    glVertexAttribPointer(12, 1, GL_FLOAT, GL_FALSE, STRIDE, (const void*)offsetof(BeamSegment, radius));
    glVertexAttribDivisor(12, 1);

    glBindVertexArray(0);
}

struct CoronaVertex
{
    glm::vec3 pos;
    uint32_t color;
    glm::vec2 uv;
};

void gfx::GLRenderer::SetupCoronaVA()
{
    corona_va_ = std::make_unique<VertexArray>(VA_POSITION | VA_COLOR | VA_UV, VF_CREATE_EBO | VF_DYNAMIC);
    corona_tex_ = assets::AssetManager::GetInstance().Get<Texture>("corona");
}

void gfx::GLRenderer::Unload()
{
    hud_shader_.reset();
    corona_tex_.reset();
    corona_va_.reset();
    beam_shader_.reset();
    beam_va_.reset();
    beam_segments_vbo_.reset();
    solid_shader_.reset();
    surface_shaders_.clear();
}

void gfx::GLRenderer::InvalidateShaders()
{
    // invalidate surface shaders
    for (auto& [flags, sshader] : surface_shaders_)
    {
        InvalidateSurfaceShader(sshader);
    }
}

gfx::SurfaceShader& gfx::GLRenderer::GetSurfaceShader(SurfaceRenderFlags flags)
{
    auto it = surface_shaders_.find(flags);

    // not yet generated
    if (it == surface_shaders_.end())
    {
        SurfaceShader& sshader = surface_shaders_[flags];
        sshader.shader = CreateSurfaceShader(flags, sshader.iflags);

        return sshader;
    }

    return it->second;
}

void gfx::GLRenderer::SetupSurfaceShader(SurfaceShader& sshader, const DrawInfo& info)
{
    const Shader& shader = *sshader.shader;

    if (current_shader_ != &shader)
    {
        glUseProgram(shader.GetId());
        current_shader_ = &shader;
    }

    if (sshader.global_setup)
    {
        return; // Global uniforms are already set up
    }

    glUniformMatrix4fv(shader.U(SU_VIEW_PROJ), 1, GL_FALSE, &info.ctx.view_proj[0][0]);
    sshader.prev_decal = false;

    if (sshader.iflags & SIF_FOG_DATA)
    {
        glUniform3fv(shader.U(SU_CAMERA_POS), 1, &info.ctx.eye[0]);
        glUniform4fv(shader.U(SU_FOG), 1, &info.env.fog[0]);
    }

    // setup lighting
    if (sshader.iflags & SIF_LIGHTING_DATA)
    {
        glUniform3fv(shader.U(SU_AMBIENT_LIGHT), 1, &info.env.ambient_light[0]);
        glUniform3fv(shader.U(SU_SUN_COLOR), 1, &info.env.sun_color[0]);
        glUniform3fv(shader.U(SU_SUN_DIRECTION), 1, &info.env.sun_direction[0]);
    }

    sshader.global_setup = true;
}

void gfx::GLRenderer::InvalidateSurfaceShader(SurfaceShader& sshader)
{
    sshader.global_setup = false;
    sshader.color = nullptr;
}

void gfx::GLRenderer::CreateLightGrid(std::span<DrawLightCmd> light_cmds, const DrawInfo& info)
{
    light_grid_.clear();
    light_grid_chunks_.clear();

    // calc distance
    for (auto& cmd : light_cmds)
    {
        auto d = cmd.light.position - info.ctx.eye;
        cmd.dist2 = glm::dot(d, d);
    }

    // sort lights by distance so closer lights get rendered instead of random npc vehicle headlights in 2km
    std::ranges::sort(light_cmds, [](const DrawLightCmd& a, const DrawLightCmd& b) { return a.dist2 < b.dist2; });

    for (const auto& cmd : light_cmds)
    {
        AddLightToGrid(cmd.light, light_grid_, light_grid_size_);
        AddLightToGrid(cmd.light, light_grid_chunks_, light_grid_chunks_size_);
    }
}

void gfx::GLRenderer::AddLightToGrid(const LightData& light, LightGrid& grid, float cell_size)
{
    float margin = light.radius + cell_size * 0.2f;

    auto aabb_min = GetCellCoord(light.position - margin, cell_size);
    auto aabb_max = GetCellCoord(light.position + margin, cell_size);

    for (int y = aabb_min.y; y <= aabb_max.y; ++y)
    {
        for (int x = aabb_min.x; x <= aabb_max.x; ++x)
        {
            auto hash = HashCellCoord(LightCellCoord(static_cast<uint16_t>(x), static_cast<uint16_t>(y)));
            auto& cell = grid[hash];

            if (cell.num_lights >= LIGHT_GRID_CELL_LIGHTS)
                continue;

            cell.lights[cell.num_lights] = light;
            ++cell.num_lights;
        }
    }
}

void gfx::GLRenderer::DrawSurfaceList(std::span<DrawSurfaceCmd> list, const DrawInfo& info)
{
    // TODO: split this all-the-responsibilities function into multiple

    if (list.empty())
        return;

    // setup view/proj matrix for decals
    auto decal_proj = info.ctx.proj;
    decal_proj[2][3] -= 0.0005f;
    auto decal_view_proj = decal_proj * info.ctx.view;

    struct PreparedSurfaceDrawCmd
    {
        const DrawSurfaceCmd* cmd = nullptr;
        
        // mesh
        GLuint vao_id = 0;
        MeshIndex tri_offset = 0;
        MeshIndex tri_count = 0;

        // material
        GLuint texture_id = 0;

        // instance
        GLuint bone_ubo_id = 0;
        GLuint deform_tex_id = 0;
        float dist = 0.0f;
        
        SurfaceRenderFlags rflags = 0;
    };

    static std::vector<PreparedSurfaceDrawCmd> prepared_cmds;
    prepared_cmds.clear();

    // prepare cmds
    for (const auto& cmd : list)
    {
        assert(cmd.mesh > 0);
        assert(cmd.material > 0);

        assert(cmd.tri_count > 0);

        auto& mesh = meshes_.Get(cmd.mesh);
        auto& material = materials_.Get(cmd.material);

        auto mat_texture = material.GetTexture();
        auto& mat_vals = material.GetProperties();

        // push prepared cmd
        auto& pcmd = prepared_cmds.emplace_back();
        pcmd.cmd = &cmd;

        // MESH
        pcmd.vao_id = mesh.GetVaoId();
        pcmd.tri_offset = cmd.tri_offset;
        pcmd.tri_count = cmd.tri_count;
         
        // skeletal
        if (mesh.GetAttrs() & MESH_VERTEX_ATTR_BONE_DATA)
        {
            auto& pose = poses_.Get(cmd.pose);
            pcmd.bone_ubo_id = pose.GetUboId();
            pcmd.rflags |= SRF_SKELETAL;
        }

        // MATERIAL
        // texture
        if (mat_texture > 0)
        {
            auto& texture = textures_.Get(mat_texture);
            pcmd.texture_id = texture.GetId();
            pcmd.rflags |= SRF_TEXTURE;
        }

        // blending type
        if (mat_vals.blend != MATERIAL_BLEND_TYPE_NONE)
        {
            pcmd.rflags |= SRF_BLEND;
            if (mat_vals.blend == MATERIAL_BLEND_TYPE_ADDITIVE)
                pcmd.rflags |= SRF_BLEND_ADDITIVE;
        }
        else
        {
            // fog only if not blended
            pcmd.rflags |= SRF_FOG;
        }

        // deform texture
        if (cmd.deform_tex > 0)
        {
            auto& deform_tex = deform_textures_.Get(cmd.deform_tex);
            pcmd.deform_tex_id = deform_tex.GetId();
            pcmd.rflags |= SRF_DEFORM;
        }

        // lighting
        if (mat_vals.lighting != MATERIAL_LIGHTING_TYPE_UNLIT)
        {
            pcmd.rflags |= SRF_LIT;

            if (mat_vals.lighting == MATERIAL_LIGHTING_TYPE_VERTEX || r_vertex_lighting.Get() > 0)
            {
                pcmd.rflags |= SRF_LIT_VERTEX;
            }

            // translucency
            if (mat_vals.translucent)
            {
                pcmd.rflags |= SRF_TRANSLUCENT;
            }
        }

        // color
        if (!cmd.colors.empty())
        {
            if (mat_vals.color == MATERIAL_OBJECT_COLOR_TYPE_MULTIPLY)
                pcmd.rflags |= SRF_OBJECT_COLOR;
            else if (mat_vals.color == MATERIAL_OBJECT_COLOR_TYPE_BACKGROUND)
                pcmd.rflags |= SRF_OBJECT_COLOR | SRF_OBJECT_COLOR_BACKGROUND;
            else if (mat_vals.color == MATERIAL_OBJECT_COLOR_TYPE_MULTICOLOR)
                pcmd.rflags |= SRF_MULTICOLOR;
        }

        // twosided
        if (mat_vals.twosided)
        {
            pcmd.rflags |= SRF_2SIDED;
        }

        // alpha culling
        if (mat_vals.blend == MATERIAL_BLEND_TYPE_NONE && (mat_vals.color == MATERIAL_OBJECT_COLOR_TYPE_NONE ||
            mat_vals.color == MATERIAL_OBJECT_COLOR_TYPE_MULTIPLY))
        {
            pcmd.rflags |= SRF_CULL_ALPHA;
        }

        if (mat_vals.decal)
        {
            pcmd.rflags |= SRF_DECAL;
        }
    }

    // sort the list to minimize state changes
    std::ranges::sort(prepared_cmds, [](const PreparedSurfaceDrawCmd& a, const PreparedSurfaceDrawCmd& b) {
        const auto blend_a = a.rflags & SRF_BLEND;
        const auto blend_b = b.rflags & SRF_BLEND;

        if (blend_a != blend_b)
            return blend_b > 0; // opaque first

        if (blend_a) // both blended
        {
            return a.dist > b.dist; // do not optimize blended, sort by distance instead
        }

        return std::tie(a.rflags, a.texture_id, a.vao_id, a.tri_offset, a.tri_count) <
               std::tie(b.rflags, b.texture_id, b.vao_id, b.tri_offset, b.tri_count);
    });

    glActiveTexture(GL_TEXTURE0); // for all future bindings

    // cache to eliminate fake state changes
    SurfaceShader* sshader = nullptr;
    GLuint last_texture = 0;
    GLuint last_vao = 0;
    GLuint last_bone_ubo = 0;
    GLuint last_deform_tex = 0;

    InvalidateShaders();

    // enable depth test
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // reset face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // reset blending
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // set to opacity blending default

    SurfaceRenderFlags last_rflags = 0;

    for (const auto& pcmd : prepared_cmds)
    {
        const auto& cmd = *pcmd.cmd;

        SurfaceRenderFlags rflags_diff = last_rflags ^ pcmd.rflags;

        // sync 2sided
        if (rflags_diff & SRF_2SIDED)
        {
            if (pcmd.rflags & SRF_2SIDED)
                glDisable(GL_CULL_FACE);
            else
                glEnable(GL_CULL_FACE);
        }

        // setup shader
        SurfaceRenderFlags last_shader_rflags = last_rflags & SRF__SHADER;
        SurfaceRenderFlags shader_rflags = pcmd.rflags & SRF__SHADER;

        if (last_shader_rflags != shader_rflags || !sshader)
        {
            sshader = &GetSurfaceShader(shader_rflags);
            SetupSurfaceShader(*sshader, info);
        }

        auto shader = sshader->shader.get();

        static const glm::mat4 identity(1.0f);
        const glm::mat4* model = &identity;

        // set model matrix
        if (cmd.matrix)
        {
            model = cmd.matrix;
        }

        glUniformMatrix4fv(shader->U(SU_MODEL), 1, GL_FALSE, &(*model)[0][0]);

        // sync color
        if (sshader->iflags & SIF_OBJECT_COLOR)
        {
            assert(!cmd.colors.empty());

            if (sshader->color != &cmd.colors[0])
            {
                glUniform4fv(shader->U(SU_COLOR), 1, &cmd.colors[0][0]);
                sshader->color = &cmd.colors[0];
            }
        }
        else if (sshader->iflags & SIF_MULTICOLOR_DATA)
        {
            assert(!cmd.colors.empty());

            if (sshader->color != &cmd.colors[0])
            {
                glUniform4fv(shader->U(SU_COLOR), cmd.colors.size(), &cmd.colors[0][0]);
                sshader->color = &cmd.colors[0];
            }
        }

        // sync blending
        if (rflags_diff & SRF__ANY_BLEND) // this is two flags so not strictly correct but works
        {
            if (pcmd.rflags & SRF__ANY_BLEND)
            {
                glEnable(GL_BLEND);
                glDepthMask(GL_FALSE);
            }
            else
            {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            }
        }

        // sync blending type
        if ((pcmd.rflags & SRF__ANY_BLEND) && (rflags_diff & SRF_BLEND_ADDITIVE))
        {
            if (pcmd.rflags & SRF_BLEND_ADDITIVE)
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            else
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        // sync view_proj by SRF_DECAL
        bool is_decal = pcmd.rflags & SRF_DECAL;
        if (is_decal != sshader->prev_decal)
        {
            if (is_decal)
            {
                glUniformMatrix4fv(shader->U(SU_VIEW_PROJ), 1, GL_FALSE, &decal_view_proj[0][0]);
            }
            else
            {
                glUniformMatrix4fv(shader->U(SU_VIEW_PROJ), 1, GL_FALSE, &info.ctx.view_proj[0][0]);
            }
            sshader->prev_decal = is_decal;
        }

        // sync lights
        if (sshader->iflags & SIF_LIGHTING_DATA)
        {
            const LightGridCell* cell = nullptr;
            if (cmd.map_chunk_hash > 0) // chunk mesh
            {
                auto it = light_grid_chunks_.find(cmd.map_chunk_hash);
                if (it != light_grid_chunks_.end())
                {
                    cell = &it->second;
                }
            }
            else
            {
                auto center = glm::vec3((*model)[3]);
                // std::cerr << center.x << ' ' << center.y << ' ' << center.z << std::endl;
                auto it = light_grid_.find(HashCellCoord(GetCellCoord(center, light_grid_size_)));
                if (it != light_grid_.end())
                {
                    cell = &it->second;
                }
            }

            size_t num_lights = cell ? cell->num_lights : 0;
            if (num_lights != sshader->num_lights)
            {
                glUniform1i(shader->U(SU_NUM_LIGHTS), num_lights);
                sshader->num_lights = num_lights;
            }

            if (num_lights > 0)
            {
                glUniformMatrix3x4fv(shader->U(SU_LIGHT_DATA), num_lights, GL_FALSE, &cell->lights->position.x);
            }
        }

        // bind texture
        if ((sshader->iflags & SIF_COLOR_TEXTURE) && last_texture != pcmd.texture_id)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, pcmd.texture_id);
            last_texture = pcmd.texture_id;
        }

        // bind skinning UBO
        if ((sshader->iflags & SIF_SKELETAL_DATA) && last_bone_ubo != pcmd.bone_ubo_id)
        {
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, pcmd.bone_ubo_id);
            last_bone_ubo = pcmd.bone_ubo_id;
        }

        // bind deform texture
        if ((sshader->iflags & SIF_DEFORM_DATA) && last_deform_tex != pcmd.deform_tex_id)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, pcmd.deform_tex_id);
            last_deform_tex = pcmd.deform_tex_id;

            // update deform tex info
            const auto& deform_info = deform_textures_.Get(cmd.deform_tex).GetInfo();
            glm::mat3 deform_info_mat;
            deform_info_mat[0] = deform_info.min;
            deform_info_mat[1] = deform_info.max;
            deform_info_mat[2] = glm::vec3(deform_info.max_offset, 0.0f, 0.0f);
            glUniformMatrix3fv(shader->U(SU_DEFORM_INFO), 1, GL_FALSE, &deform_info_mat[0][0]);
        }

        // bind VAO
        if (last_vao != pcmd.vao_id)
        {
            glBindVertexArray(pcmd.vao_id);
            last_vao = pcmd.vao_id;
        }

        // draw
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(pcmd.tri_count * 3U), GL_UNSIGNED_INT,
                       (void*)(pcmd.tri_offset * 3U * sizeof(GLuint)));

        last_rflags = pcmd.rflags;
    }
}

static float GetRandomOffset(float max_offset)
{
    return (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 2.0f * max_offset;
}

void gfx::GLRenderer::DrawBeamList(std::span<DrawBeamCmd> queue, const DrawInfo& info)
{
    static std::vector<BeamSegment> segments;
    static std::vector<glm::vec3> points;
    segments.clear();

    for (const auto& cmd : queue)
    {
        if (cmd.num_segments < 1)
            continue;

        points.resize(cmd.num_segments + 1);

        const glm::vec3 seg_step = (cmd.end - cmd.start) / static_cast<float>(cmd.num_segments);
        glm::vec3 pos = cmd.start;
        for (size_t i = 0; i <= cmd.num_segments; ++i)
        {
            points[i] = pos;
            pos += seg_step;
        }

        if (cmd.max_offset > 0.0f)
        {
            for (auto& p : points)
            {
                p.x += GetRandomOffset(cmd.max_offset);
                p.y += GetRandomOffset(cmd.max_offset);
                p.z += GetRandomOffset(cmd.max_offset);
            }
        }

        for (size_t i = 1; i < points.size(); ++i)
        {
            auto& segment = segments.emplace_back();
            segment.p0 = points[i - 1];
            segment.p1 = points[i];
            segment.color = cmd.color;
            segment.radius = cmd.radius;
        }
    }

    if (segments.empty())
        return;

    glBindVertexArray(beam_va_->GetVAOId());
    beam_segments_vbo_->SetData(segments.data(), segments.size() * sizeof(segments[0]));

    Shader* shader = beam_shader_.get();
    glUseProgram(shader->GetId());
    current_shader_ = shader;

    glDisable(GL_CULL_FACE);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // ADDITIVE blend

    glUniformMatrix4fv(shader->U(gfx::SU_VIEW_PROJ), 1, GL_FALSE, &info.ctx.view_proj[0][0]);
    glUniform3fv(shader->U(gfx::SU_CAMERA), 1, &info.ctx.eye[0]);

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, segments.size());

    glBindVertexArray(0);
}

void gfx::GLRenderer::DrawCoronaList(std::span<DrawCoronaCmd> queue, const DrawInfo& info)
{
    if (queue.empty())
        return;

    // create array
    static std::vector<CoronaVertex> vertices;
    static std::vector<uint32_t> indices;
    vertices.clear();
    indices.clear();

    // auto cam_backward = glm::transpose(glm::mat3(params.view))[2];

    auto aspect = static_cast<float>(info.ctx.viewport_size.x) / static_cast<float>(info.ctx.viewport_size.y);
    glm::vec2 size_scale(1.0f, aspect);

    for (const auto& corona : queue)
    {
        auto to_cam = info.ctx.eye - corona.pos;
        auto dist = glm::length(to_cam);
        if (dist < 0.001f)
            continue;

        to_cam /= dist;

        glm::vec4 clip_pos = info.ctx.view_proj * glm::vec4(corona.pos + to_cam * 0.1f, 1.0f);
        if (clip_pos.w == 0.0f)
            continue;

        glm::vec3 ndc_pos = glm::vec3(clip_pos) / clip_pos.w;

        if (ndc_pos.z < -1.0f || ndc_pos.z > 1.0f)
            continue; // behind camera

        float intensity = glm::sqrt(glm::max(glm::dot(corona.dir, to_cam), 0.0f));
        float mult = glm::mix(0.0f, 0.7f, intensity);
        float scale = glm::mix(0.3f, 1.0f, intensity);

        dist = glm::max(dist, 0.1f);

        constexpr float a = 1.0f;
        constexpr float b = 0.15f;
        float size = 0.1f * scale * (1.0f / (a + b * dist));
        auto size_xy = size * size_scale;

        glm::vec4 color(corona.color * mult, 1.0f);
        uint32_t color_u32 = glm::packUnorm4x8(color);

        uint32_t base_index = vertices.size();

        vertices.emplace_back(
            CoronaVertex{ndc_pos + glm::vec3(-size_xy.x, -size_xy.y, 0.0f), color_u32, glm::vec2(0.0f, 0.0f)});
        vertices.emplace_back(
            CoronaVertex{ndc_pos + glm::vec3(size_xy.x, -size_xy.y, 0.0f), color_u32, glm::vec2(1.0, 0.0f)});
        vertices.emplace_back(
            CoronaVertex{ndc_pos + glm::vec3(size_xy.x, size_xy.y, 0.0f), color_u32, glm::vec2(1.0f, 1.0f)});
        vertices.emplace_back(
            CoronaVertex{ndc_pos + glm::vec3(-size_xy.x, size_xy.y, 0.0f), color_u32, glm::vec2(0.0f, 1.0f)});

        indices.push_back(base_index + 0);
        indices.push_back(base_index + 1);
        indices.push_back(base_index + 2);
        indices.push_back(base_index + 0);
        indices.push_back(base_index + 2);
        indices.push_back(base_index + 3);
    }

    if (indices.empty())
        return;

    // upload
    corona_va_->SetVBOData(vertices.data(), vertices.size() * sizeof(vertices[0]));
    corona_va_->SetIndices(indices.data(), indices.size());

    // render

    Shader* shader = hud_shader_.get();
    current_shader_ = shader;
    glUseProgram(shader->GetId());

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glm::mat3 matrix(1.0f);
    glUniformMatrix3fv(shader->U(SU_MODEL), 1, GL_FALSE, &matrix[0][0]);

    glBindVertexArray(corona_va_->GetVAOId());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_.Get(corona_tex_->GetID()).GetId());

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, (void*)0);

}

void gfx::GLRenderer::DrawHudList(std::span<DrawHudCmd> queue, const DrawInfo& info)
{
    if (queue.empty())
        return;

    // cannot sort anything here, must be drawn in FIFO order for correct overlay

    Shader* shader = hud_shader_.get();
    current_shader_ = shader;
    glUseProgram(shader->GetId());

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(info.ctx.viewport_size.x);
    float h = static_cast<float>(info.ctx.viewport_size.y);
    glm::vec2 screen_size_px(w, h);
    glm::vec2 ndc_scale(2.0f / screen_size_px.x, -2.0f / screen_size_px.y);
    constexpr glm::vec2 ndc_offset(-1.0f, 1.0f);

    glm::mat3 matrix(1.0f);
    matrix[0][0] = ndc_scale.x;
    matrix[1][1] = ndc_scale.y;
    matrix[2][0] = ndc_offset.x;
    matrix[2][1] = ndc_offset.y;

    glUniformMatrix3fv(shader->U(SU_MODEL), 1, GL_FALSE, &matrix[0][0]);

    GLuint last_texture = 0;
    GLuint last_vao = 0;
    const glm::mat3* last_matrix = nullptr;

    glActiveTexture(GL_TEXTURE0);

    for (const auto& cmd : queue)
    {
        assert(cmd.mesh > 0);
        assert(cmd.texture > 0);
        assert(cmd.tri_count > 0);

        auto& mesh = meshes_.Get(cmd.mesh);
        auto& texture = textures_.Get(cmd.texture);

        GLuint vao_id = mesh.GetVaoId();
        GLuint tex_id = texture.GetId();

        // update matrix
        if (last_matrix != cmd.matrix)
        {
            glUniformMatrix3fv(shader->U(SU_MODEL), 1, GL_FALSE, &(matrix * *cmd.matrix)[0][0]);
            last_matrix = cmd.matrix;
        }

        // bind texture
        if (last_texture != tex_id)
        {
            glBindTexture(GL_TEXTURE_2D, tex_id);
            last_texture = tex_id;
        }

        // bind vao
        if (last_vao != vao_id)
        {
            glBindVertexArray(vao_id);
            last_vao = vao_id;
        }

        uint32_t first = cmd.tri_offset * 3;
        uint32_t count = cmd.tri_count * 3;

        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT, (void*)(first * sizeof(GLuint)));
    }

}

