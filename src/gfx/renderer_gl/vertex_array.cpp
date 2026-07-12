#include "vertex_array.hpp"

#include <cstdint>

// Struktura pro informace o jednotlivych vertex atributu
struct VertexAttribInfo {
    GLuint index;
    GLint size;
    GLenum type;
    bool integer;
    GLboolean normalized;
    size_t byte_size;
};

// Informace o jednotlivych vertex atributech
static const VertexAttribInfo s_ATTR_INFO[] = {
    { 0, 3, GL_FLOAT,           false,  GL_FALSE,   sizeof(float) * 3   }, // VA_POSITION
    { 1, 3, GL_FLOAT,           false,  GL_FALSE,   sizeof(float) * 3   }, // VA_NORMAL
    { 2, 4, GL_UNSIGNED_BYTE,   false,  GL_TRUE,    4                   }, // VA_COLOR
    { 3, 2, GL_FLOAT,           false,  GL_FALSE,   sizeof(float) * 2   }, // VA_UV
	{ 4, 2, GL_FLOAT,           false,  GL_FALSE,   sizeof(float) * 2   }, // VA_LIGHTMAP_UV
    { 5, 4, GL_UNSIGNED_BYTE,   true,   GL_FALSE,   4                   }, // VA_BONE_INDICES
	{ 6, 4, GL_FLOAT,           false,  GL_FALSE,   sizeof(float) * 4   }, // VA_BONE_WEIGHTS
};

// Pocet typu vertex atributu
static const size_t s_ATTR_COUNT = 7;

gfx::VertexArray::VertexArray(int attrs, int flags) : m_usage(GL_STATIC_DRAW), m_num_indices(0) {
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    if (flags & VF_DYNAMIC)
        m_usage = GL_DYNAMIC_DRAW;

    // vytvori buffer pro vrcholy
    m_vbo = std::make_unique<BufferObject>(GL_ARRAY_BUFFER, m_usage);
    m_vbo->Bind();

    // vypocet velikosti jednoho vrcholu
    size_t stride = 0U;
    for (size_t i = 0; i < s_ATTR_COUNT; i++) {
        if (attrs & (1 << i))
            stride += s_ATTR_INFO[i].byte_size;
    }

    // povoli a nastavi jednotlive vertex atributy
    size_t offset = 0U;
    for (size_t i = 0; i < s_ATTR_COUNT; i++) {
        const auto& info = s_ATTR_INFO[i];
        if (attrs & (1 << i)) {
            glEnableVertexAttribArray(info.index);

            if (!info.integer)
            {
                glVertexAttribPointer(info.index, info.size, info.type, info.normalized, stride, (const void*)offset);
            }
            else
            {
			    glVertexAttribIPointer(info.index, info.size, info.type, stride, (const void*)offset);
            }

            offset += info.byte_size;
        } else if (i == 2)
        {
            // pokud neni barva, nastavime defaultni hodnotu 1.0f,1.0f,1.0f,1.0f
            glDisableVertexAttribArray(info.index);
            glVertexAttrib4f(info.index, 1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    // vytvori buffer pro indexy
    if (flags & VF_CREATE_EBO) {
        m_ebo = std::make_unique<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, m_usage);
        m_ebo->Bind();
    }

    glBindVertexArray(0);
}

gfx::VertexArray::~VertexArray() {
    // uklid
    glDeleteVertexArrays(1, &m_vao);
}

// Nastavi data do VBO
void gfx::VertexArray::SetVBOData(const void* data, size_t size) {
    glBindVertexArray(m_vao);
    m_vbo->SetData(data, size);
    glBindVertexArray(0);
}

// Nastavi indexy do EBO
void gfx::VertexArray::SetIndices(const GLuint* data, size_t size) {
    glBindVertexArray(m_vao);
    m_ebo->SetData(data, size * sizeof(*data));
    m_num_indices = size;
    glBindVertexArray(0);
}
