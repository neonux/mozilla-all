/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *   Mark Steele <mwsteele@gmail.com>
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

#ifndef WEBGLCONTEXT_H_
#define WEBGLCONTEXT_H_

#include <stdarg.h>
#include <vector>

#include "nsTArray.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"

#include "nsIDocShell.h"

#include "nsICanvasRenderingContextWebGL.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsHTMLCanvasElement.h"
#include "nsWeakReference.h"
#include "nsIDOMHTMLElement.h"
#include "nsIJSNativeInitializer.h"

#include "GLContext.h"
#include "Layers.h"

#define UNPACK_FLIP_Y_WEBGL            0x9240
#define UNPACK_PREMULTIPLY_ALPHA_WEBGL 0x9241
#define CONTEXT_LOST_WEBGL             0x9242

class nsIDocShell;

namespace mozilla {

class WebGLTexture;
class WebGLBuffer;
class WebGLProgram;
class WebGLShader;
class WebGLFramebuffer;
class WebGLRenderbuffer;
class WebGLUniformLocation;

class WebGLZeroingObject;
class WebGLContextBoundObject;

class WebGLObjectBaseRefPtr
{
protected:
    friend class WebGLZeroingObject;

    WebGLObjectBaseRefPtr()
        : mRawPtr(0)
    {
    }

    WebGLObjectBaseRefPtr(nsISupports *rawPtr)
        : mRawPtr(rawPtr)
    {
    }

    void Zero() {
        if (mRawPtr) {
            // Note: RemoveRefOwner isn't called here, because
            // the entire owner array will be cleared.
            mRawPtr->Release();
            mRawPtr = 0;
        }
    }

protected:
    nsISupports *mRawPtr;
};

template <class T>
class WebGLObjectRefPtr
    : public WebGLObjectBaseRefPtr
{
public:
    typedef T element_type;

    WebGLObjectRefPtr()
    { }

    WebGLObjectRefPtr(const WebGLObjectRefPtr<T>& aSmartPtr)
        : WebGLObjectBaseRefPtr(aSmartPtr.mRawPtr)
    {
        if (mRawPtr) {
            RawPtr()->AddRef();
            RawPtr()->AddRefOwner(this);
        }
    }

    WebGLObjectRefPtr(T *aRawPtr)
        : WebGLObjectBaseRefPtr(aRawPtr)
    {
        if (mRawPtr) {
            RawPtr()->AddRef();
            RawPtr()->AddRefOwner(this);
        }
    }

    WebGLObjectRefPtr(const already_AddRefed<T>& aSmartPtr)
        : WebGLObjectBaseRefPtr(aSmartPtr.mRawPtr)
          // construct from |dont_AddRef(expr)|
    {
        if (mRawPtr) {
            RawPtr()->AddRef();
            RawPtr()->AddRefOwner(this);
        }
    }

    ~WebGLObjectRefPtr() {
        if (mRawPtr) {
            RawPtr()->RemoveRefOwner(this);
            RawPtr()->Release();
        }
    }

    WebGLObjectRefPtr<T>&
    operator=(const WebGLObjectRefPtr<T>& rhs)
    {
        assign_with_AddRef(static_cast<T*>(rhs.mRawPtr));
        return *this;
    }

    WebGLObjectRefPtr<T>&
    operator=(T* rhs)
    {
        assign_with_AddRef(rhs);
        return *this;
    }

    WebGLObjectRefPtr<T>&
    operator=(const already_AddRefed<T>& rhs)
    {
        assign_assuming_AddRef(static_cast<T*>(rhs.mRawPtr));
        return *this;
    }

    T* get() const {
        return const_cast<T*>(static_cast<T*>(mRawPtr));
    }

    operator T*() const {
        return get();
    }

    T* operator->() const {
        NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL WebGLObjectRefPtr with operator->()!");
        return get();
    }

    T& operator*() const {
        NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL WebGLObjectRefPtr with operator*()!");
        return *get();
    }

private:
    T* RawPtr() { return static_cast<T*>(mRawPtr); }

    void assign_with_AddRef(T* rawPtr) {
        if (rawPtr) {
            rawPtr->AddRef();
            rawPtr->AddRefOwner(this);
        }

        assign_assuming_AddRef(rawPtr);
    }

    void assign_assuming_AddRef(T* newPtr) {
        T* oldPtr = RawPtr();
        mRawPtr = newPtr;
        if (oldPtr) {
            oldPtr->RemoveRefOwner(this);
            oldPtr->Release();
        }
    }
};

class WebGLBuffer;

struct WebGLVertexAttribData {
    WebGLVertexAttribData()
        : buf(0), stride(0), size(0), byteOffset(0), type(0), enabled(PR_FALSE)
    { }

    WebGLObjectRefPtr<WebGLBuffer> buf;
    WebGLuint stride;
    WebGLuint size;
    GLuint byteOffset;
    GLenum type;
    PRBool enabled;

    GLuint componentSize() const {
        switch(type) {
            case LOCAL_GL_BYTE:
                return sizeof(GLbyte);
                break;
            case LOCAL_GL_UNSIGNED_BYTE:
                return sizeof(GLubyte);
                break;
            case LOCAL_GL_SHORT:
                return sizeof(GLshort);
                break;
            case LOCAL_GL_UNSIGNED_SHORT:
                return sizeof(GLushort);
                break;
            // XXX case LOCAL_GL_FIXED:
            case LOCAL_GL_FLOAT:
                return sizeof(GLfloat);
                break;
            default:
                NS_ERROR("Should never get here!");
                return 0;
        }
    }

    GLuint actualStride() const {
        if (stride) return stride;
        return size * componentSize();
    }
};

class WebGLContext :
    public nsICanvasRenderingContextWebGL,
    public nsICanvasRenderingContextInternal,
    public nsSupportsWeakReference
{
public:
    WebGLContext();
    virtual ~WebGLContext();

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(WebGLContext, nsICanvasRenderingContextWebGL)

    NS_DECL_NSICANVASRENDERINGCONTEXTWEBGL

    // nsICanvasRenderingContextInternal
    NS_IMETHOD SetCanvasElement(nsHTMLCanvasElement* aParentCanvas);
    NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height);
    NS_IMETHOD InitializeWithSurface(nsIDocShell *docShell, gfxASurface *surface, PRInt32 width, PRInt32 height)
        { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Render(gfxContext *ctx, gfxPattern::GraphicsFilter f);
    NS_IMETHOD GetInputStream(const char* aMimeType,
                              const PRUnichar* aEncoderOptions,
                              nsIInputStream **aStream);
    NS_IMETHOD GetThebesSurface(gfxASurface **surface);
    NS_IMETHOD SetIsOpaque(PRBool b) { return NS_OK; };

    nsresult SynthesizeGLError(WebGLenum err);
    nsresult SynthesizeGLError(WebGLenum err, const char *fmt, ...);

    nsresult ErrorInvalidEnum(const char *fmt = 0, ...);
    nsresult ErrorInvalidOperation(const char *fmt = 0, ...);
    nsresult ErrorInvalidValue(const char *fmt = 0, ...);

    already_AddRefed<CanvasLayer> GetCanvasLayer(LayerManager *manager);
    void MarkContextClean() { }

    // a number that increments every time we have an event that causes
    // all context resources to be lost.
    PRUint32 Generation() { return mGeneration; }
protected:
    nsCOMPtr<nsIDOMHTMLCanvasElement> mCanvasElement;
    nsHTMLCanvasElement *HTMLCanvasElement() {
        return static_cast<nsHTMLCanvasElement*>(mCanvasElement.get());
    }

    nsRefPtr<gl::GLContext> gl;

    PRInt32 mWidth, mHeight;
    PRUint32 mGeneration;

    PRBool mInvalidated;

    WebGLuint mActiveTexture;
    WebGLenum mSynthesizedGLError;

    PRBool SafeToCreateCanvas3DContext(nsHTMLCanvasElement *canvasElement);
    PRBool InitAndValidateGL();
    PRBool ValidateBuffers(PRUint32 count);
    static PRBool ValidateCapabilityEnum(WebGLenum cap);
    static PRBool ValidateBlendEquationEnum(WebGLuint cap);
    static PRBool ValidateBlendFuncDstEnum(WebGLuint mode);
    static PRBool ValidateBlendFuncSrcEnum(WebGLuint mode);
    static PRBool ValidateTextureTargetEnum(WebGLenum target);
    static PRBool ValidateComparisonEnum(WebGLenum target);
    static PRBool ValidateStencilOpEnum(WebGLenum action);
    static PRBool ValidateFaceEnum(WebGLenum target);

    void Invalidate();

    void MakeContextCurrent() { gl->MakeCurrent(); }

    // helpers
    nsresult TexImage2D_base(WebGLenum target, WebGLint level, WebGLenum internalformat,
                             WebGLsizei width, WebGLsizei height, WebGLint border,
                             WebGLenum format, WebGLenum type,
                             void *data, PRUint32 byteLength);
    nsresult TexSubImage2D_base(WebGLenum target, WebGLint level,
                                WebGLint xoffset, WebGLint yoffset,
                                WebGLsizei width, WebGLsizei height,
                                WebGLenum format, WebGLenum type,
                                void *pixels, PRUint32 byteLength);
    nsresult ReadPixels_base(WebGLint x, WebGLint y, WebGLsizei width, WebGLsizei height,
                             WebGLenum format, WebGLenum type, void *data, PRUint32 byteLength);

    nsresult DOMElementToImageSurface(nsIDOMElement *imageOrCanvas,
                                      gfxImageSurface **imageOut,
                                      PRBool flipY, PRBool premultiplyAlpha);

    // Conversion from public nsI* interfaces to concrete objects
    template<class ConcreteObjectType, class BaseInterfaceType>
    PRBool GetConcreteObject(BaseInterfaceType *aInterface,
                             ConcreteObjectType **aConcreteObject,
                             PRBool *isNull = 0,
                             PRBool *isDeleted = 0);

    template<class ConcreteObjectType, class BaseInterfaceType>
    PRBool GetConcreteObjectAndGLName(BaseInterfaceType *aInterface,
                                      ConcreteObjectType **aConcreteObject,
                                      WebGLuint *aGLObjectName,
                                      PRBool *isNull = 0,
                                      PRBool *isDeleted = 0);

    template<class ConcreteObjectType, class BaseInterfaceType>
    PRBool GetGLName(BaseInterfaceType *aInterface,
                     WebGLuint *aGLObjectName,
                     PRBool *isNull = 0,
                     PRBool *isDeleted = 0);

    template<class ConcreteObjectType, class BaseInterfaceType>
    PRBool CheckConversion(BaseInterfaceType *aInterface,
                           PRBool *isNull = 0,
                           PRBool *isDeleted = 0);


    // the buffers bound to the current program's attribs
    nsTArray<WebGLVertexAttribData> mAttribBuffers;

    // the textures bound to any sampler uniforms
    nsTArray<WebGLObjectRefPtr<WebGLTexture> > mUniformTextures;

    // textures bound to 
    nsTArray<WebGLObjectRefPtr<WebGLTexture> > mBound2DTextures;
    nsTArray<WebGLObjectRefPtr<WebGLTexture> > mBoundCubeMapTextures;

    WebGLObjectRefPtr<WebGLBuffer> mBoundArrayBuffer;
    WebGLObjectRefPtr<WebGLBuffer> mBoundElementArrayBuffer;
    WebGLObjectRefPtr<WebGLProgram> mCurrentProgram;

    // XXX these 3 are wrong types, and aren't used atm (except for the length of the attachments)
    nsTArray<WebGLObjectRefPtr<WebGLTexture> > mFramebufferColorAttachments;
    nsRefPtr<WebGLFramebuffer> mFramebufferDepthAttachment;
    nsRefPtr<WebGLFramebuffer> mFramebufferStencilAttachment;

    nsRefPtr<WebGLFramebuffer> mBoundFramebuffer;
    nsRefPtr<WebGLRenderbuffer> mBoundRenderbuffer;

    // lookup tables for GL name -> object wrapper
    nsRefPtrHashtable<nsUint32HashKey, WebGLTexture> mMapTextures;
    nsRefPtrHashtable<nsUint32HashKey, WebGLBuffer> mMapBuffers;
    nsRefPtrHashtable<nsUint32HashKey, WebGLProgram> mMapPrograms;
    nsRefPtrHashtable<nsUint32HashKey, WebGLShader> mMapShaders;
    nsRefPtrHashtable<nsUint32HashKey, WebGLFramebuffer> mMapFramebuffers;
    nsRefPtrHashtable<nsUint32HashKey, WebGLRenderbuffer> mMapRenderbuffers;

    // WebGL-specific PixelStore parameters
    PRBool mPixelStoreFlipY, mPixelStorePremultiplyAlpha;

public:
    // console logging helpers
    static void LogMessage (const char *fmt, ...);
    static void LogMessage(const char *fmt, va_list ap);
};

// this class is a mixin for the named type wrappers, and is used
// by WebGLObjectRefPtr to tell the object who holds references, so that
// we can zero them out appropriately when the object is deleted, because
// it will be unbound in the GL.
class WebGLZeroingObject
{
public:
    WebGLZeroingObject()
    { }

    void AddRefOwner(WebGLObjectBaseRefPtr *owner) {
        mRefOwners.AppendElement(owner);
    }

    void RemoveRefOwner(WebGLObjectBaseRefPtr *owner) {
        mRefOwners.RemoveElement(owner);
    }

    void ZeroOwners() {
        WebGLObjectBaseRefPtr **owners = mRefOwners.Elements();
        
        for (PRUint32 i = 0; i < mRefOwners.Length(); i++) {
            owners[i]->Zero();
        }

        mRefOwners.Clear();
    }

protected:
    nsTArray<WebGLObjectBaseRefPtr *> mRefOwners;
};

// this class is a mixin for GL objects that have dimensions
// that we need to track.
class WebGLRectangleObject
{
protected:
    WebGLRectangleObject()
        : mWidth(0), mHeight(0) { }

public:
    WebGLsizei width() { return mWidth; }
    void width(WebGLsizei value) { mWidth = value; }

    WebGLsizei height() { return mHeight; }
    void height(WebGLsizei value) { mHeight = value; }

    void setDimensions(WebGLsizei width, WebGLsizei height) {
        mWidth = width;
        mHeight = height;
    }

    void setDimensions(WebGLRectangleObject *rect) {
        if (rect) {
            mWidth = rect->width();
            mHeight = rect->height();
        } else {
            mWidth = 0;
            mHeight = 0;
        }
    }

protected:
    WebGLsizei mWidth;
    WebGLsizei mHeight;
};

// This class is a mixin for objects that are tied to a specific
// context (which is to say, all of them).  They provide initialization
// as well as comparison with the current context.
class WebGLContextBoundObject
{
public:
    WebGLContextBoundObject(WebGLContext *context) {
        mContext = context;
        mContextGeneration = context->Generation();
    }

    PRBool IsCompatibleWithContext(WebGLContext *other) {
        return mContext == other &&
            mContextGeneration == other->Generation();
    }

protected:
    WebGLContext *mContext;
    PRUint32 mContextGeneration;
};

#define WEBGLBUFFER_PRIVATE_IID \
    {0xd69f22e9, 0x6f98, 0x48bd, {0xb6, 0x94, 0x34, 0x17, 0xed, 0x06, 0x11, 0xab}}
class WebGLBuffer :
    public nsIWebGLBuffer,
    public WebGLZeroingObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLBUFFER_PRIVATE_IID)

    WebGLBuffer(WebGLContext *context, WebGLuint name) :
        WebGLContextBoundObject(context),
        mName(name), mDeleted(PR_FALSE),
        mByteLength(0), mTarget(LOCAL_GL_NONE), mData(nsnull)
    { }

    ~WebGLBuffer() {
        Delete();
    }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();

        free(mData);
        mData = nsnull;

        mDeleted = PR_TRUE;
        mByteLength = 0;
    }

    PRBool Deleted() const { return mDeleted; }
    GLuint GLName() const { return mName; }
    GLuint ByteLength() const { return mByteLength; }
    GLenum Target() const { return mTarget; }
    const void *Data() const { return mData; }

    void SetByteLength(GLuint byteLength) { mByteLength = byteLength; }
    void SetTarget(GLenum target) { mTarget = target; }

    // element array buffers are the only buffers for which we need to keep a copy of the data.
    // this method assumes that the byte length has previously been set by calling SetByteLength.
    void CopyDataIfElementArray(const void* data) {
        if (mTarget == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
            mData = realloc(mData, mByteLength);
            memcpy(mData, data, mByteLength);
        }
    }

    // same comments as for CopyElementArrayData
    void ZeroDataIfElementArray() {
        if (mTarget == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
            mData = realloc(mData, mByteLength);
            memset(mData, 0, mByteLength);
        }
    }

    // same comments as for CopyElementArrayData
    void CopySubDataIfElementArray(GLuint byteOffset, GLuint byteLength, const void* data) {
        if (mTarget == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
            memcpy((void*) (size_t(mData)+byteOffset), data, byteLength);
        }
    }

    // this method too is only for element array buffers. It returns the maximum value in the part of
    // the buffer starting at given offset, consisting of given count of elements. The type T is the type
    // to interprete the array elements as, must be GLushort or GLubyte.
    template<typename T>
    T FindMaximum(GLuint count, GLuint byteOffset)
    {
        const T* start = reinterpret_cast<T*>(reinterpret_cast<size_t>(mData) + byteOffset);
        const T* stop = start + count;
        T result = 0;
        for(const T* ptr = start; ptr != stop; ++ptr) {
            if (*ptr > result) result = *ptr;
        }
        return result;
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLBUFFER
protected:
    WebGLuint mName;
    PRBool mDeleted;
    GLuint mByteLength;
    GLenum mTarget;
    void* mData; // in the case of an Element Array Buffer, we keep a copy.
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLBuffer, WEBGLBUFFER_PRIVATE_IID)

#define WEBGLTEXTURE_PRIVATE_IID \
    {0x4c19f189, 0x1f86, 0x4e61, {0x96, 0x21, 0x0a, 0x11, 0xda, 0x28, 0x10, 0xdd}}
class WebGLTexture :
    public nsIWebGLTexture,
    public WebGLZeroingObject,
    public WebGLRectangleObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLTEXTURE_PRIVATE_IID)

    WebGLTexture(WebGLContext *context, WebGLuint name) :
        WebGLContextBoundObject(context),
        mName(name), mDeleted(PR_FALSE)
    { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }

    PRBool Deleted() { return mDeleted; }
    WebGLuint GLName() { return mName; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLTEXTURE
protected:
    WebGLuint mName;
    PRBool mDeleted;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLTexture, WEBGLTEXTURE_PRIVATE_IID)

#define WEBGLSHADER_PRIVATE_IID \
    {0x48cce975, 0xd459, 0x4689, {0x83, 0x82, 0x37, 0x82, 0x6e, 0xac, 0xe0, 0xa7}}
class WebGLShader :
    public nsIWebGLShader,
    public WebGLZeroingObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLSHADER_PRIVATE_IID)

    WebGLShader(WebGLContext *context, WebGLuint name, WebGLenum stype) :
        WebGLContextBoundObject(context),
        mName(name), mDeleted(PR_FALSE), mType(stype)
    { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }

    PRBool Deleted() { return mDeleted; }
    WebGLuint GLName() { return mName; }
    WebGLenum ShaderType() { return mType; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLSHADER
protected:
    WebGLuint mName;
    PRBool mDeleted;
    WebGLenum mType;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLShader, WEBGLSHADER_PRIVATE_IID)

#define WEBGLPROGRAM_PRIVATE_IID \
    {0xb3084a5b, 0xa5b4, 0x4ee0, {0xa0, 0xf0, 0xfb, 0xdd, 0x64, 0xaf, 0x8e, 0x82}}
class WebGLProgram :
    public nsIWebGLProgram,
    public WebGLZeroingObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLPROGRAM_PRIVATE_IID)

    WebGLProgram(WebGLContext *context, WebGLuint name) :
        WebGLContextBoundObject(context),
        mName(name), mDeleted(PR_FALSE), mLinkStatus(PR_FALSE), mGeneration(0),
        mUniformMaxNameLength(0), mAttribMaxNameLength(0),
        mUniformCount(0), mAttribCount(0)
    {
        mMapUniformLocations.Init();
    }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }

    PRBool Deleted() { return mDeleted; }
    WebGLuint GLName() { return mName; }
    const nsTArray<WebGLShader*>& AttachedShaders() const { return mAttachedShaders; }
    PRBool LinkStatus() { return mLinkStatus; }
    GLuint Generation() const { return mGeneration; }
    void SetLinkStatus(PRBool val) { mLinkStatus = val; }

    PRBool ContainsShader(WebGLShader *shader) {
        return mAttachedShaders.Contains(shader);
    }

    // return true if the shader wasn't already attached
    PRBool AttachShader(WebGLShader *shader) {
        if (ContainsShader(shader))
            return PR_FALSE;
        mAttachedShaders.AppendElement(shader);
        return PR_TRUE;
    }

    // return true if the shader was found and removed
    PRBool DetachShader(WebGLShader *shader) {
        return mAttachedShaders.RemoveElement(shader);
    }

    PRBool HasBothShaderTypesAttached() {
        PRBool haveVertex = PR_FALSE;
        PRBool haveFrag = PR_FALSE;
        for (PRUint32 i = 0; i < mAttachedShaders.Length(); ++i) {
            if (mAttachedShaders[i]->ShaderType() == LOCAL_GL_FRAGMENT_SHADER)
                haveFrag = PR_TRUE;
            else if (mAttachedShaders[i]->ShaderType() == LOCAL_GL_VERTEX_SHADER)
                haveVertex = PR_TRUE;
            if (haveFrag && haveVertex)
                return PR_TRUE;
        }

        return PR_FALSE;
    }

    PRBool NextGeneration()
    {
        GLuint nextGeneration = mGeneration + 1;
        if (nextGeneration == 0)
            return PR_FALSE; // must exit without changing mGeneration
        mGeneration = nextGeneration;
        mMapUniformLocations.Clear();
        return PR_TRUE;
    }

    already_AddRefed<WebGLUniformLocation> GetUniformLocationObject(GLint glLocation);

    /* Called only after LinkProgram */
    PRBool UpdateInfo(gl::GLContext *gl);

    /* Getters for cached program info */
    WebGLint UniformMaxNameLength() const { return mUniformMaxNameLength; }
    WebGLint AttribMaxNameLength() const { return mAttribMaxNameLength; }
    WebGLint UniformCount() const { return mUniformCount; }
    WebGLint AttribCount() const { return mAttribCount; }
    bool IsAttribInUse(unsigned i) const { return mAttribsInUse[i]; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLPROGRAM
protected:
    WebGLuint mName;
    PRPackedBool mDeleted;
    PRPackedBool mLinkStatus;
    nsTArray<WebGLShader*> mAttachedShaders;
    nsRefPtrHashtable<nsUint32HashKey, WebGLUniformLocation> mMapUniformLocations;
    GLuint mGeneration;
    GLint mUniformMaxNameLength;
    GLint mAttribMaxNameLength;
    GLint mUniformCount;
    GLint mAttribCount;
    std::vector<bool> mAttribsInUse;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLProgram, WEBGLPROGRAM_PRIVATE_IID)

#define WEBGLFRAMEBUFFER_PRIVATE_IID \
    {0x0052a16f, 0x4bc9, 0x4a55, {0x9d, 0xa3, 0x54, 0x95, 0xaa, 0x4e, 0x80, 0xb9}}
class WebGLFramebuffer :
    public nsIWebGLFramebuffer,
    public WebGLZeroingObject,
    public WebGLRectangleObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLFRAMEBUFFER_PRIVATE_IID)

    WebGLFramebuffer(WebGLContext *context, WebGLuint name) :
        WebGLContextBoundObject(context),
        mName(name), mDeleted(PR_FALSE)
    { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }
    PRBool Deleted() { return mDeleted; }
    WebGLuint GLName() { return mName; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLFRAMEBUFFER
protected:
    WebGLuint mName;
    PRBool mDeleted;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLFramebuffer, WEBGLFRAMEBUFFER_PRIVATE_IID)

#define WEBGLRENDERBUFFER_PRIVATE_IID \
    {0x3cbc2067, 0x5831, 0x4e3f, {0xac, 0x52, 0x7e, 0xf4, 0x5c, 0x04, 0xff, 0xae}}
class WebGLRenderbuffer :
    public nsIWebGLRenderbuffer,
    public WebGLZeroingObject,
    public WebGLRectangleObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLRENDERBUFFER_PRIVATE_IID)

    WebGLRenderbuffer(WebGLContext *context, WebGLuint name) :
        WebGLContextBoundObject(context),
        mName(name), mDeleted(PR_FALSE)
    { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }
    PRBool Deleted() { return mDeleted; }
    WebGLuint GLName() { return mName; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLRENDERBUFFER
protected:
    WebGLuint mName;
    PRBool mDeleted;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLRenderbuffer, WEBGLRENDERBUFFER_PRIVATE_IID)

#define WEBGLUNIFORMLOCATION_PRIVATE_IID \
    {0x01a8a614, 0xb109, 0x42f1, {0xb4, 0x40, 0x8d, 0x8b, 0x87, 0x0b, 0x43, 0xa7}}
class WebGLUniformLocation :
    public nsIWebGLUniformLocation,
    public WebGLZeroingObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLUNIFORMLOCATION_PRIVATE_IID)

    WebGLUniformLocation(WebGLContext *context, WebGLProgram *program, GLint location) :
        WebGLContextBoundObject(context), mProgram(program), mProgramGeneration(program->Generation()),
        mLocation(location) { }

    WebGLProgram *Program() const { return mProgram; }
    GLint Location() const { return mLocation; }
    GLuint ProgramGeneration() const { return mProgramGeneration; }

    // needed for our generic helpers to check nsIxxx parameters, see GetConcreteObject.
    PRBool Deleted() { return PR_FALSE; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLUNIFORMLOCATION
protected:
    WebGLObjectRefPtr<WebGLProgram> mProgram;
    GLuint mProgramGeneration;
    GLint mLocation;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLUniformLocation, WEBGLUNIFORMLOCATION_PRIVATE_IID)

/**
 ** Template implementations
 **/

/* Helper function taking a BaseInterfaceType pointer and check that
 * it matches the required concrete implementation type (if it's
 * non-null), that it's not null/deleted unless we allowed it to, and
 * obtain a pointer to the concrete object.
 *
 * By default, null (respectively: deleted) aInterface pointers are
 * not allowed, but if you pass a non-null isNull (respectively:
 * isDeleted) pointer, then they become allowed and the value at
 * isNull (respecively isDeleted) is overwritten. In case of a null
 * pointer, the resulting
 */

template<class ConcreteObjectType, class BaseInterfaceType>
PRBool
WebGLContext::GetConcreteObject(BaseInterfaceType *aInterface,
                                ConcreteObjectType **aConcreteObject,
                                PRBool *isNull,
                                PRBool *isDeleted)
{
    if (!aInterface) {
        if (NS_LIKELY(isNull)) {
            // non-null isNull means that the caller will accept a null arg
            *isNull = PR_TRUE;
            if(isDeleted) *isDeleted = PR_FALSE;
            *aConcreteObject = 0;
            return PR_TRUE;
        } else {
            LogMessage("Null object passed to WebGL function");
            return PR_FALSE;
        }
    }

    if (isNull)
        *isNull = PR_FALSE;

    nsresult rv;
    nsCOMPtr<ConcreteObjectType> tmp(do_QueryInterface(aInterface, &rv));
    if (NS_FAILED(rv))
        return PR_FALSE;

    *aConcreteObject = tmp;

    if (!(*aConcreteObject)->IsCompatibleWithContext(this)) {
        // the object doesn't belong to this WebGLContext
        LogMessage("Object from different WebGL context given as argument (or older generation of this one)");
        return PR_FALSE;
    }

    if ((*aConcreteObject)->Deleted()) {
        if (NS_LIKELY(isDeleted)) {
            // non-null isDeleted means that the caller will accept a deleted arg
            *isDeleted = PR_TRUE;
            return PR_TRUE;
        } else {
            LogMessage("Deleted object passed to WebGL function");
            return PR_FALSE;
        }
    }

    if (isDeleted)
      *isDeleted = PR_FALSE;

    return PR_TRUE;
}

/* Same as GetConcreteObject, and in addition gets the GL object name.
 * Null objects give the name 0.
 */
template<class ConcreteObjectType, class BaseInterfaceType>
PRBool
WebGLContext::GetConcreteObjectAndGLName(BaseInterfaceType *aInterface,
                                         ConcreteObjectType **aConcreteObject,
                                         WebGLuint *aGLObjectName,
                                         PRBool *isNull,
                                         PRBool *isDeleted)
{
    PRBool result = GetConcreteObject(aInterface, aConcreteObject, isNull, isDeleted);
    if (result == PR_FALSE) return PR_FALSE;
    *aGLObjectName = *aConcreteObject ? (*aConcreteObject)->GLName() : 0;
    return PR_TRUE;
}

/* Same as GetConcreteObjectAndGLName when you don't need the concrete object pointer.
 */
template<class ConcreteObjectType, class BaseInterfaceType>
PRBool
WebGLContext::GetGLName(BaseInterfaceType *aInterface,
                        WebGLuint *aGLObjectName,
                        PRBool *isNull,
                        PRBool *isDeleted)
{
    ConcreteObjectType *aConcreteObject;
    return GetConcreteObjectAndGLName(aInterface, &aConcreteObject, aGLObjectName, isNull, isDeleted);
}

/* Same as GetConcreteObject when you only want to check if the conversion succeeds.
 */
template<class ConcreteObjectType, class BaseInterfaceType>
PRBool
WebGLContext::CheckConversion(BaseInterfaceType *aInterface,
                              PRBool *isNull,
                              PRBool *isDeleted)
{
    ConcreteObjectType *aConcreteObject;
    return GetConcreteObject(aInterface, &aConcreteObject, isNull, isDeleted);
}

}

#endif
