#pragma once

#include "buffer_object.hpp"

namespace gfx
{
	template<class T>
	class UniformBuffer : public BufferObject
	{
	public:
		UniformBuffer() : BufferObject(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW)
		{
		}

		void SetData(const T* data, size_t count = 1)
		{
			BufferObject::SetData(data, sizeof(T) * count);
		}

	};
}