#include "renderer.hpp"

#include <algorithm>
#include <ranges>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "client/gl.hpp"

#include "gfx/shader_sources.hpp"

gfx::Renderer::Renderer()
{
    ShaderSources::MakeShader(mesh_shader_.shader, SS_MESH_VERT, SS_MESH_FRAG);
    ShaderSources::MakeShader(skel_mesh_shader_.shader, SS_SKEL_MESH_VERT, SS_SKEL_MESH_FRAG);
    ShaderSources::MakeShader(solid_shader_, SS_SOLID_VERT, SS_SOLID_FRAG);
}

void gfx::Renderer::Begin(size_t width, size_t height)
{
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

		const bool trans_a = sa->sflags & SF_TRANSPARENT;
		const bool trans_b = sb->sflags & SF_TRANSPARENT;
		
		if (trans_a != trans_b)
			return trans_b; // opaque first

		if (trans_a) // both transparent
		{
			return a.dist > b.dist; // do not optimize transparent, sort by distance instead
		}

		if (auto cmp = sa <=> sb; cmp != 0)
			return cmp < 0;

		if (auto cmp = sa->texture <=> sb->texture; cmp != 0)
			return cmp < 0;

		if (auto cmp = sa->va.get() <=> sb->va.get(); cmp != 0)
			return cmp < 0;

		return false;
	});

	glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

	InvalidateShaders();

	// cache to eliminate fake state changes
	const gfx::Texture* last_texture = nullptr;
	const gfx::VertexArray* last_vao = nullptr;
	bool last_double_sided = false;

	glActiveTexture(GL_TEXTURE0); // for all future bindings

	for (const DrawSurfaceCmd& cmd : list)
	{
		const Surface* surface = cmd.surface;

		// mesh flags
		const bool skeletal_flag = surface->mflags & MF_SKELETAL;
		// surface flags
		const bool double_sided_flag = surface->sflags & SF_DOUBLE_SIDED;
		const bool transparent_flag = surface->sflags & SF_TRANSPARENT;
		const bool object_color_flag = surface->sflags & SF_OBJECT_COLOR;

		// adjust state
		if (last_double_sided != double_sided_flag)
		{
			if (double_sided_flag)
				glDisable(GL_CULL_FACE);
			else
				glEnable(GL_CULL_FACE);

			last_double_sided = double_sided_flag;
		}

		// select shader
		MeshShader& mshader = skeletal_flag ? skel_mesh_shader_ : mesh_shader_;
		SetupMeshShader(mshader, params);

		// set model matrix
		if (cmd.matrices)
		{
			glUniformMatrix4fv(mshader.shader->U(gfx::SU_MODEL), 1, GL_FALSE, &cmd.matrices[0][0][0]);
		}
		else
		{ // use identity if no matrix provided
			static const glm::mat4 identity(1.0f);
			glUniformMatrix4fv(mshader.shader->U(gfx::SU_MODEL), 1, GL_FALSE, &identity[0][0]);
		}

		// set color
		glm::vec4 color = (object_color_flag && cmd.color) ? glm::vec4(*cmd.color) : glm::vec4(1.0f);
		if (mshader.color != color)
		{
			glUniform4fv(mshader.shader->U(gfx::SU_COLOR), 1, &color[0]);
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



}
