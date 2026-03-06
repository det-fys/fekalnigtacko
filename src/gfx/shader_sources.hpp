#pragma once

#include <memory>
#include "shader.hpp"

namespace gfx
{
	enum ShaderSource
	{
		SS_MESH_VERT,
		SS_MESH_FRAG,

		SS_SKEL_MESH_VERT,
		SS_SKEL_MESH_FRAG,

		SS_DEFORM_MESH_VERT,
		SS_DEFORM_MESH_FRAG,

		SS_SOLID_VERT,
		SS_SOLID_FRAG,

		SS_HUD_VERT,
		SS_HUD_FRAG,

		SS_BEAM_VERT,
		SS_BEAM_FRAG,
	};

	class ShaderSources
	{

	public:
		// Vrati zdrojovy kod shaderu
		static const char* Get(ShaderSource ss);

		static void MakeShader(std::unique_ptr<Shader>& shader, ShaderSource ss_vert, ShaderSource ss_frag) {
			shader = std::make_unique<Shader>(
				Get(ss_vert),
				Get(ss_frag)
			);
		}
	};
}

