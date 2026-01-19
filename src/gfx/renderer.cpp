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
    ShaderSources::MakeShader(solid_shader_, SS_SOLID_VERT, SS_SOLID_FRAG);
    ShaderSources::MakeShader(hud_shader_, SS_HUD_VERT, SS_HUD_FRAG);
}

void gfx::Renderer::Begin(size_t width, size_t height)
{
	current_shader_ = nullptr;
    glViewport(0, 0, width, height);
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
    DrawHudList(list.huds, params);
}

void gfx::Renderer::InvalidateShaders()
{
	InvalidateMeshShader(mesh_shader_);
	InvalidateMeshShader(skel_mesh_shader_);
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

		if (auto cmp = sa <=> sb; cmp != 0)
			return cmp < 0;

		if (auto cmp = sa->texture <=> sb->texture; cmp != 0)
			return cmp < 0;

		if (auto cmp = sa->va.get() <=> sb->va.get(); cmp != 0)
			return cmp < 0;

		return false;
	});
	
	glActiveTexture(GL_TEXTURE0); // for all future bindings

	// cache to eliminate fake state changes
	const gfx::Texture* last_texture = nullptr;
	const gfx::VertexArray* last_vao = nullptr;
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
		MeshShader& mshader = skeletal_flag ? skel_mesh_shader_ : mesh_shader_;
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
			glBindTexture(GL_TEXTURE_2D, tex_id);
			last_texture = surface->texture.get();
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

	const gfx::Texture* last_texture = nullptr;
	const gfx::VertexArray* last_vao = nullptr;
	glm::vec4 last_color = glm::vec4(-1.0f);

	for (const auto& cmd : queue)
	{
		if (!cmd.va || !cmd.texture || !cmd.pos)
		{
            throw std::runtime_error("invalid hud draw");
		}

		// calculate transform
		const auto& hp = *cmd.pos;
		
		glm::vec2 pos_px = hp.anchor * screen_size_px + hp.pos;
		glm::vec2 trans_ndc = ndc_offset + pos_px * ndc_scale;

		glm::mat3 matrix(1.0f);
		matrix[0][0] = hp.scale.x * ndc_scale.x;
		matrix[1][1] = hp.scale.y * ndc_scale.y;
		matrix[2][0] = trans_ndc.x;
		matrix[2][1] = trans_ndc.y;

		glUniformMatrix3fv(shader->U(SU_MODEL), 1, GL_FALSE, &matrix[0][0]);

		//sync color
		glm::vec4 color = cmd.color ? *cmd.color : glm::vec4(1.0f);
		if (last_color != color)
		{
			glUniform4fv(shader->U(SU_COLOR), 1, &color[0]);
			last_color = color;
		}

		// bind texture
		if (last_texture != cmd.texture)
		{
			GLuint tex_id = cmd.texture ? cmd.texture->GetId() : 0;
			glBindTexture(GL_TEXTURE_2D, tex_id);
			last_texture = cmd.texture;
		}

		// bind vao
		if (last_vao = cmd.va)
		{
			glBindVertexArray(cmd.va->GetVAOId());
			last_vao = cmd.va;
		}

		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(cmd.va->GetNumIndices()), GL_UNSIGNED_INT, NULL);
	}
}
