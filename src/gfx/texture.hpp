#pragma once
#include <memory>
#include <string>
#include "client/utils.hpp"
#include "client/gl.hpp"
#include "assets/asset_manager.hpp"

namespace gfx
{
/**
 * \brief Wrapper pro OpenGL texturu
 */
class Texture : public assets::Asset
{
	GLuint m_id;

public:
    Texture();
	Texture(GLuint width, GLuint height, const void* data, GLint internalformat, GLenum format, GLenum type, bool linear, bool mipmaps);
	~Texture();

	void SetData(GLuint width, GLuint height, const void* data, GLint internalformat, GLenum format, GLenum type,
                 bool linear, bool mipmaps);

	GLuint GetId() const { return m_id; }

	static std::shared_ptr<Texture> Load(const std::string& name);
	static std::shared_ptr<Texture> LoadFromFile(const std::string& filename);

};

}