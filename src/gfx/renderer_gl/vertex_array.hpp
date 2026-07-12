#pragma once

#include <cstdint>
#include <cassert>
#include <memory>

#include "gl.hpp"
#include "utils/defs.hpp"
#include "buffer_object.hpp"

namespace gfx
{
	// Vycet jednotlivych atributu vrcholu
	enum VertexAttr
	{
		VA_POSITION		= 1 << 0,
		VA_NORMAL		= 1 << 1,
		VA_COLOR		= 1 << 2,
		VA_UV			= 1 << 3,
		VA_LIGHTMAP_UV	= 1 << 4,
		VA_BONE_INDICES = 1 << 5,
		VA_BONE_WEIGHTS = 1 << 6,
	};

	// Informace pro vytvoreni VertexArraye
	enum VertexArrayFlags
	{
		VF_CREATE_EBO		= 1 << 0,
		VF_DYNAMIC			= 1 << 1,
	};

	// VertexArray - trida pro praci s modely
	class VertexArray
	{
		GLuint m_vao;// , m_vbo, m_ebo;
		std::unique_ptr<BufferObject> m_vbo;
		std::unique_ptr<BufferObject> m_ebo;
		size_t m_num_indices;
		GLenum m_usage;
	
	public:
		VertexArray(int attrs, int flags);
        DELETE_COPY_MOVE(VertexArray);

		~VertexArray();

		// Nastavi data do VBO
		void SetVBOData(const void* data, size_t size);
	
		// Nastavi indexy do EBO
		void SetIndices(const GLuint* data, size_t size);

		GLuint GetVAOId() const { return m_vao; }
		GLuint GetVBOId() const { return m_vbo->GetId(); }
		GLuint GetEBOId() const {
			assert(m_ebo && "GetEBOId() ale !ebo");
			return m_ebo->GetId();
		}

		//size_t GetVBOSize() const { return m_vbo->; }
		size_t GetNumIndices() const { return m_num_indices; }
	};

}
