#pragma once

#include "client/gl.hpp"
#include "client/utils.hpp"

namespace gfx
{
	/**
	 * \brief Uniformy shaderu
	 */
	enum ShaderUniform
	{
		SU_MODEL,
		SU_VIEW_PROJ,
		SU_TEX,
		SU_COLOR,
		SU_FLAGS,
		SU_CAMERA,
		SU_DEFORM_TEX,
		SU_DEFORM_INFO,
		SU_AMBIENT_LIGHT,
		SU_SUN_COLOR,
		SU_SUN_DIRECTION,
		SU_FOG,
		SU_NUMLIGHTS,
		SU_LIGHT_POSITIONS,
		SU_LIGHT_COLORS_RS,
		SU_CAMERA_POS,

		SU_COUNT
	};

	/**
	 * \brief Wrapper pro OpenGL shader program
	 */
	class Shader : public NonCopyableNonMovable
	{
		GLuint m_id;
		GLint m_uni[SU_COUNT];

	public:
		Shader(const char* vert_src, const char* frag_src);
		~Shader();

		/**
		 * \brief Vr�t� lokaci uniformy
		 *
		 * \param u Uniforma
		 * \return Lokace
		 */
		GLint U(ShaderUniform u) const { return m_uni[u]; }

		/**
		 * \brief Vr�t� OpenGL n�zev shaderu
		 *
		 * \return N�zev shaderu
		 */
		GLuint GetId() const { return m_id; }

	private:
		void SetupBindings();

	};
}
