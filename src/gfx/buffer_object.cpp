#include <stdexcept>
#include "buffer_object.hpp"

gfx::BufferObject::BufferObject(GLenum target, GLenum usage) : m_target(target), m_usage(usage), m_size(0U) {
    glGenBuffers(1, &m_id);
}

gfx::BufferObject::~BufferObject() {
    glDeleteBuffers(1, &m_id);
}

void gfx::BufferObject::Bind() const {
    glBindBuffer(m_target, m_id);
}

void gfx::BufferObject::Unbind() const {
    glBindBuffer(m_target, 0);
}

void gfx::BufferObject::SetData(const void* data, size_t size) {
    Bind();

    if (size > m_size)
        glBufferData(m_target, size, data, m_usage);
    else
        glBufferSubData(m_target, 0, size, data);

    m_size = size;
}
