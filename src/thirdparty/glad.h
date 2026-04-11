#pragma once
#include <GLFW/glfw3.h>

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_STATIC_DRAW                    0x88E0
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_FRAMEBUFFER                    0x8D40
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_DEPTH_ATTACHMENT                0x8D00
#define GL_RENDERBUFFER                   0x8D41
#define GL_DEPTH_COMPONENT                0x1902
#define GL_LINEAR_MIPMAP_LINEAR           0x2703
#endif

// Typedefs for modern GL functions
typedef void (APIENTRY * PFNGLGENVERTEXARRAYSPROC) (GLsizei n, uint32_t *arrays);
typedef void (APIENTRY * PFNGLBINDVERTEXARRAYPROC) (uint32_t array);
typedef void (APIENTRY * PFNGLGENBUFFERSPROC) (GLsizei n, uint32_t *buffers);
typedef void (APIENTRY * PFNGLBINDBUFFERPROC) (GLenum target, uint32_t buffer);
typedef void (APIENTRY * PFNGLBUFFERDATAPROC) (GLenum target, ptrdiff_t size, const void *data, GLenum usage);
typedef void (APIENTRY * PFNGLENABLEVERTEXATTRIBARRAYPROC) (uint32_t index);
typedef void (APIENTRY * PFNGLVERTEXATTRIBPOINTERPROC) (uint32_t index, int size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef uint32_t (APIENTRY * PFNGLCREATESHADERPROC) (GLenum type);
typedef void (APIENTRY * PFNGLSHADERSOURCEPROC) (uint32_t shader, GLsizei count, const char* const* string, const int *length);
typedef void (APIENTRY * PFNGLCOMPILESHADERPROC) (uint32_t shader);
typedef void (APIENTRY * PFNGLGETSHADERIVPROC) (uint32_t shader, GLenum pname, int *params);
typedef void (APIENTRY * PFNGLGETSHADERINFOLOGPROC) (uint32_t shader, GLsizei bufSize, GLsizei *length, char *infoLog);
typedef uint32_t (APIENTRY * PFNGLCREATEPROGRAMPROC) (void);
typedef void (APIENTRY * PFNGLATTACHSHADERPROC) (uint32_t program, uint32_t shader);
typedef void (APIENTRY * PFNGLLINKPROGRAMPROC) (uint32_t program);
typedef void (APIENTRY * PFNGLUSEPROGRAMPROC) (uint32_t program);
typedef int (APIENTRY * PFNGLGETUNIFORMLOCATIONPROC) (uint32_t program, const char *name);
typedef void (APIENTRY * PFNGLUNIFORMMATRIX4FVPROC) (int location, GLsizei count, GLboolean transpose, const float *value);
typedef void (APIENTRY * PFNGLUNIFORM1IPROC) (int location, int v0);
typedef void (APIENTRY * PFNGLUNIFORM3FVPROC) (int location, GLsizei count, const float *value);
typedef void (APIENTRY * PFNGLGENFRAMEBUFFERSPROC) (GLsizei n, uint32_t *framebuffers);
typedef void (APIENTRY * PFNGLBINDFRAMEBUFFERPROC) (GLenum target, uint32_t framebuffer);
typedef void (APIENTRY * PFNGLFRAMEBUFFERTEXTURE2DPROC) (GLenum target, GLenum attachment, GLenum textarget, uint32_t texture, int level);
typedef void (APIENTRY * PFNGLGENRENDERBUFFERSPROC) (GLsizei n, uint32_t *renderbuffers);
typedef void (APIENTRY * PFNGLBINDRENDERBUFFERPROC) (GLenum target, uint32_t renderbuffer);
typedef void (APIENTRY * PFNGLRENDERBUFFERSTORAGEPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY * PFNGLFRAMEBUFFERRENDERBUFFERPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, uint32_t renderbuffer);
typedef void (APIENTRY * PFNGLGENERATEMIPMAPPROC) (GLenum target);
typedef void (APIENTRY * PFNGLDELETEVERTEXARRAYSPROC) (GLsizei n, const uint32_t *arrays);
typedef void (APIENTRY * PFNGLDELETEBUFFERSPROC) (GLsizei n, const uint32_t *buffers);
typedef void (APIENTRY * PFNGLDELETEPROGRAMPROC) (uint32_t program);
typedef void (APIENTRY * PFNGLDELETESHADERPROC) (uint32_t shader);
typedef void (APIENTRY * PFNGLDELETEFRAMEBUFFERSPROC) (GLsizei n, const uint32_t *framebuffers);
typedef void (APIENTRY * PFNGLDELETERENDERBUFFERSPROC) (GLsizei n, const uint32_t *renderbuffers);

#ifdef __cplusplus
extern "C" {
#endif
void* glP_Get(const char* name);
#ifdef __cplusplus
}
#endif

#define glGenVertexArrays    ((PFNGLGENVERTEXARRAYSPROC)    glP_Get("glGenVertexArrays"))
#define glBindVertexArray    ((PFNGLBINDVERTEXARRAYPROC)    glP_Get("glBindVertexArray"))
#define glGenBuffers         ((PFNGLGENBUFFERSPROC)         glP_Get("glGenBuffers"))
#define glBindBuffer         ((PFNGLBINDBUFFERPROC)         glP_Get("glBindBuffer"))
#define glBufferData         ((PFNGLBUFFERDATAPROC)         glP_Get("glBufferData"))
#define glEnableVertexAttribArray ((PFNGLENABLEVERTEXATTRIBARRAYPROC) glP_Get("glEnableVertexAttribArray"))
#define glVertexAttribPointer ((PFNGLVERTEXATTRIBPOINTERPROC) glP_Get("glVertexAttribPointer"))
#define glCreateShader       ((PFNGLCREATESHADERPROC)       glP_Get("glCreateShader"))
#define glShaderSource       ((PFNGLSHADERSOURCEPROC)       glP_Get("glShaderSource"))
#define glCompileShader      ((PFNGLCOMPILESHADERPROC)      glP_Get("glCompileShader"))
#define glGetShaderiv        ((PFNGLGETSHADERIVPROC)        glP_Get("glGetShaderiv"))
#define glGetShaderInfoLog   ((PFNGLGETSHADERINFOLOGPROC)   glP_Get("glGetShaderInfoLog"))
#define glCreateProgram      ((PFNGLCREATEPROGRAMPROC)      glP_Get("glCreateProgram"))
#define glAttachShader       ((PFNGLATTACHSHADERPROC)       glP_Get("glAttachShader"))
#define glLinkProgram        ((PFNGLLINKPROGRAMPROC)        glP_Get("glLinkProgram"))
#define glUseProgram         ((PFNGLUSEPROGRAMPROC)         glP_Get("glUseProgram"))
#define glGetUniformLocation ((PFNGLGETUNIFORMLOCATIONPROC) glP_Get("glGetUniformLocation"))
#define glUniformMatrix4fv   ((PFNGLUNIFORMMATRIX4FVPROC)   glP_Get("glUniformMatrix4fv"))
#define glUniform1i          ((PFNGLUNIFORM1IPROC)          glP_Get("glUniform1i"))
#define glUniform3fv         ((PFNGLUNIFORM3FVPROC)         glP_Get("glUniform3fv"))
#define glGenFramebuffers    ((PFNGLGENFRAMEBUFFERSPROC)    glP_Get("glGenFramebuffers"))
#define glBindFramebuffer    ((PFNGLBINDFRAMEBUFFERPROC)    glP_Get("glBindFramebuffer"))
#define glFramebufferTexture2D ((PFNGLFRAMEBUFFERTEXTURE2DPROC) glP_Get("glFramebufferTexture2D"))
#define glGenRenderbuffers   ((PFNGLGENRENDERBUFFERSPROC)   glP_Get("glGenRenderbuffers"))
#define glBindRenderbuffer   ((PFNGLBINDRENDERBUFFERPROC)   glP_Get("glBindRenderbuffer"))
#define glRenderbufferStorage ((PFNGLRENDERBUFFERSTORAGEPROC) glP_Get("glRenderbufferStorage"))
#define glFramebufferRenderbuffer ((PFNGLFRAMEBUFFERRENDERBUFFERPROC) glP_Get("glFramebufferRenderbuffer"))
#define glGenerateMipmap      ((PFNGLGENERATEMIPMAPPROC)      glP_Get("glGenerateMipmap"))
#define glDeleteVertexArrays  ((PFNGLDELETEVERTEXARRAYSPROC) glP_Get("glDeleteVertexArrays"))
#define glDeleteBuffers       ((PFNGLDELETEBUFFERSPROC)      glP_Get("glDeleteBuffers"))
#define glDeleteProgram       ((PFNGLDELETEPROGRAMPROC)      glP_Get("glDeleteProgram"))
#define glDeleteShader        ((PFNGLDELETESHADERPROC)       glP_Get("glDeleteShader"))
#define glDeleteFramebuffers  ((PFNGLDELETEFRAMEBUFFERSPROC) glP_Get("glDeleteFramebuffers"))
#define glDeleteRenderbuffers ((PFNGLDELETERENDERBUFFERSPROC) glP_Get("glDeleteRenderbuffers"))
