#include "renderer.hpp"

#include <algorithm>
#include <ranges>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "client/gl.hpp"

#include "shader_sources.hpp"
#include "shader_defs.hpp"

gfx::Renderer::Renderer()
{
    ShaderSources::MakeShader(mesh_shader_.shader, SS_MESH_VERT, SS_MESH_FRAG);
    ShaderSources::MakeShader(skel_mesh_shader_.shader, SS_SKEL_MESH_VERT, SS_SKEL_MESH_FRAG);
    ShaderSources::MakeShader(deform_mesh_shader_.shader, SS_DEFORM_MESH_VERT, SS_DEFORM_MESH_FRAG);
    ShaderSources::MakeShader(solid_shader_, SS_SOLID_VERT, SS_SOLID_FRAG);
    ShaderSources::MakeShader(hud_shader_, SS_HUD_VERT, SS_HUD_FRAG);
    ShaderSources::MakeShader(beam_shader_, SS_BEAM_VERT, SS_BEAM_FRAG);

	SetupBeamVA();
}

void gfx::Renderer::Begin(size_t width, size_t height)
{
	current_shader_ = nullptr;
    glViewport(0, 0, width, height);

	//glEnable(GL_MULTISAMPLE);
}

void gfx::Renderer::ClearColor(const glm::vec3& color)
{
    glClearColor(color.r, color.g, color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void gfx::Renderer::ClearDepth()
{
    glClear(GL_DEPTH_BUFFER_BIT);
}

void gfx::Renderer::DrawList(gfx::DrawList& list, const DrawListParams& params)
{
	DrawSurfaceList(list.surfaces, params);
	DrawBeamList(list.beams, params);
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

void gfx::Renderer::InvalidateShaders()
{
	InvalidateMeshShader(mesh_shader_);
	InvalidateMeshShader(skel_mesh_shader_);
	InvalidateMeshShader(deform_mesh_shader_);
}

void gfx::Renderer::InvalidateMeshShader(MeshShader& mshader)
{
	mshader.global_setup = false;
	mshader.color = glm::vec4(-1.0f); // invalidate color
}

void gfx::Renderer::SetupMeshShader(MeshShader& mshader, const DrawListParams& params)
{
	const Shader& shader = *mshader.shader;

	if (current_shader_ != &shader)
	{
		glUseProgram(shader.GetId());
		current_shader_ = &shader;
	}

	if (mshader.global_setup)
	{
		return; // Global uniforms are already set up
	}

	glUniformMatrix4fv(shader.U(gfx::SU_VIEW_PROJ), 1, GL_FALSE, &params.view_proj[0][0]);

	mshader.global_setup = true;
}

void gfx::Renderer::DrawSurfaceList(std::span<DrawSurfaceCmd> list, const DrawListParams& params)
{
	// sort the list to minimize state changes
	std::ranges::sort(list, [](const DrawSurfaceCmd& a, const DrawSurfaceCmd& b) {
		const Surface* sa = a.surface;
		const Surface* sb = b.surface;

		const bool blend_a = sa->sflags & SF_BLEND;
		const bool blend_b = sb->sflags & SF_BLEND;
		
		if (blend_a != blend_b)
			return blend_b; // opaque first

		if (blend_a) // both blended
		{
			return a.dist > b.dist; // do not optimize blended, sort by distance instead
		}

		if (sa == sb)
			return false;

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
	bool last_twosided = false;

	// reset blending
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	bool last_blend = false;
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // set to opacity blending default
	bool last_blend_additive = false;
	
	for (const DrawSurfaceCmd& cmd : list)
	{
		const Surface* surface = cmd.surface;

		// mesh flags
		const bool skeletal_flag = surface->mflags & MF_SKELETAL;
		// surface flags
		const bool twosided_flag = surface->sflags & SF_2SIDED;
		const bool blend_flag = surface->sflags & SF_BLEND;
		const bool object_color_flag = surface->sflags & SF_OBJECT_COLOR;
		const bool deform_flag = surface->sflags & SF_DEFORM_GRID;

		// sync 2sided
		if (last_twosided != twosided_flag)
		{
			if (twosided_flag)
				glDisable(GL_CULL_FACE);
			else
				glEnable(GL_CULL_FACE);

			last_twosided = twosided_flag;
		}

		// select shader
		MeshShader& mshader = skeletal_flag ? skel_mesh_shader_ : (deform_flag ? deform_mesh_shader_ : mesh_shader_);
		SetupMeshShader(mshader, params);

		// set model matrix
		if (cmd.matrices)
		{
			glUniformMatrix4fv(mshader.shader->U(SU_MODEL), 1, GL_FALSE, &cmd.matrices[0][0][0]);
		}
		else
		{ // use identity if no matrix provided
			static const glm::mat4 identity(1.0f);
			glUniformMatrix4fv(mshader.shader->U(SU_MODEL), 1, GL_FALSE, &identity[0][0]);
		}

		// set color
		int shflags = SHF_CULL_ALPHA;

		glm::vec4 color = glm::vec4(1.0f);
		if (object_color_flag && cmd.color)
		{
			// use object color and disable alpha cull
			shflags &= ~SHF_CULL_ALPHA;
			shflags |= SHF_BACKGROUND;
			color = glm::vec4(*cmd.color);
		}

		// sync blending
		if (blend_flag != last_blend)
		{
			if (blend_flag)
			{
				glEnable(GL_BLEND);
				glDepthMask(GL_FALSE);
			}
			else
			{
				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
			}

			last_blend = blend_flag;
		}

		// sync blending type
		if (blend_flag)
		{
			shflags &= ~SHF_CULL_ALPHA;

			const bool blend_additive = surface->sflags & SF_BLEND_ADDITIVE;
			if (blend_additive != last_blend_additive)
			{
				if (blend_additive)
					glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				else
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			last_blend_additive = blend_additive;
		}
		
		// sync cull_alpha
		if (mshader.flags != shflags)
		{
			glUniform1i(mshader.shader->U(SU_FLAGS), shflags);
			mshader.flags = shflags;
		}

		// sync color
		if (mshader.color != color)
		{
			glUniform4fv(mshader.shader->U(SU_COLOR), 1, &color[0]);
			mshader.color = color;
		}

		// bind texture
		if (last_texture != surface->texture.get())
		{
			GLuint tex_id = surface->texture ? surface->texture->GetId() : 0;
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, tex_id);
			last_texture = surface->texture.get();
		}
		
		// bind skinning UBO
		if (cmd.skinning && last_skin != cmd.skinning)
		{
			glBindBufferBase(GL_UNIFORM_BUFFER, 0, cmd.skinning->GetId());
            last_skin = cmd.skinning;
		}
		
		// bind deform texture
		if (deform_flag && surface->deform_tex.get() != last_deform)
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
			glUniformMatrix3fv(mshader.shader->U(SU_DEFORM_INFO), 1, GL_FALSE, &deform_info_mat[0][0]);
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
	}

	// reset this as it is rare and other stuff might not reset this
	glDepthMask(GL_TRUE);

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

	glDepthMask(GL_TRUE);

}

void gfx::Renderer::DrawHudList(std::span<DrawHudCmd> queue, const DrawListParams& params)
{
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

	glActiveTexture(GL_TEXTURE0);

	for (const auto& cmd : queue)
	{
		if (!cmd.va || !cmd.texture)
		{
            throw std::runtime_error("invalid hud draw");
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
