
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */



#ifndef GrGLInterface_DEFINED
#define GrGLInterface_DEFINED

#include "GrGLConfig.h"
#include "GrRefCnt.h"

#if !defined(GR_GL_FUNCTION_TYPE)
    #define GR_GL_FUNCTION_TYPE
#endif

////////////////////////////////////////////////////////////////////////////////

/**
 * Helpers for glGetString()
 */

typedef uint32_t GrGLVersion;
typedef uint32_t GrGLSLVersion;

#define GR_GL_VER(major, minor) ((static_cast<int>(major) << 16) | \
                                 static_cast<int>(minor))
#define GR_GLSL_VER(major, minor) ((static_cast<int>(major) << 16) | \
                                   static_cast<int>(minor))

// these variants assume caller already has a string from glGetString()
GrGLVersion GrGLGetVersionFromString(const char* versionString);
GrGLSLVersion GrGLGetGLSLVersionFromString(const char* versionString);
bool GrGLHasExtensionFromString(const char* ext, const char* extensionString);

// these variants call glGetString()
bool GrGLHasExtension(const GrGLInterface*, const char* ext);
GrGLVersion GrGLGetVersion(const GrGLInterface*);
GrGLSLVersion GrGLGetGLSLVersion(const GrGLInterface*);

////////////////////////////////////////////////////////////////////////////////

/**
 * Rather than depend on platform-specific GL headers and libraries, we require
 * the client to provide a struct of GL function pointers. This struct can be
 * specified per-GrContext as a parameter to GrContext::Create. If NULL is
 * passed to Create then the "default" GL interface is used. If the default is
 * also NULL GrContext creation will fail.
 *
 * The default interface is returned by GrGLDefaultInterface. This function's
 * implementation is platform-specifc. Several have been provided, along with an
 * implementation that simply returns NULL. It is implementation-specific
 * whether the same GrGLInterface is returned or whether a new one is created
 * at each call. Some platforms may not be able to use a single GrGLInterface
 * because extension function ptrs vary across contexts. Note that GrGLInterface
 * is ref-counted. So if the same object is returned by multiple calls to 
 * GrGLDefaultInterface, each should bump the ref count.
 *
 * By defining GR_GL_PER_GL_CALL_IFACE_CALLBACK to 1 the client can specify a
 * callback function that will be called prior to each GL function call. See
 * comments in GrGLConfig.h
 */

struct GrGLInterface;

const GrGLInterface* GrGLDefaultInterface();

/**
 * Creates a GrGLInterface for a "native" GL context (e.g. WGL on windows,
 * GLX on linux, AGL on Mac). On platforms that have context-specific function
 * pointers for GL extensions (e.g. windows) the returned interface is only 
 * valid for the context that was current at creation.
 */
const GrGLInterface* GrGLCreateNativeInterface();

/**
 * Creates a GrGLInterface for an OSMesa context.
 */
const GrGLInterface* GrGLCreateMesaInterface();

/**
 * Creates a null GrGLInterface that doesn't draw anything. Used for measuring
 * CPU overhead.
 */
const GrGLInterface* GrGLCreateNullInterface();

typedef unsigned int GrGLenum;
typedef unsigned char GrGLboolean;
typedef unsigned int GrGLbitfield;
typedef signed char GrGLbyte;
typedef char GrGLchar;
typedef short GrGLshort;
typedef int GrGLint;
typedef int GrGLsizei;
typedef int64_t GrGLint64;
typedef unsigned char GrGLubyte;
typedef unsigned short GrGLushort;
typedef unsigned int GrGLuint;
typedef uint64_t GrGLuint64;
typedef float GrGLfloat;
typedef float GrGLclampf;
typedef double GrGLdouble;
typedef double GrGLclampd;
typedef void GrGLvoid;
typedef long GrGLintptr;
typedef long GrGLsizeiptr;

enum GrGLBinding {
    kDesktop_GrGLBinding = 0x01,
    kES2_GrGLBinding = 0x02
};

extern "C" {
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLActiveTextureProc)(GrGLenum texture);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLAttachShaderProc)(GrGLuint program, GrGLuint shader);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBeginQueryProc)(GrGLenum target, GrGLuint id);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBindAttribLocationProc)(GrGLuint program, GrGLuint index, const char* name);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBindBufferProc)(GrGLenum target, GrGLuint buffer);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBindTextureProc)(GrGLenum target, GrGLuint texture);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBlendColorProc)(GrGLclampf red, GrGLclampf green, GrGLclampf blue, GrGLclampf alpha);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBindFragDataLocationProc)(GrGLuint program, GrGLuint colorNumber, const GrGLchar* name);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBlendFuncProc)(GrGLenum sfactor, GrGLenum dfactor);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBufferDataProc)(GrGLenum target, GrGLsizeiptr size, const GrGLvoid* data, GrGLenum usage);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBufferSubDataProc)(GrGLenum target, GrGLintptr offset, GrGLsizeiptr size, const GrGLvoid* data);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLClearProc)(GrGLbitfield mask);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLClearColorProc)(GrGLclampf red, GrGLclampf green, GrGLclampf blue, GrGLclampf alpha);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLClearStencilProc)(GrGLint s);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLColorMaskProc)(GrGLboolean red, GrGLboolean green, GrGLboolean blue, GrGLboolean alpha);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLColorPointerProc)(GrGLint size, GrGLenum type, GrGLsizei stride, const GrGLvoid* pointer);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLCompileShaderProc)(GrGLuint shader);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLCompressedTexImage2DProc)(GrGLenum target, GrGLint level, GrGLenum internalformat, GrGLsizei width, GrGLsizei height, GrGLint border, GrGLsizei imageSize, const GrGLvoid* data);
    typedef GrGLuint (GR_GL_FUNCTION_TYPE *GrGLCreateProgramProc)(void);
    typedef GrGLuint (GR_GL_FUNCTION_TYPE *GrGLCreateShaderProc)(GrGLenum type);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLCullFaceProc)(GrGLenum mode);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDeleteBuffersProc)(GrGLsizei n, const GrGLuint* buffers);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDeleteProgramProc)(GrGLuint program);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDeleteQueriesProc)(GrGLsizei n, const GrGLuint *ids);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDeleteShaderProc)(GrGLuint shader);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDeleteTexturesProc)(GrGLsizei n, const GrGLuint* textures);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDepthMaskProc)(GrGLboolean flag);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDisableProc)(GrGLenum cap);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDisableVertexAttribArrayProc)(GrGLuint index);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDrawArraysProc)(GrGLenum mode, GrGLint first, GrGLsizei count);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDrawBufferProc)(GrGLenum mode);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDrawBuffersProc)(GrGLsizei n, const GrGLenum* bufs);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDrawElementsProc)(GrGLenum mode, GrGLsizei count, GrGLenum type, const GrGLvoid* indices);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLEnableProc)(GrGLenum cap);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLEnableVertexAttribArrayProc)(GrGLuint index);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLEndQueryProc)(GrGLenum target);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLFinishProc)();
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLFlushProc)();
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLFrontFaceProc)(GrGLenum mode);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGenBuffersProc)(GrGLsizei n, GrGLuint* buffers);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGenQueriesProc)(GrGLsizei n, GrGLuint *ids);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGenTexturesProc)(GrGLsizei n, GrGLuint* textures);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetBufferParameterivProc)(GrGLenum target, GrGLenum pname, GrGLint* params);
    typedef GrGLenum (GR_GL_FUNCTION_TYPE *GrGLGetErrorProc)();
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetIntegervProc)(GrGLenum pname, GrGLint* params);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetProgramInfoLogProc)(GrGLuint program, GrGLsizei bufsize, GrGLsizei* length, char* infolog);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetProgramivProc)(GrGLuint program, GrGLenum pname, GrGLint* params);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetQueryivProc)(GrGLenum GLtarget, GrGLenum pname, GrGLint *params);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetQueryObjecti64vProc)(GrGLuint id, GrGLenum pname, GrGLint64 *params);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetQueryObjectivProc)(GrGLuint id, GrGLenum pname, GrGLint *params);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetQueryObjectui64vProc)(GrGLuint id, GrGLenum pname, GrGLuint64 *params);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetQueryObjectuivProc)(GrGLuint id, GrGLenum pname, GrGLuint *params);    
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetShaderInfoLogProc)(GrGLuint shader, GrGLsizei bufsize, GrGLsizei* length, char* infolog);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetShaderivProc)(GrGLuint shader, GrGLenum pname, GrGLint* params);
    typedef const GrGLubyte* (GR_GL_FUNCTION_TYPE *GrGLGetStringProc)(GrGLenum name);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetTexLevelParameterivProc)(GrGLenum target, GrGLint level, GrGLenum pname, GrGLint* params);
    typedef GrGLint (GR_GL_FUNCTION_TYPE *GrGLGetUniformLocationProc)(GrGLuint program, const char* name);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLLineWidthProc)(GrGLfloat width);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLLinkProgramProc)(GrGLuint program);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLPixelStoreiProc)(GrGLenum pname, GrGLint param);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLQueryCounterProc)(GrGLuint id, GrGLenum target);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLReadBufferProc)(GrGLenum src);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLReadPixelsProc)(GrGLint x, GrGLint y, GrGLsizei width, GrGLsizei height, GrGLenum format, GrGLenum type, GrGLvoid* pixels);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLScissorProc)(GrGLint x, GrGLint y, GrGLsizei width, GrGLsizei height);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLShaderSourceProc)(GrGLuint shader, GrGLsizei count, const char** str, const GrGLint* length);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLStencilFuncProc)(GrGLenum func, GrGLint ref, GrGLuint mask);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLStencilFuncSeparateProc)(GrGLenum face, GrGLenum func, GrGLint ref, GrGLuint mask);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLStencilMaskProc)(GrGLuint mask);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLStencilMaskSeparateProc)(GrGLenum face, GrGLuint mask);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLStencilOpProc)(GrGLenum fail, GrGLenum zfail, GrGLenum zpass);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLStencilOpSeparateProc)(GrGLenum face, GrGLenum fail, GrGLenum zfail, GrGLenum zpass);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLTexImage2DProc)(GrGLenum target, GrGLint level, GrGLint internalformat, GrGLsizei width, GrGLsizei height, GrGLint border, GrGLenum format, GrGLenum type, const GrGLvoid* pixels);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLTexParameteriProc)(GrGLenum target, GrGLenum pname, GrGLint param);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLTexStorage2DProc)(GrGLenum target, GrGLsizei levels, GrGLenum internalformat, GrGLsizei width, GrGLsizei height);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLTexSubImage2DProc)(GrGLenum target, GrGLint level, GrGLint xoffset, GrGLint yoffset, GrGLsizei width, GrGLsizei height, GrGLenum format, GrGLenum type, const GrGLvoid* pixels);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform1fProc)(GrGLint location, GrGLfloat v0);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform1iProc)(GrGLint location, GrGLint v0);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform1fvProc)(GrGLint location, GrGLsizei count, const GrGLfloat* v);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform1ivProc)(GrGLint location, GrGLsizei count, const GrGLint* v);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform2fProc)(GrGLint location, GrGLfloat v0, GrGLfloat v1);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform2iProc)(GrGLint location, GrGLint v0, GrGLint v1);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform2fvProc)(GrGLint location, GrGLsizei count, const GrGLfloat* v);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform2ivProc)(GrGLint location, GrGLsizei count, const GrGLint* v);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform3fProc)(GrGLint location, GrGLfloat v0, GrGLfloat v1, GrGLfloat v2);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform3iProc)(GrGLint location, GrGLint v0, GrGLint v1, GrGLint v2);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform3fvProc)(GrGLint location, GrGLsizei count, const GrGLfloat* v);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform3ivProc)(GrGLint location, GrGLsizei count, const GrGLint* v);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform4fProc)(GrGLint location, GrGLfloat v0, GrGLfloat v1, GrGLfloat v2, GrGLfloat v3);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform4iProc)(GrGLint location, GrGLint v0, GrGLint v1, GrGLint v2, GrGLint v3);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform4fvProc)(GrGLint location, GrGLsizei count, const GrGLfloat* v);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniform4ivProc)(GrGLint location, GrGLsizei count, const GrGLint* v);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniformMatrix2fvProc)(GrGLint location, GrGLsizei count, GrGLboolean transpose, const GrGLfloat* value);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniformMatrix3fvProc)(GrGLint location, GrGLsizei count, GrGLboolean transpose, const GrGLfloat* value);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUniformMatrix4fvProc)(GrGLint location, GrGLsizei count, GrGLboolean transpose, const GrGLfloat* value);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLUseProgramProc)(GrGLuint program);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLVertexAttrib4fvProc)(GrGLuint indx, const GrGLfloat* values);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLVertexAttribPointerProc)(GrGLuint indx, GrGLint size, GrGLenum type, GrGLboolean normalized, GrGLsizei stride, const GrGLvoid* ptr);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLViewportProc)(GrGLint x, GrGLint y, GrGLsizei width, GrGLsizei height);

    // FBO Extension Functions
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBindFramebufferProc)(GrGLenum target, GrGLuint framebuffer);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBindRenderbufferProc)(GrGLenum target, GrGLuint renderbuffer);
    typedef GrGLenum (GR_GL_FUNCTION_TYPE *GrGLCheckFramebufferStatusProc)(GrGLenum target);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDeleteFramebuffersProc)(GrGLsizei n, const GrGLuint *framebuffers);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLDeleteRenderbuffersProc)(GrGLsizei n, const GrGLuint *renderbuffers);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLFramebufferRenderbufferProc)(GrGLenum target, GrGLenum attachment, GrGLenum renderbuffertarget, GrGLuint renderbuffer);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLFramebufferTexture2DProc)(GrGLenum target, GrGLenum attachment, GrGLenum textarget, GrGLuint texture, GrGLint level);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGenFramebuffersProc)(GrGLsizei n, GrGLuint *framebuffers);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGenRenderbuffersProc)(GrGLsizei n, GrGLuint *renderbuffers);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetFramebufferAttachmentParameterivProc)(GrGLenum target, GrGLenum attachment, GrGLenum pname, GrGLint* params);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLGetRenderbufferParameterivProc)(GrGLenum target, GrGLenum pname, GrGLint* params);
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLRenderbufferStorageProc)(GrGLenum target, GrGLenum internalformat, GrGLsizei width, GrGLsizei height);

    // Multisampling Extension Functions
    // same prototype for ARB_FBO, EXT_FBO, GL 3.0, & Apple ES extension
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLRenderbufferStorageMultisampleProc)(GrGLenum target, GrGLsizei samples, GrGLenum internalformat, GrGLsizei width, GrGLsizei height);
    // desktop: ext_fbo_blit, arb_fbo, gl 3.0
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBlitFramebufferProc)(GrGLint srcX0, GrGLint srcY0, GrGLint srcX1, GrGLint srcY1, GrGLint dstX0, GrGLint dstY0, GrGLint dstX1, GrGLint dstY1, GrGLbitfield mask, GrGLenum filter);
    // apple's es extension
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLResolveMultisampleFramebufferProc)();

    // Buffer mapping (extension in ES).
    typedef GrGLvoid* (GR_GL_FUNCTION_TYPE *GrGLMapBufferProc)(GrGLenum target, GrGLenum access);
    typedef GrGLboolean (GR_GL_FUNCTION_TYPE *GrGLUnmapBufferProc)(GrGLenum target);

    // Dual source blending
    typedef GrGLvoid (GR_GL_FUNCTION_TYPE *GrGLBindFragDataLocationIndexedProc)(GrGLuint program, GrGLuint colorNumber, GrGLuint index, const GrGLchar * name);
}  // extern "C"

#if GR_GL_PER_GL_FUNC_CALLBACK
typedef void (*GrGLInterfaceCallbackProc)(const GrGLInterface*);
typedef intptr_t GrGLInterfaceCallbackData;
#endif


enum GrGLCapability {
    kProbe_GrGLCapability = -1
};

/*
 * The following interface exports the OpenGL entry points used by the system.
 * Use of OpenGL calls is disallowed.  All calls should be invoked through
 * the global instance of this struct, defined above.
 *
 * IMPORTANT NOTE: The OpenGL entry points exposed here include both core GL
 * functions, and extensions.  The system assumes that the address of the
 * extension pointer will be valid across contexts.
 */
struct GR_API GrGLInterface : public GrRefCnt {

    GrGLInterface();

    bool validate() const;
    bool supportsDesktop() const {
        return 0 != (kDesktop_GrGLBinding & fBindingsExported);
    }
    bool supportsES2() const {
        return 0 !=  (kES2_GrGLBinding & fBindingsExported);
    }

    // Indicator variable specifying the type of GL implementation
    // exported:  GLES{1|2} or Desktop.
    GrGLBinding fBindingsExported;

    GrGLActiveTextureProc fActiveTexture;
    GrGLAttachShaderProc fAttachShader;
    GrGLBeginQueryProc fBeginQuery;
    GrGLBindAttribLocationProc fBindAttribLocation;
    GrGLBindBufferProc fBindBuffer;
    GrGLBindFragDataLocationProc fBindFragDataLocation;
    GrGLBindTextureProc fBindTexture;
    GrGLBlendColorProc fBlendColor;
    GrGLBlendFuncProc fBlendFunc;
    GrGLBufferDataProc fBufferData;
    GrGLBufferSubDataProc fBufferSubData;
    GrGLClearProc fClear;
    GrGLClearColorProc fClearColor;
    GrGLClearStencilProc fClearStencil;
    GrGLColorMaskProc fColorMask;
    GrGLColorPointerProc fColorPointer;
    GrGLCompileShaderProc fCompileShader;
    GrGLCompressedTexImage2DProc fCompressedTexImage2D;
    GrGLCreateProgramProc fCreateProgram;
    GrGLCreateShaderProc fCreateShader;
    GrGLCullFaceProc fCullFace;
    GrGLDeleteBuffersProc fDeleteBuffers;
    GrGLDeleteProgramProc fDeleteProgram;
    GrGLDeleteQueriesProc fDeleteQueries;
    GrGLDeleteShaderProc fDeleteShader;
    GrGLDeleteTexturesProc fDeleteTextures;
    GrGLDepthMaskProc fDepthMask;
    GrGLDisableProc fDisable;
    GrGLDisableVertexAttribArrayProc fDisableVertexAttribArray;
    GrGLDrawArraysProc fDrawArrays;
    GrGLDrawBufferProc fDrawBuffer;
    GrGLDrawBuffersProc fDrawBuffers;
    GrGLDrawElementsProc fDrawElements;
    GrGLEnableProc fEnable;
    GrGLEnableVertexAttribArrayProc fEnableVertexAttribArray;
    GrGLEndQueryProc fEndQuery;
    GrGLFinishProc fFinish;
    GrGLFlushProc fFlush;
    GrGLFrontFaceProc fFrontFace;
    GrGLGenBuffersProc fGenBuffers;
    GrGLGenQueriesProc fGenQueries;
    GrGLGenTexturesProc fGenTextures;
    GrGLGetBufferParameterivProc fGetBufferParameteriv;
    GrGLGetErrorProc fGetError;
    GrGLGetIntegervProc fGetIntegerv;
    GrGLGetQueryObjecti64vProc fGetQueryObjecti64v;
    GrGLGetQueryObjectivProc fGetQueryObjectiv;
    GrGLGetQueryObjectui64vProc fGetQueryObjectui64v;
    GrGLGetQueryObjectuivProc fGetQueryObjectuiv;
    GrGLGetQueryivProc fGetQueryiv;
    GrGLGetProgramInfoLogProc fGetProgramInfoLog;
    GrGLGetProgramivProc fGetProgramiv;
    GrGLGetShaderInfoLogProc fGetShaderInfoLog;
    GrGLGetShaderivProc fGetShaderiv;
    GrGLGetStringProc fGetString;
    GrGLGetTexLevelParameterivProc fGetTexLevelParameteriv;
    GrGLGetUniformLocationProc fGetUniformLocation;
    GrGLLineWidthProc fLineWidth;
    GrGLLinkProgramProc fLinkProgram;
    GrGLPixelStoreiProc fPixelStorei;
    GrGLQueryCounterProc fQueryCounter;
    GrGLReadBufferProc fReadBuffer;
    GrGLReadPixelsProc fReadPixels;
    GrGLScissorProc fScissor;
    GrGLShaderSourceProc fShaderSource;
    GrGLStencilFuncProc fStencilFunc;
    GrGLStencilFuncSeparateProc fStencilFuncSeparate;
    GrGLStencilMaskProc fStencilMask;
    GrGLStencilMaskSeparateProc fStencilMaskSeparate;
    GrGLStencilOpProc fStencilOp;
    GrGLStencilOpSeparateProc fStencilOpSeparate;
    GrGLTexImage2DProc fTexImage2D;
    GrGLTexParameteriProc fTexParameteri;
    GrGLTexSubImage2DProc fTexSubImage2D;
    GrGLTexStorage2DProc fTexStorage2D;
    GrGLUniform1fProc fUniform1f;
    GrGLUniform1iProc fUniform1i;
    GrGLUniform1fvProc fUniform1fv;
    GrGLUniform1ivProc fUniform1iv;
    GrGLUniform2fProc fUniform2f;
    GrGLUniform2iProc fUniform2i;
    GrGLUniform2fvProc  fUniform2fv;
    GrGLUniform2ivProc fUniform2iv;
    GrGLUniform3fProc fUniform3f;
    GrGLUniform3iProc fUniform3i;
    GrGLUniform3fvProc fUniform3fv;
    GrGLUniform3ivProc fUniform3iv;
    GrGLUniform4fProc fUniform4f;
    GrGLUniform4iProc fUniform4i;
    GrGLUniform4fvProc fUniform4fv;
    GrGLUniform4ivProc fUniform4iv;
    GrGLUniformMatrix2fvProc fUniformMatrix2fv;
    GrGLUniformMatrix3fvProc fUniformMatrix3fv;
    GrGLUniformMatrix4fvProc fUniformMatrix4fv;
    GrGLUseProgramProc fUseProgram;
    GrGLVertexAttrib4fvProc fVertexAttrib4fv;
    GrGLVertexAttribPointerProc fVertexAttribPointer;
    GrGLViewportProc fViewport;

    // FBO Extension Functions
    GrGLBindFramebufferProc fBindFramebuffer;
    GrGLBindRenderbufferProc fBindRenderbuffer;
    GrGLCheckFramebufferStatusProc fCheckFramebufferStatus;
    GrGLDeleteFramebuffersProc fDeleteFramebuffers;
    GrGLDeleteRenderbuffersProc fDeleteRenderbuffers;
    GrGLFramebufferRenderbufferProc fFramebufferRenderbuffer;
    GrGLFramebufferTexture2DProc fFramebufferTexture2D;
    GrGLGenFramebuffersProc fGenFramebuffers;
    GrGLGenRenderbuffersProc fGenRenderbuffers;
    GrGLGetFramebufferAttachmentParameterivProc fGetFramebufferAttachmentParameteriv;
    GrGLGetRenderbufferParameterivProc fGetRenderbufferParameteriv;
    GrGLRenderbufferStorageProc fRenderbufferStorage;

    // Multisampling Extension Functions
    // same prototype for ARB_FBO, EXT_FBO, GL 3.0, & Apple ES extension
    GrGLRenderbufferStorageMultisampleProc fRenderbufferStorageMultisample;
    // desktop: ext_fbo_blit, arb_fbo, gl 3.0
    GrGLBlitFramebufferProc fBlitFramebuffer;
    // apple's es extension
    GrGLResolveMultisampleFramebufferProc fResolveMultisampleFramebuffer;

    // Buffer mapping (extension in ES).
    GrGLMapBufferProc fMapBuffer;
    GrGLUnmapBufferProc fUnmapBuffer;

    // Dual Source Blending
    GrGLBindFragDataLocationIndexedProc fBindFragDataLocationIndexed;

    // Per-GL func callback
#if GR_GL_PER_GL_FUNC_CALLBACK
    GrGLInterfaceCallbackProc fCallback;
    GrGLInterfaceCallbackData fCallbackData;
#endif

};

#endif
