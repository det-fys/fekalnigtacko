#pragma once

#include <memory>
#include <span>

#include "shader.hpp"
#include "draw_list.hpp"

namespace gfx
{
	struct DrawListParams
	{
		glm::mat4 view_proj;
		size_t screen_width = 0;
		size_t screen_height = 0;
	};

	struct MeshShader
	{
		std::unique_ptr<Shader> shader;

		// cached state to avoid redundant uniform updates which are expensive especially on WebGL
		bool global_setup = false;
		glm::vec4 color = glm::vec4(-1.0f); // invalid to force initial setup
		bool cull_alpha = false;
	};

	class Renderer
	{
	public:
		Renderer();

		void Begin(size_t width, size_t height);

		void ClearColor(const glm::vec3& color);
		void ClearDepth();

		void DrawList(gfx::DrawList& list, const DrawListParams& params);

	private:
		MeshShader mesh_shader_;
		MeshShader skel_mesh_shader_;
		std::unique_ptr<Shader> solid_shader_;
		std::unique_ptr<Shader> hud_shader_;

		const Shader* current_shader_ = nullptr;

		void InvalidateShaders();
		void InvalidateMeshShader(MeshShader& mshader);
		void SetupMeshShader(MeshShader& mshader, const DrawListParams& params);

		void DrawSurfaceList(std::span<DrawSurfaceCmd> queue, const DrawListParams& params);
		void DrawHudList(std::span<DrawHudCmd> queue, const DrawListParams& params);

	};

}