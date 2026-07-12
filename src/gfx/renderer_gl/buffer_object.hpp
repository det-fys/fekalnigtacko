#pragma once
#include "gl.hpp"

#include "utils/defs.hpp"

namespace gfx
{
/**
* \brief Wrapper pro OpenGL buffer object
*/
class BufferObject
{
    GLuint m_id;
    GLenum m_target;
    GLenum m_usage;
    size_t m_size;

public:
    BufferObject(GLenum target, GLenum usage);
    DELETE_COPY_MOVE(BufferObject);

    ~BufferObject();

    void Bind() const;
    void Unbind() const;

    void SetData(const void* data, size_t size);

    GLuint GetId() const { return m_id; }

};

}
