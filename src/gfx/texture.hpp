#pragma once
#include <memory>
#include <string>
#include "client/utils.hpp"
#include "client/gl.hpp"

namespace gfx
{
/**
 * \brief Wrapper pro OpenGL texturu
 */
class Texture : public NonCopyableNonMovable
{
	GLuint m_id;

public:
	Texture(GLuint width, GLuint height, const void* data, GLint internalformat, GLenum format, GLenum type, bool linear, bool mipmaps);
	~Texture();

	GLuint GetId() const { return m_id; }

	static std::shared_ptr<Texture> LoadFromFile(const std::string& filename);

};

}