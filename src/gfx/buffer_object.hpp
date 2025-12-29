#pragma once
#include "client/gl.hpp"
#include "client/utils.hpp"

namespace gfx
{
/**
* \brief Wrapper pro OpenGL buffer object
*/
class BufferObject : public NonCopyableNonMovable
{
    GLuint m_id;
    GLenum m_target;
    GLenum m_usage;
    size_t m_size;

public:
    BufferObject(GLenum target, GLenum usage);
    ~BufferObject();

    void Bind() const;
    void Unbind() const;

    void SetData(const void* data, size_t size);

    GLuint GetId() const { return m_id; }

};

}
