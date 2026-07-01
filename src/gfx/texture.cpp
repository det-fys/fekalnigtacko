#include "texture.hpp"
#include <stdexcept>

#include "utils/files.hpp"
#include "assets/cmdfile.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static void GetGLFilterModes(bool linear, bool mipmaps, GLenum& filter_min, GLenum& filter_mag) {
	if (linear)
	{
		filter_min = GL_LINEAR;
		filter_mag = GL_LINEAR;
		
		if (mipmaps)
		{
			filter_min = GL_LINEAR_MIPMAP_LINEAR;
		}
	}
	else
	{
		filter_min = GL_NEAREST;
		filter_mag = GL_NEAREST;
		
		if (mipmaps)
		{
			// Mipmaps always linear
			filter_min = GL_LINEAR_MIPMAP_LINEAR;
		}
	}
}

gfx::Texture::Texture(GLuint width, GLuint height, const void* data, GLint internalformat, GLenum format, GLenum type, bool linear, bool mipmaps) {
	glGenTextures(1, &m_id);

	if (!m_id)
		throw std::runtime_error("Nelze vytvorit texturu!");

	glBindTexture(GL_TEXTURE_2D, m_id);

	glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, type, data);

	GLenum filter_min, filter_mag;
	GetGLFilterModes(linear, mipmaps, filter_min, filter_mag);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_min);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mag);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	if (mipmaps)
	{
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);

		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
}

gfx::Texture::~Texture() {
	glDeleteTextures(1, &m_id);
}

std::shared_ptr<gfx::Texture> gfx::Texture::Load(const std::string& name)
{
    return LoadFromFile("data/" + name + ".png");
}

std::shared_ptr<gfx::Texture> gfx::Texture::LoadFromFile(const std::string& filename)
{
	printf("Loading texture from file: %s\n", filename.c_str());

	std::string content = fs::ReadFileAsString(filename);

	int width, height, channels;
	unsigned char* data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(content.data()), content.size(), &width, &height, &channels, 4);

	if (!data) {
		throw std::runtime_error("Failed to load texture from file: " + filename);
	}

	bool mipmaps = true;
	bool linear = false;

	std::string config_path = filename + ".cfg";
	if (fs::FileExists(config_path))
	{
		assets::LoadCMDFile(config_path, [&](const std::string& command, CmdLineStream& iss) {
			if (command == "nomipmaps")
			{
				mipmaps = false;
			}
			else if (command == "linear")
			{
				linear = true;
			}
		});
	}

	std::shared_ptr<Texture> texture;
	try {
		texture = std::make_shared<Texture>(width, height, data, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, linear, mipmaps);
	}
	catch (const std::exception& e) {
		stbi_image_free(data);
		throw; // Rethrow the exception after freeing the data
	}

	stbi_image_free(data);
	return texture;
}
