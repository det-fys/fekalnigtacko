#include "renderer.hpp"

#include <algorithm>
#include <ranges>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "client/gl.hpp"

#include "shader_sources.hpp"
#include "shader_defs.hpp"

#include "utils/cvars.hpp"
#include "assets/asset_manager.hpp"

CVAR_CL(uint8_t, r_vertex_lighting, CV_SAVE, 0, 0, 1);

gfx::Renderer::Renderer()
{
    ShaderSources::MakeShader(solid_shader_, SS_SOLID_VERT, SS_SOLID_FRAG);
    ShaderSources::MakeShader(hud_shader_, SS_HUD_VERT, SS_HUD_FRAG);
    ShaderSources::MakeShader(beam_shader_, SS_BEAM_VERT, SS_BEAM_FRAG);

	SetupBeamVA();
    SetupCoronaVA();
}

void gfx::Renderer::DrawList(gfx::DrawList& list, const DrawListParams& params)
{
	++frame_;

    current_shader_ = nullptr;
    glViewport(0, 0, params.screen_width, params.screen_height);
    glClearColor(params.env.clear_color.r, params.env.clear_color.g, params.env.clear_color.b, 1.0f);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	light_grid_chunks_size_ = list.chunk_size;
	CreateLightGrid(list.lights, params);

	DrawSurfaceList(list.surfaces, params);
	DrawBeamList(list.beams, params);
    DrawCoronaList(list.coronas, params);
    DrawHudList(list.huds, params);
}

struct BeamSegment
{
	glm::vec3 p0;
	uint32_t color;
	glm::vec3 p1;
	float radius;
};

void gfx::Renderer::SetupBeamVA()
{
	beam_va_ = std::make_unique<VertexArray>(VA_POSITION, 0);
	
	static const float quad_points[] = {
		0.0f, 0.0f, 0.0f,	
		1.0f, 0.0f, 0.0f,	
		0.0f, 1.0f, 0.0f,	
		1.0f, 1.0f, 0.0f,	
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

void gfx::Renderer::SetupCoronaVA()
{
    corona_va_ = std::make_unique<VertexArray>(VA_POSITION | VA_COLOR | VA_UV, VF_CREATE_EBO | VF_DYNAMIC);
    corona_tex_ = assets::AssetManager::GetInstance().Get<Texture>("corona");
}

void gfx::Renderer::InvalidateShaders()
{
	// invalidate surface shaders
	for (auto& [flags, sshader] : surface_shaders_)
	{
		InvalidateSurfaceShader(sshader);
	}
}

gfx::SurfaceShader& gfx::Renderer::GetSurfaceShader(SurfaceRenderFlags flags)
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

void gfx::Renderer::SetupSurfaceShader(SurfaceShader& sshader, const DrawListParams& params)
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

	glUniformMatrix4fv(shader.U(SU_VIEW_PROJ), 1, GL_FALSE, &params.view_proj[0][0]);
    sshader.prev_decal = false;

	if (sshader.iflags & SIF_FOG_DATA)
	{
		glUniform3fv(shader.U(SU_CAMERA_POS), 1, &params.cam_pos[0]);
		glUniform4fv(shader.U(SU_FOG), 1, &params.env.fog[0]);
	}
	
	// setup lighting
	if (sshader.iflags & SIF_LIGHTING_DATA)
	{
		glUniform3fv(shader.U(SU_AMBIENT_LIGHT), 1, &params.env.ambient_light[0]);
		glUniform3fv(shader.U(SU_SUN_COLOR), 1, &params.env.sun_color[0]);
		glUniform3fv(shader.U(SU_SUN_DIRECTION), 1, &params.env.sun_direction[0]);
	}

	sshader.global_setup = true;
}

void gfx::Renderer::InvalidateSurfaceShader(SurfaceShader& sshader)
{
	sshader.global_setup = false;
    sshader.color = nullptr;
}

void gfx::Renderer::CreateLightGrid(std::span<DrawLightCmd> light_cmds, const DrawListParams& params)
{
	light_grid_.clear();
    light_grid_chunks_.clear();

	// calc distance
	for (auto& cmd : light_cmds)
	{
        auto d = cmd.light.position - params.cam_pos;
        cmd.dist2 = glm::dot(d, d);
	}

	// sort lights by distance so closer lights get rendered instead of random npc vehicle headlights in 2km
	std::ranges::sort(light_cmds, [](const DrawLightCmd& a, const DrawLightCmd& b) { 
		return a.dist2 < b.dist2;
	});

	for (const auto& cmd : light_cmds)
	{
        AddLightToGrid(cmd.light, light_grid_, light_grid_size_);
        AddLightToGrid(cmd.light, light_grid_chunks_, light_grid_chunks_size_);
	}
}

void gfx::Renderer::AddLightToGrid(const LightData& light, LightGrid& grid, float cell_size)
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

void gfx::Renderer::DrawSurfaceList(std::span<DrawSurfaceCmd> list, const DrawListParams& params)
{
    if (list.empty())
        return;

	// setup view/proj matrix for decals
	auto decal_proj = params.proj;
    decal_proj[2][3] -= 0.0005f;
    auto decal_view_proj = decal_proj * params.view;

	// determine render flags
	for (auto& cmd : list)
	{
		if (cmd.surface->sflags & SF_BLEND)
			cmd.rflags |= SRF_BLEND;
		else
			cmd.rflags |= SRF_FOG;

		if (cmd.surface->texture)
			cmd.rflags |= SRF_TEXTURE;

		if ((cmd.surface->mflags & MF_SKELETAL) && cmd.skinning)
			cmd.rflags |= SRF_SKELETAL;

		if ((cmd.surface->sflags & SF_DEFORM_GRID) && cmd.surface->deform_tex)
			cmd.rflags |= SRF_DEFORM;

		if ((cmd.surface->sflags & SF_UNLIT) == 0)
		{
			cmd.rflags |= SRF_LIT;

			if ((r_vertex_lighting.Get() > 0) || (cmd.surface->sflags & SF_VERTEX_LIT))
                cmd.rflags |= SRF_LIT_VERTEX;

			if (cmd.surface->sflags & SF_TRANSLUCENT)
                cmd.rflags |= SRF_TRANSLUCENT;
		}

		if (cmd.color)
		{
			if (cmd.surface->sflags & SF_OBJECT_COLOR_MULT)
				cmd.rflags |= SRF_OBJECT_COLOR;
			else if (cmd.surface->sflags & SF_OBJECT_COLOR)
				cmd.rflags |= SRF_OBJECT_COLOR | SRF_OBJECT_COLOR_BACKGROUND;
			else if (cmd.surface->sflags & SF_MULTICOLOR)
				cmd.rflags |= SRF_MULTICOLOR;
		}

		if (cmd.surface->sflags & SF_2SIDED)
			cmd.rflags |= SRF_2SIDED;

		if (cmd.surface->sflags & SF_BLEND_ADDITIVE)
			cmd.rflags |= SRF_BLEND_ADDITIVE;

		if ((cmd.surface->sflags & (SF_BLEND | SF_OBJECT_COLOR)) == 0)
			cmd.rflags |= SRF_CULL_ALPHA;

		if (cmd.surface->sflags & SF_DECAL)
            cmd.rflags |= SRF_DECAL;
	}

	// sort the list to minimize state changes
	std::ranges::sort(list, [](const DrawSurfaceCmd& a, const DrawSurfaceCmd& b) {
        const bool blend_a = a.rflags & SRF_BLEND;
        const bool blend_b = b.rflags & SRF_BLEND;
		
		if (blend_a != blend_b)
			return blend_b; // opaque first

		if (blend_a) // both blended
		{
			return a.dist > b.dist; // do not optimize blended, sort by distance instead
		}

		if (a.surface == b.surface)
			return false;

		if (auto cmp = a.rflags <=> b.rflags; cmp != 0)
			return cmp < 0;

		const auto sa = a.surface;
		const auto sb = a.surface;

		if (auto cmp = sa->texture <=> sb->texture; cmp != 0)
			return cmp < 0;

		if (auto cmp = sa->va.get() <=> sb->va.get(); cmp != 0)
			return cmp < 0;

		if (auto cmp = sa <=> sb; cmp != 0)
			return cmp < 0;

		return false;
	});
	
	glActiveTexture(GL_TEXTURE0); // for all future bindings

	// cache to eliminate fake state changes
	SurfaceShader* sshader = nullptr;
	const gfx::Texture* last_texture = nullptr;
	const gfx::VertexArray* last_vao = nullptr;
    const gfx::UniformBuffer<glm::mat4>* last_skin = nullptr;
	const DeformTexture* last_deform = nullptr;

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
	
	for (const DrawSurfaceCmd& cmd : list)
	{
		const Surface* surface = cmd.surface;
		SurfaceRenderFlags rflags_diff = last_rflags ^ cmd.rflags;

		// sync 2sided
		if (rflags_diff & SRF_2SIDED)
		{
			if (cmd.rflags & SRF_2SIDED)
				glDisable(GL_CULL_FACE);
			else
				glEnable(GL_CULL_FACE);
		}

		// setup shader
		SurfaceRenderFlags last_shader_rflags = last_rflags & SRF__SHADER;
		SurfaceRenderFlags shader_rflags = cmd.rflags & SRF__SHADER;

		if (last_shader_rflags != shader_rflags || !sshader)
		{
			sshader = &GetSurfaceShader(shader_rflags);
			SetupSurfaceShader(*sshader, params);
		}

		auto shader = sshader->shader.get();

		static const glm::mat4 identity(1.0f);
		const glm::mat4* model = &identity;

		// set model matrix
		if (cmd.matrices)
		{
			model = &cmd.matrices[0];
		}

		glUniformMatrix4fv(shader->U(SU_MODEL), 1, GL_FALSE, &(*model)[0][0]);

		// sync color
		if (sshader->iflags & SIF_OBJECT_COLOR)
		{
			if (sshader->color != cmd.color)
			{
				glUniform4fv(shader->U(SU_COLOR), 1, &(*cmd.color)[0]);
				sshader->color = cmd.color;
			}
		}
		else if (sshader->iflags & SIF_MULTICOLOR_DATA)
		{
			if (sshader->color != cmd.color && cmd.num_colors > 0)
			{
				glUniform4fv(shader->U(SU_COLOR), cmd.num_colors, &(*cmd.color)[0]);
				sshader->color = cmd.color;
			}
		}

		// sync blending
		if (rflags_diff & SRF__ANY_BLEND) // this is two flags so not strictly correct but works
		{
            if (cmd.rflags & SRF__ANY_BLEND)
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
        if ((cmd.rflags & SRF__ANY_BLEND) && (rflags_diff & SRF_BLEND_ADDITIVE))
		{
			if (cmd.rflags & SRF_BLEND_ADDITIVE)
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			else
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		// sync view_proj by SRF_DECAL
        bool is_decal = cmd.rflags & SRF_DECAL;
		if (is_decal != sshader->prev_decal)
		{
			if (is_decal)
			{
                glUniformMatrix4fv(shader->U(SU_VIEW_PROJ), 1, GL_FALSE, &decal_view_proj[0][0]);
			}
			else
			{
                glUniformMatrix4fv(shader->U(SU_VIEW_PROJ), 1, GL_FALSE, &params.view_proj[0][0]);
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
                //std::cerr << center.x << ' ' << center.y << ' ' << center.z << std::endl; 
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
		if ((sshader->iflags & SIF_COLOR_TEXTURE) && last_texture != surface->texture.get())
		{
			GLuint tex_id = surface->texture ? surface->texture->GetId() : 0;
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, tex_id);
			last_texture = surface->texture.get();
		}
		
		// bind skinning UBO
		if ((sshader->iflags & SIF_SKELETAL_DATA) && last_skin != cmd.skinning)
		{
			glBindBufferBase(GL_UNIFORM_BUFFER, 0, cmd.skinning->GetId());
            last_skin = cmd.skinning;
		}
		
		// bind deform texture
		if ((sshader->iflags & SIF_DEFORM_DATA) && surface->deform_tex.get() != last_deform)
		{
			const auto& deform_tex = *surface->deform_tex;
			GLuint tex_id = deform_tex.GetId();
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_3D, tex_id);
			last_deform = &deform_tex;

			// update deform tex info
			const auto& deform_info = deform_tex.GetInfo();
			glm::mat3 deform_info_mat;
			deform_info_mat[0] = deform_info.min;
			deform_info_mat[1] = deform_info.max;
			deform_info_mat[2] = glm::vec3(deform_info.max_offset, 0.0f, 0.0f);
			glUniformMatrix3fv(shader->U(SU_DEFORM_INFO), 1, GL_FALSE, &deform_info_mat[0][0]);
		}

		// bind VAO
		if (last_vao != surface->va.get())
		{
			glBindVertexArray(surface->va->GetVAOId());
			last_vao = surface->va.get();
		}

		size_t first_tri = surface->first + cmd.first;
		size_t num_tris = cmd.count ? cmd.count : surface->count;

		// draw
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(num_tris * 3U), GL_UNSIGNED_INT,
			(void*)(first_tri * 3U * sizeof(GLuint)));

		last_rflags = cmd.rflags;
	}
}

static float GetRandomOffset(float max_offset)
{
	return (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 2.0f * max_offset;
}

void gfx::Renderer::DrawBeamList(std::span<DrawBeamCmd> queue, const DrawListParams& params)
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

	glUniformMatrix4fv(shader->U(gfx::SU_VIEW_PROJ), 1, GL_FALSE, &params.view_proj[0][0]);
    glUniform3fv(shader->U(gfx::SU_CAMERA), 1, &params.cam_pos[0]);

	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, segments.size());

	glBindVertexArray(0);

}

void gfx::Renderer::DrawCoronaList(std::span<DrawCoronaCmd> queue, const DrawListParams& params)
{
    if (queue.empty())
        return;

	// create array
    static std::vector<CoronaVertex> vertices;
    static std::vector<uint32_t> indices;
	vertices.clear();
    indices.clear();

	//auto cam_backward = glm::transpose(glm::mat3(params.view))[2];

	auto aspect = static_cast<float>(params.screen_width) / static_cast<float>(params.screen_height);
	glm::vec2 size_scale(1.0f, aspect);

	for (const auto& corona : queue)
	{
        auto to_cam = params.cam_pos - corona.pos;
        auto dist = glm::length(to_cam);
        if (dist < 0.001f)
            continue;

        to_cam /= dist;

		glm::vec4 clip_pos = params.view_proj * glm::vec4(corona.pos + to_cam * 0.1f, 1.0f);
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

        vertices.emplace_back(CoronaVertex{ndc_pos + glm::vec3(-size_xy.x, -size_xy.y, 0.0f), color_u32, glm::vec2(0.0f, 0.0f)});
        vertices.emplace_back(CoronaVertex{ndc_pos + glm::vec3(size_xy.x, -size_xy.y, 0.0f), color_u32, glm::vec2(1.0, 0.0f)});
        vertices.emplace_back(CoronaVertex{ndc_pos + glm::vec3(size_xy.x, size_xy.y, 0.0f), color_u32, glm::vec2(1.0f, 1.0f)});
        vertices.emplace_back(CoronaVertex{ndc_pos + glm::vec3(-size_xy.x, size_xy.y, 0.0f), color_u32, glm::vec2(0.0f, 1.0f)});

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
    glBindTexture(GL_TEXTURE_2D, corona_tex_->GetId());

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, (void*)0);
}

void gfx::Renderer::DrawHudList(std::span<DrawHudCmd> queue, const DrawListParams& params)
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

	float w = static_cast<float>(params.screen_width);
	float h = static_cast<float>(params.screen_height);
	glm::vec2 screen_size_px(w, h);
    glm::vec2 ndc_scale(2.0f / screen_size_px.x, -2.0f / screen_size_px.y);
	constexpr glm::vec2 ndc_offset(-1.0f, 1.0f);

	glm::mat3 matrix(1.0f);
	matrix[0][0] = ndc_scale.x;
	matrix[1][1] = ndc_scale.y;
    matrix[2][0] = ndc_offset.x;
    matrix[2][1] = ndc_offset.y;

	glUniformMatrix3fv(shader->U(SU_MODEL), 1, GL_FALSE, &matrix[0][0]);

	const gfx::Texture* last_texture = nullptr;
	const gfx::VertexArray* last_vao = nullptr;
	const glm::mat3* last_matrix = nullptr;

	glActiveTexture(GL_TEXTURE0);

	for (const auto& cmd : queue)
	{
		if (!cmd.va || !cmd.texture)
		{
            throw std::runtime_error("invalid hud draw");
		}

		// update matrix
		if (last_matrix != cmd.matrix)
		{
			glUniformMatrix3fv(shader->U(SU_MODEL), 1, GL_FALSE, &(matrix * *cmd.matrix)[0][0]);
			last_matrix = cmd.matrix;
		}

		// bind texture
		if (last_texture != cmd.texture)
		{
			GLuint tex_id = cmd.texture ? cmd.texture->GetId() : 0;
			glBindTexture(GL_TEXTURE_2D, tex_id);
			last_texture = cmd.texture;
		}

		// bind vao
		if (last_vao != cmd.va)
		{
			glBindVertexArray(cmd.va->GetVAOId());
			last_vao = cmd.va;
		}

		size_t first = cmd.first * 3;
		size_t count = cmd.count ? cmd.count * 3 : cmd.va->GetNumIndices();

		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT, (void*)(first * sizeof(GLuint)));
	}
}

