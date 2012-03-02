/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Benoit Jacob <bjacob@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GLCONTEXTSYMBOLS_H_
#define GLCONTEXTSYMBOLS_H_

/*
 * This file should only be included by GLContext.h, and should be
 * autogenerated in the future.
 */

#ifndef GLAPIENTRY
#ifdef XP_WIN
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif
#define GLAPI
#endif

namespace mozilla {
namespace gl {

struct GLContextSymbols
{
    GLContextSymbols() {
        Zero();
    }

    void Zero() {
        memset(this, 0, sizeof(GLContextSymbols));
    }

    typedef void (GLAPIENTRY * PFNGLACTIVETEXTUREPROC) (GLenum texture);
    PFNGLACTIVETEXTUREPROC fActiveTexture;
    typedef void (GLAPIENTRY * PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
    PFNGLATTACHSHADERPROC fAttachShader;
    typedef void (GLAPIENTRY * PFNGLBINDATTRIBLOCATIONPROC) (GLuint program, GLuint index, const GLchar* name);
    PFNGLBINDATTRIBLOCATIONPROC fBindAttribLocation;
    typedef void (GLAPIENTRY * PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
    PFNGLBINDBUFFERPROC fBindBuffer;
    typedef void (GLAPIENTRY * PFNGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
    PFNGLBINDTEXTUREPROC fBindTexture;
    typedef void (GLAPIENTRY * PFNGLBLENDCOLORPROC) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
    PFNGLBLENDCOLORPROC fBlendColor;
    typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONPROC) (GLenum mode);
    PFNGLBLENDEQUATIONPROC fBlendEquation;
    typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONSEPARATEPROC) (GLenum, GLenum);
    PFNGLBLENDEQUATIONSEPARATEPROC fBlendEquationSeparate;
    typedef void (GLAPIENTRY * PFNGLBLENDFUNCPROC) (GLenum, GLenum);
    PFNGLBLENDFUNCPROC fBlendFunc;
    typedef void (GLAPIENTRY * PFNGLBLENDFUNCSEPARATEPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
    PFNGLBLENDFUNCSEPARATEPROC fBlendFuncSeparate;
    typedef void (GLAPIENTRY * PFNGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
    PFNGLBUFFERDATAPROC fBufferData;
    typedef void (GLAPIENTRY * PFNGLBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);
    PFNGLBUFFERSUBDATAPROC fBufferSubData;
    typedef void (GLAPIENTRY * PFNGLCLEARPROC) (GLbitfield);
    PFNGLCLEARPROC fClear;
    typedef void (GLAPIENTRY * PFNGLCLEARCOLORPROC) (GLclampf, GLclampf, GLclampf, GLclampf);
    PFNGLCLEARCOLORPROC fClearColor;
    typedef void (GLAPIENTRY * PFNGLCLEARSTENCILPROC) (GLint);
    PFNGLCLEARSTENCILPROC fClearStencil;
    typedef void (GLAPIENTRY * PFNGLCOLORMASKPROC) (realGLboolean red, realGLboolean green, realGLboolean blue, realGLboolean alpha);
    PFNGLCOLORMASKPROC fColorMask;
    typedef void (GLAPIENTRY * PFNGLCULLFACEPROC) (GLenum mode);
    PFNGLCULLFACEPROC fCullFace;
    typedef void (GLAPIENTRY * PFNGLDETACHSHADERPROC) (GLuint program, GLuint shader);
    PFNGLDETACHSHADERPROC fDetachShader;
    typedef void (GLAPIENTRY * PFNGLDEPTHFUNCPROC) (GLenum);
    PFNGLDEPTHFUNCPROC fDepthFunc;
    typedef void (GLAPIENTRY * PFNGLDEPTHMASKPROC) (realGLboolean);
    PFNGLDEPTHMASKPROC fDepthMask;
    typedef void (GLAPIENTRY * PFNGLDISABLEPROC) (GLenum);
    PFNGLDISABLEPROC fDisable;
    typedef void (GLAPIENTRY * PFNGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint);
    PFNGLDISABLEVERTEXATTRIBARRAYPROC fDisableVertexAttribArray;
    typedef void (GLAPIENTRY * PFNGLDRAWARRAYSPROC) (GLenum mode, GLint first, GLsizei count);
    PFNGLDRAWARRAYSPROC fDrawArrays;
    typedef void (GLAPIENTRY * PFNGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
    PFNGLDRAWELEMENTSPROC fDrawElements;
    typedef void (GLAPIENTRY * PFNGLENABLEPROC) (GLenum);
    PFNGLENABLEPROC fEnable;
    typedef void (GLAPIENTRY * PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint);
    PFNGLENABLEVERTEXATTRIBARRAYPROC fEnableVertexAttribArray;
    typedef void (GLAPIENTRY * PFNGLFINISHPROC) (void);
    PFNGLFINISHPROC fFinish;
    typedef void (GLAPIENTRY * PFNGLFLUSHPROC) (void);
    PFNGLFLUSHPROC fFlush;
    typedef void (GLAPIENTRY * PFNGLFRONTFACEPROC) (GLenum);
    PFNGLFRONTFACEPROC fFrontFace;
    typedef void (GLAPIENTRY * PFNGLGETACTIVEATTRIBPROC) (GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
    PFNGLGETACTIVEATTRIBPROC fGetActiveAttrib;
    typedef void (GLAPIENTRY * PFNGLGETACTIVEUNIFORMPROC) (GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
    PFNGLGETACTIVEUNIFORMPROC fGetActiveUniform;
    typedef void (GLAPIENTRY * PFNGLGETATTACHEDSHADERSPROC) (GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders);
    PFNGLGETATTACHEDSHADERSPROC fGetAttachedShaders;
    typedef GLint (GLAPIENTRY * PFNGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar* name);
    PFNGLGETATTRIBLOCATIONPROC fGetAttribLocation;
    typedef void (GLAPIENTRY * PFNGLGETINTEGERVPROC) (GLenum pname, GLint *params);
    PFNGLGETINTEGERVPROC fGetIntegerv;
    typedef void (GLAPIENTRY * PFNGLGETFLOATVPROC) (GLenum pname, GLfloat *params);
    PFNGLGETFLOATVPROC fGetFloatv;
    typedef void (GLAPIENTRY * PFNGLGETBOOLEANBPROC) (GLenum pname, realGLboolean *params);
    PFNGLGETBOOLEANBPROC fGetBooleanv;
    typedef void (GLAPIENTRY * PFNGLGETBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint* params);
    PFNGLGETBUFFERPARAMETERIVPROC fGetBufferParameteriv;
    typedef void (GLAPIENTRY * PFNGLGENERATEMIPMAPPROC) (GLenum target);
    PFNGLGENERATEMIPMAPPROC fGenerateMipmap;
    typedef GLenum (GLAPIENTRY * PFNGLGETERRORPROC) (void);
    PFNGLGETERRORPROC fGetError;
    typedef void (GLAPIENTRY * PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint* param);
    PFNGLGETPROGRAMIVPROC fGetProgramiv;
    typedef void (GLAPIENTRY * PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
    PFNGLGETPROGRAMINFOLOGPROC fGetProgramInfoLog;
    typedef void (GLAPIENTRY * PFNGLTEXPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);
    PFNGLTEXPARAMETERIPROC fTexParameteri;
    typedef void (GLAPIENTRY * PFNGLTEXPARAMETERFPROC) (GLenum target, GLenum pname, GLfloat param);
    PFNGLTEXPARAMETERFPROC fTexParameterf;
    typedef GLubyte* (GLAPIENTRY * PFNGLGETSTRINGPROC) (GLenum);
    PFNGLGETSTRINGPROC fGetString;
    typedef void (GLAPIENTRY * PFNGLGETTEXIMAGEPROC) (GLenum target, GLint level, GLenum format, GLenum type, GLvoid* image);
    PFNGLGETTEXIMAGEPROC fGetTexImage;
    typedef void (GLAPIENTRY * PFNGLGETTEXLEVELPARAMETERIVPROC) (GLenum target, GLint level, GLenum pname, GLint *params);
    PFNGLGETTEXLEVELPARAMETERIVPROC fGetTexLevelParameteriv;
    typedef void (GLAPIENTRY * PFNGLGETTEXPARAMETERFVPROC) (GLenum target, GLenum pname, const GLfloat *params);
    PFNGLGETTEXPARAMETERFVPROC fGetTexParameterfv;
    typedef void (GLAPIENTRY * PFNGLGETTEXPARAMETERIVPROC) (GLenum target, GLenum pname, const GLint *params);
    PFNGLGETTEXPARAMETERIVPROC fGetTexParameteriv;
    typedef void (GLAPIENTRY * PFNGLGETUNIFORMFVPROC) (GLuint program, GLint location, GLfloat* params);
    PFNGLGETUNIFORMFVPROC fGetUniformfv;
    typedef void (GLAPIENTRY * PFNGLGETUNIFORMIVPROC) (GLuint program, GLint location, GLint* params);
    PFNGLGETUNIFORMIVPROC fGetUniformiv;
    typedef GLint (GLAPIENTRY * PFNGLGETUNIFORMLOCATIONPROC) (GLint programObj, const GLchar* name);
    PFNGLGETUNIFORMLOCATIONPROC fGetUniformLocation;
    typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBFVPROC) (GLuint, GLenum, GLfloat*);
    PFNGLGETVERTEXATTRIBFVPROC fGetVertexAttribfv;
    typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBIVPROC) (GLuint, GLenum, GLint*);
    PFNGLGETVERTEXATTRIBIVPROC fGetVertexAttribiv;
    typedef void (GLAPIENTRY * PFNGLHINTPROC) (GLenum target, GLenum mode);
    PFNGLHINTPROC fHint;
    typedef realGLboolean (GLAPIENTRY * PFNGLISBUFFERPROC) (GLuint buffer);
    PFNGLISBUFFERPROC fIsBuffer;
    typedef realGLboolean (GLAPIENTRY * PFNGLISENABLEDPROC) (GLenum cap);
    PFNGLISENABLEDPROC fIsEnabled;
    typedef realGLboolean (GLAPIENTRY * PFNGLISPROGRAMPROC) (GLuint program);
    PFNGLISPROGRAMPROC fIsProgram;
    typedef realGLboolean (GLAPIENTRY * PFNGLISSHADERPROC) (GLuint shader);
    PFNGLISSHADERPROC fIsShader;
    typedef realGLboolean (GLAPIENTRY * PFNGLISTEXTUREPROC) (GLuint texture);
    PFNGLISTEXTUREPROC fIsTexture;
    typedef void (GLAPIENTRY * PFNGLLINEWIDTHPROC) (GLfloat width);
    PFNGLLINEWIDTHPROC fLineWidth;
    typedef void (GLAPIENTRY * PFNGLLINKPROGRAMPROC) (GLuint program);
    PFNGLLINKPROGRAMPROC fLinkProgram;
    typedef void (GLAPIENTRY * PFNGLPIXELSTOREIPROC) (GLenum pname, GLint param);
    PFNGLPIXELSTOREIPROC fPixelStorei;
    typedef void (GLAPIENTRY * PFNGLPOLYGONOFFSETPROC) (GLfloat factor, GLfloat bias);
    PFNGLPOLYGONOFFSETPROC fPolygonOffset;
    typedef void (GLAPIENTRY * PFNGLREADBUFFERPROC) (GLenum);
    PFNGLREADBUFFERPROC fReadBuffer;
    typedef void (GLAPIENTRY * PFNGLREADPIXELSPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
    PFNGLREADPIXELSPROC fReadPixels;
    typedef void (GLAPIENTRY * PFNGLSAMPLECOVERAGEPROC) (GLclampf value, realGLboolean invert);
    PFNGLSAMPLECOVERAGEPROC fSampleCoverage;
    typedef void (GLAPIENTRY * PFNGLSTENCILFUNCPROC) (GLenum func, GLint ref, GLuint mask);
    PFNGLSTENCILFUNCPROC fStencilFunc;
    typedef void (GLAPIENTRY * PFNGLSTENCILFUNCSEPARATEPROC) (GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask);
    PFNGLSTENCILFUNCSEPARATEPROC fStencilFuncSeparate;
    typedef void (GLAPIENTRY * PFNGLSTENCILMASKPROC) (GLuint mask);
    PFNGLSTENCILMASKPROC fStencilMask;
    typedef void (GLAPIENTRY * PFNGLSTENCILMASKSEPARATEPROC) (GLenum, GLuint);
    PFNGLSTENCILMASKSEPARATEPROC fStencilMaskSeparate;
    typedef void (GLAPIENTRY * PFNGLSTENCILOPPROC) (GLenum fail, GLenum zfail, GLenum zpass);
    PFNGLSTENCILOPPROC fStencilOp;
    typedef void (GLAPIENTRY * PFNGLSTENCILOPSEPARATEPROC) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
    PFNGLSTENCILOPSEPARATEPROC fStencilOpSeparate;
    typedef void (GLAPIENTRY * PFNGLTEXIMAGE2DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
    PFNGLTEXIMAGE2DPROC fTexImage2D;
    typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
    PFNGLTEXSUBIMAGE2DPROC fTexSubImage2D;
    typedef void (GLAPIENTRY * PFNGLUNIFORM1FPROC) (GLint location, GLfloat v0);
    PFNGLUNIFORM1FPROC fUniform1f;
    typedef void (GLAPIENTRY * PFNGLUNIFORM1FVPROC) (GLint location, GLsizei count, const GLfloat* value);
    PFNGLUNIFORM1FVPROC fUniform1fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
    PFNGLUNIFORM1IPROC fUniform1i;
    typedef void (GLAPIENTRY * PFNGLUNIFORM1IVPROC) (GLint location, GLsizei count, const GLint* value);
    PFNGLUNIFORM1IVPROC fUniform1iv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM2FPROC) (GLint location, GLfloat v0, GLfloat v1);
    PFNGLUNIFORM2FPROC fUniform2f;
    typedef void (GLAPIENTRY * PFNGLUNIFORM2FVPROC) (GLint location, GLsizei count, const GLfloat* value);
    PFNGLUNIFORM2FVPROC fUniform2fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM2IPROC) (GLint location, GLint v0, GLint v1);
    PFNGLUNIFORM2IPROC fUniform2i;
    typedef void (GLAPIENTRY * PFNGLUNIFORM2IVPROC) (GLint location, GLsizei count, const GLint* value);
    PFNGLUNIFORM2IVPROC fUniform2iv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM3FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    PFNGLUNIFORM3FPROC fUniform3f;
    typedef void (GLAPIENTRY * PFNGLUNIFORM3FVPROC) (GLint location, GLsizei count, const GLfloat* value);
    PFNGLUNIFORM3FVPROC fUniform3fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM3IPROC) (GLint location, GLint v0, GLint v1, GLint v2);
    PFNGLUNIFORM3IPROC fUniform3i;
    typedef void (GLAPIENTRY * PFNGLUNIFORM3IVPROC) (GLint location, GLsizei count, const GLint* value);
    PFNGLUNIFORM3IVPROC fUniform3iv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM4FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    PFNGLUNIFORM4FPROC fUniform4f;
    typedef void (GLAPIENTRY * PFNGLUNIFORM4FVPROC) (GLint location, GLsizei count, const GLfloat* value);
    PFNGLUNIFORM4FVPROC fUniform4fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM4IPROC) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
    PFNGLUNIFORM4IPROC fUniform4i;
    typedef void (GLAPIENTRY * PFNGLUNIFORM4IVPROC) (GLint location, GLsizei count, const GLint* value);
    PFNGLUNIFORM4IVPROC fUniform4iv;
    typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX2FVPROC) (GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value);
    PFNGLUNIFORMMATRIX2FVPROC fUniformMatrix2fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX3FVPROC) (GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value);
    PFNGLUNIFORMMATRIX3FVPROC fUniformMatrix3fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX4FVPROC) (GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value);
    PFNGLUNIFORMMATRIX4FVPROC fUniformMatrix4fv;
    typedef void (GLAPIENTRY * PFNGLUSEPROGRAMPROC) (GLuint program);
    PFNGLUSEPROGRAMPROC fUseProgram;
    typedef void (GLAPIENTRY * PFNGLVALIDATEPROGRAMPROC) (GLuint program);
    PFNGLVALIDATEPROGRAMPROC fValidateProgram;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, realGLboolean normalized, GLsizei stride, const GLvoid* pointer);
    PFNGLVERTEXATTRIBPOINTERPROC fVertexAttribPointer;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1FPROC) (GLuint index, GLfloat x);
    PFNGLVERTEXATTRIB1FPROC fVertexAttrib1f;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2FPROC) (GLuint index, GLfloat x, GLfloat y);
    PFNGLVERTEXATTRIB2FPROC fVertexAttrib2f;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
    PFNGLVERTEXATTRIB3FPROC fVertexAttrib3f;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    PFNGLVERTEXATTRIB4FPROC fVertexAttrib4f;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1FVPROC) (GLuint index, const GLfloat* v);
    PFNGLVERTEXATTRIB1FVPROC fVertexAttrib1fv;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2FVPROC) (GLuint index, const GLfloat* v);
    PFNGLVERTEXATTRIB2FVPROC fVertexAttrib2fv;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3FVPROC) (GLuint index, const GLfloat* v);
    PFNGLVERTEXATTRIB3FVPROC fVertexAttrib3fv;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4FVPROC) (GLuint index, const GLfloat* v);
    PFNGLVERTEXATTRIB4FVPROC fVertexAttrib4fv;
    typedef void (GLAPIENTRY * PFNGLCOMPILESHADERPROC) (GLuint shader);
    PFNGLCOMPILESHADERPROC fCompileShader;
    typedef void (GLAPIENTRY * PFNGLCOPYTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
    PFNGLCOPYTEXIMAGE2DPROC fCopyTexImage2D;
    typedef void (GLAPIENTRY * PFNGLCOPYTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
    PFNGLCOPYTEXSUBIMAGE2DPROC fCopyTexSubImage2D;
    typedef void (GLAPIENTRY * PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint* param);
    PFNGLGETSHADERIVPROC fGetShaderiv;
    typedef void (GLAPIENTRY * PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
    PFNGLGETSHADERINFOLOGPROC fGetShaderInfoLog;
    typedef void (GLAPIENTRY * PFNGETSHADERPRECISIONFORMAT) (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);
    PFNGETSHADERPRECISIONFORMAT fGetShaderPrecisionFormat;
    typedef void (GLAPIENTRY * PFNGLGETSHADERSOURCEPROC) (GLint obj, GLsizei maxLength, GLsizei* length, GLchar* source);
    PFNGLGETSHADERSOURCEPROC fGetShaderSource;
    typedef void (GLAPIENTRY * PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar** strings, const GLint* lengths);
    PFNGLSHADERSOURCEPROC fShaderSource;

    typedef void (GLAPIENTRY * PFNGLBINDFRAMEBUFFER) (GLenum target, GLuint framebuffer);
    PFNGLBINDFRAMEBUFFER fBindFramebuffer;
    typedef void (GLAPIENTRY * PFNGLBINDRENDERBUFFER) (GLenum target, GLuint renderbuffer);
    PFNGLBINDRENDERBUFFER fBindRenderbuffer;
    typedef GLenum (GLAPIENTRY * PFNGLCHECKFRAMEBUFFERSTATUS) (GLenum target);
    PFNGLCHECKFRAMEBUFFERSTATUS fCheckFramebufferStatus;
    typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERRENDERBUFFER) (GLenum target, GLenum attachmentPoint, GLenum renderbufferTarget, GLuint renderbuffer);
    PFNGLFRAMEBUFFERRENDERBUFFER fFramebufferRenderbuffer;
    typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE2D) (GLenum target, GLenum attachmentPoint, GLenum textureTarget, GLuint texture, GLint level);
    PFNGLFRAMEBUFFERTEXTURE2D fFramebufferTexture2D;
    typedef void (GLAPIENTRY * PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIV) (GLenum target, GLenum attachment, GLenum pname, GLint* value);
    PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIV fGetFramebufferAttachmentParameteriv;
    typedef void (GLAPIENTRY * PFNGLGETRENDERBUFFERPARAMETERIV) (GLenum target, GLenum pname, GLint* value);
    PFNGLGETRENDERBUFFERPARAMETERIV fGetRenderbufferParameteriv;
    typedef realGLboolean (GLAPIENTRY * PFNGLISFRAMEBUFFER) (GLuint framebuffer);
    PFNGLISFRAMEBUFFER fIsFramebuffer;
    typedef realGLboolean (GLAPIENTRY * PFNGLISRENDERBUFFER) (GLuint renderbuffer);
    PFNGLISRENDERBUFFER fIsRenderbuffer;
    typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGE) (GLenum target, GLenum internalFormat, GLsizei width, GLsizei height);
    PFNGLRENDERBUFFERSTORAGE fRenderbufferStorage;

    typedef void (GLAPIENTRY * PFNGLBLITFRAMEBUFFER) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
    PFNGLBLITFRAMEBUFFER fBlitFramebuffer;
    typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEMULTISAMPLE) (GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height);
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLE fRenderbufferStorageMultisample;


    /* These are different between GLES2 and desktop GL; we hide those differences, use the GL
     * names, but the most limited data type.
     */
    typedef void (GLAPIENTRY * PFNGLDEPTHRANGEFPROC) (GLclampf, GLclampf);
    PFNGLDEPTHRANGEFPROC fDepthRangef;
    typedef void (GLAPIENTRY * PFNGLCLEARDEPTHFPROC) (GLclampf);
    PFNGLCLEARDEPTHFPROC fClearDepthf;

    typedef void (GLAPIENTRY * PFNGLDEPTHRANGEPROC) (GLclampd, GLclampd);
    PFNGLDEPTHRANGEPROC fDepthRange;
    typedef void (GLAPIENTRY * PFNGLCLEARDEPTHPROC) (GLclampd);
    PFNGLCLEARDEPTHPROC fClearDepth;

    /* These are special because we end up tracking these so that we don't
     * have to query the values from GL.
     */

    typedef void (GLAPIENTRY * PFNGLVIEWPORTPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
    PFNGLVIEWPORTPROC fViewport;
    typedef void (GLAPIENTRY * PFNGLSCISSORPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
    PFNGLSCISSORPROC fScissor;


    /* These are special -- they create or delete GL resources that can live
     * in a shared namespace.  In DEBUG, we wrap these calls so that we can
     * check when we have something that failed to do cleanup at the time the
     * final context is destroyed.
     */

    typedef GLuint (GLAPIENTRY * PFNGLCREATEPROGRAMPROC) (void);
    PFNGLCREATEPROGRAMPROC fCreateProgram;
    typedef GLuint (GLAPIENTRY * PFNGLCREATESHADERPROC) (GLenum type);
    PFNGLCREATESHADERPROC fCreateShader;
    typedef void (GLAPIENTRY * PFNGLGENBUFFERSPROC) (GLsizei n, GLuint* buffers);
    PFNGLGENBUFFERSPROC fGenBuffers;
    typedef void (GLAPIENTRY * PFNGLGENTEXTURESPROC) (GLsizei n, GLuint *textures);
    PFNGLGENTEXTURESPROC fGenTextures;
    typedef void (GLAPIENTRY * PFNGLGENFRAMEBUFFERS) (GLsizei n, GLuint* ids);
    PFNGLGENFRAMEBUFFERS fGenFramebuffers;
    typedef void (GLAPIENTRY * PFNGLGENRENDERBUFFERS) (GLsizei n, GLuint* ids);
    PFNGLGENRENDERBUFFERS fGenRenderbuffers;

    typedef void (GLAPIENTRY * PFNGLDELETEPROGRAMPROC) (GLuint program);
    PFNGLDELETEPROGRAMPROC fDeleteProgram;
    typedef void (GLAPIENTRY * PFNGLDELETESHADERPROC) (GLuint shader);
    PFNGLDELETESHADERPROC fDeleteShader;
    typedef void (GLAPIENTRY * PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint* buffers);
    PFNGLDELETEBUFFERSPROC fDeleteBuffers;
    typedef void (GLAPIENTRY * PFNGLDELETETEXTURESPROC) (GLsizei n, const GLuint* textures);
    PFNGLDELETETEXTURESPROC fDeleteTextures;
    typedef void (GLAPIENTRY * PFNGLDELETEFRAMEBUFFERS) (GLsizei n, const GLuint* ids);
    PFNGLDELETEFRAMEBUFFERS fDeleteFramebuffers;
    typedef void (GLAPIENTRY * PFNGLDELETERENDERBUFFERS) (GLsizei n, const GLuint* ids);
    PFNGLDELETERENDERBUFFERS fDeleteRenderbuffers;

    typedef void* (GLAPIENTRY * PFNGLMAPBUFFER) (GLenum target, GLenum access);
    PFNGLMAPBUFFER fMapBuffer;
    typedef realGLboolean (GLAPIENTRY * PFNGLUNMAPBUFFER) (GLenum target);
    PFNGLUNMAPBUFFER fUnmapBuffer;

    typedef GLenum (GLAPIENTRY * PFNGLGETGRAPHICSRESETSTATUS) (void);
    PFNGLGETGRAPHICSRESETSTATUS fGetGraphicsResetStatus;
};

}
}

#endif /* GLCONTEXTSYMBOLS_H_ */
