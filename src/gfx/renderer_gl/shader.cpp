#include "shader.hpp"

#include <stdexcept>
#include <string>
#include <iostream>

#define LOG_LENGTH 1024

// Nazvy uniformnich promennych v shaderech
static const char* const s_uni_names[] = {
	"u_model",              // SU_MODEL
	"u_view_proj",          // SU_VIEW_PROJ
    "u_tex",                // SU_TEX
	"u_color",              // SU_COLOR
    "u_flags",              // SU_FLAGS
    "u_camera",             // SU_CAMERA
    "u_deform_tex",         // SU_DEFORM_TEX
    "u_deform_info",        // SU_DEFORM_INFO
    "u_ambient_light",      // SU_AMBIENT_LIGHT
    "u_sun_color",          // SU_SUN_COLOR
    "u_sun_direction",      // SU_SUN_DIRECTION
    "u_fog",                // SU_FOG
    "u_num_lights",
    "u_light_data",
    "u_camera_pos",
};

// Vytvori shader z daneho zdroje
static GLuint CreateShader(const char* src, GLenum type) {
	GLuint id = glCreateShader(type);

    glShaderSource(id, 1, &src, NULL);
    glCompileShader(id);

    GLint is_compiled = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == GL_FALSE) {
        GLchar log[LOG_LENGTH];
        glGetShaderInfoLog(id, LOG_LENGTH, NULL, log);

        glDeleteShader(id);

        std::cerr << "CANNOT COMPILE: " << src << std::endl;

        throw std::runtime_error(std::string("Nelze zkompilovat shader: ") + log);
    }

    return id;
}

gfx::Shader::Shader(const char* vert_src, const char* frag_src) {
    m_id = glCreateProgram();
    
    if (!m_id)
        throw std::runtime_error("Nelze vytvorit program!");
    
    GLuint vert, frag;

    // Vytvoreni vertex shaderu
    vert = CreateShader(vert_src, GL_VERTEX_SHADER);

    try {
        // Vyvoreni fragment shaderu
        frag = CreateShader(frag_src, GL_FRAGMENT_SHADER);
    }
    catch (std::exception& e) {
        glDeleteShader(vert);
        throw e;
    }

    // Pripojeni shaderu k programu
    glAttachShader(m_id, vert);
    glAttachShader(m_id, frag);

    // Linknuti programu
    glLinkProgram(m_id);

    // Smazani shaderu
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint is_linked = 0;
    glGetProgramiv(m_id, GL_LINK_STATUS, &is_linked);
    if (is_linked == GL_FALSE) {
        GLchar log[LOG_LENGTH];
        glGetProgramInfoLog(m_id, LOG_LENGTH, NULL, log);

        glDeleteProgram(m_id);

        throw std::runtime_error(std::string("Nelze linknout shader: ") + log);
    }

    // Ziskani lokaci uniformnich promennych
    for (size_t i = 0; i < SU_COUNT; i++)
        m_uni[i] = glGetUniformLocation(m_id, s_uni_names[i]);

	SetupBindings();
}

gfx::Shader::~Shader() {
    // Smazani programu
    glDeleteProgram(m_id);
}

void gfx::Shader::SetupBindings()
{
    glUseProgram(m_id);
    
    glUniform1i(m_uni[SU_TEX], 0);
    glUniform1i(m_uni[SU_DEFORM_TEX], 1);

    // Bones UBO
	int ubo_index = glGetUniformBlockIndex(m_id, "Bones");
    if (ubo_index != GL_INVALID_INDEX)
    {
        glUniformBlockBinding(m_id, ubo_index, 0); // bind to binding point 0
	}

	glUseProgram(0);
}
