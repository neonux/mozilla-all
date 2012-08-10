/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOROGL_H
#define MOZILLA_GFX_COMPOSITOROGL_H

#include "Compositor.h"
#include "GLContext.h"
#include "LayerManagerOGLProgram.h"

#include "mozilla/TimeStamp.h"

namespace mozilla {

namespace layers {

//TODO[nrc] remove this when we remove the friend decl
class LayerManagerOGL;

class CompositorOGL : public Compositor
{
  typedef mozilla::gl::GLContext GLContext;
  typedef mozilla::gl::ShaderProgramType ProgramType;

public:
  CompositorOGL(nsIWidget *aWidget, int aSurfaceWidth = -1, int aSurfaceHeight = -1,
                bool aIsRenderingToEGLSurface = false);

  virtual ~CompositorOGL();

  /**
   * Initializes the compositor with a given GLContext. force should indicate
   * whether GL layers have been force-enabled. If aContext is null, the compositor
   * creates a context for the associated widget. Returns true if initialization
   * is succesful, false otherwise.
   */
  bool Initialize(bool force = false, nsRefPtr<GLContext> aContext = nsnull);

  void Destroy();

  //TODO[nrc]
  virtual TemporaryRef<TextureHost>
    CreateTextureHost(const TextureIdentifier &aIdentifier)
  {
    return nullptr;
  }

  virtual TextureHostIdentifier GetTextureHostIdentifier()
  {
    TextureHostIdentifier result;
    result.mType = HOST_OGL;
    result.mMaxTextureSize = mGLContext->GetMaxTextureSize();
    return result;
  }


  virtual TemporaryRef<Texture>
    CreateTextureForData(const gfx::IntSize &aSize, PRInt8 *aData, PRUint32 aStride,
                         TextureFormat aFormat) MOZ_OVERRIDE;

  virtual TemporaryRef<ImageSource> 
    CreateImageSourceForSharedImage(ImageSourceType aType) MOZ_OVERRIDE;


  virtual TemporaryRef<Surface> CreateSurface(const gfx::IntRect &aRect,
                                              SurfaceInitMode aInit) MOZ_OVERRIDE;

  virtual TemporaryRef<Surface> CreateSurfaceFromSurface(const gfx::IntRect &aRect,
                                                         const Surface *aSource) MOZ_OVERRIDE;

  virtual void SetSurfaceTarget(Surface *aSurface) MOZ_OVERRIDE;

  virtual void RemoveSurfaceTarget() MOZ_OVERRIDE;

  virtual void DrawQuad(const gfx::Rect &aRect, const gfx::Rect *aSourceRect,
                        const gfx::Rect *aClipRect, const EffectChain &aEffectChain,
                        gfx::Float aOpacity, const gfx::Matrix4x4 &aTransform,
                        const gfx::Point &aOffset) MOZ_OVERRIDE;

  virtual void EndFrame() MOZ_OVERRIDE;

  virtual bool SupportsPartialTextureUpdate() MOZ_OVERRIDE
  {
    return mGLContext->CanUploadSubTextures();
  }

  virtual bool CanUseCanvasLayerForSize(const gfxIntSize &aSize)
  {
      if (!mGLContext)
          return false;
      PRInt32 maxSize = mGLContext->GetMaxTextureSize();
      return aSize <= gfxIntSize(maxSize, maxSize);
  }

  virtual PRInt32 GetMaxTextureSize() const
  {
    return mGLContext->GetMaxTextureSize();
  }

  /**
   * Set the size of the EGL surface we're rendering to.
   */
  void SetSurfaceSize(int width, int height);

  GLContext* gl() const { return mGLContext; }

  void MakeCurrent(bool aForce = false) {
    if (mDestroyed) {
      NS_WARNING("Call on destroyed layer manager");
      return;
    }
    mGLContext->MakeCurrent(aForce);
  }

  void SetTarget(gfxContext* aTarget)
  {
    mTarget = aTarget;
  }

  void SetClamping(GLuint aTexture)
  {
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);
    mGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    mGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
  }

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const { return "OGL"; }
#endif // MOZ_DUMP_PAINTING

private:
  /** 
   * Context target, NULL when drawing directly to our swap chain.
   */
  nsRefPtr<gfxContext> mTarget;

  /** Widget associated with this compositor */
  nsIWidget *mWidget;  // TODO: Do we really need to keep this?
  nsIntSize mWidgetSize;
  nsRefPtr<GLContext> mGLContext;

  /** The size of the surface we are rendering to */
  nsIntSize mSurfaceSize;

  already_AddRefed<mozilla::gl::GLContext> CreateContext();

  /** Backbuffer */
  //TODO remove these
  GLuint mBackBufferFBO;
  GLuint mBackBufferTexture;

  /** Shader Programs */
  struct ShaderProgramVariations {
    ShaderProgramOGL* mVariations[NumMaskTypes];
  };
  nsTArray<ShaderProgramVariations> mPrograms;

  /** Texture target to use for FBOs */
  GLenum mFBOTextureTarget;

  /** Currently bound FBO */
  GLuint mBoundFBO;

  /** VBO that has some basics in it for a textured quad,
   *  including vertex coords and texcoords for both
   *  flipped and unflipped textures */
  GLuint mQuadVBO;

  bool mHasBGRA;

  /**
   * When rendering to an EGL surface (e.g. on Android), we rely on being told
   * about size changes (via SetSurfaceSize) rather than pulling this information
   * from the widget.
   */
  bool mIsRenderingToEGLSurface;

  /**
   * Have we had DrawQuad calls since the last frame was rendered?
   */
  bool mFrameInProgress;

  void BeginFrame(const gfx::Rect *aClipRect, const gfxMatrix& aTransform);

  /**
   * Updates all layer programs with a new projection matrix.
   */
  void SetLayerProgramProjectionMatrix(const gfx3DMatrix& aMatrix);

  /**
   * Helper method for Initialize, creates all valid variations of a program
   * and adds them to mPrograms
   */
  void AddPrograms(gl::ShaderProgramType aType);

  ShaderProgramOGL* GetBasicLayerProgram(bool aOpaque, bool aIsRGB,
                                         MaskType aMask = MaskNone)
  {
    gl::ShaderProgramType format = gl::BGRALayerProgramType;
    if (aIsRGB) {
      if (aOpaque) {
        format = gl::RGBXLayerProgramType;
      } else {
        format = gl::RGBALayerProgramType;
      }
    } else {
      if (aOpaque) {
        format = gl::BGRXLayerProgramType;
      }
    }
    return GetProgram(format, aMask);
  }

  ShaderProgramOGL* GetProgram(gl::ShaderProgramType aType,
                               MaskType aMask = MaskNone) {
    NS_ASSERTION(ProgramProfileOGL::ProgramExists(aType, aMask),
                 "Invalid program type.");
    return mPrograms[aType].mVariations[aMask];
  }

  gl::ShaderProgramType GetFBOLayerProgramType() {
    if (mFBOTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB)
      return gl::RGBARectLayerProgramType;
    return gl::RGBALayerProgramType;
  }

  /* Create a FBO backed by a texture.
   * Note that the texture target type will be
   * of the type returned by FBOTextureTarget; different
   * shaders are required to sample from the different
   * texture types.
   */
  void CreateFBOWithTexture(const gfx::IntRect& aRect, SurfaceInitMode aInit,
                            GLuint aSourceFrameBuffer,
                            GLuint *aFBO, GLuint *aTexture);

  GLuint QuadVBO() { return mQuadVBO; }
  GLintptr QuadVBOVertexOffset() { return 0; }
  GLintptr QuadVBOTexCoordOffset() { return sizeof(float)*4*2; }
  GLintptr QuadVBOFlippedTexCoordOffset() { return sizeof(float)*8*2; }

  void BindQuadVBO() {
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);
  }

  void QuadVBOVerticesAttrib(GLuint aAttribIndex) {
    mGLContext->fVertexAttribPointer(aAttribIndex, 2,
                                     LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                     (GLvoid*) QuadVBOVertexOffset());
  }

  void QuadVBOTexCoordsAttrib(GLuint aAttribIndex) {
    mGLContext->fVertexAttribPointer(aAttribIndex, 2,
                                     LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                     (GLvoid*) QuadVBOTexCoordOffset());
  }

  void QuadVBOFlippedTexCoordsAttrib(GLuint aAttribIndex) {
    mGLContext->fVertexAttribPointer(aAttribIndex, 2,
                                     LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                     (GLvoid*) QuadVBOFlippedTexCoordOffset());
  }

  void BindAndDrawQuad(GLuint aVertAttribIndex,
                       GLuint aTexCoordAttribIndex,
                       bool aFlipped = false)
  {
    BindQuadVBO();
    QuadVBOVerticesAttrib(aVertAttribIndex);

    if (aTexCoordAttribIndex != GLuint(-1)) {
      if (aFlipped)
        QuadVBOFlippedTexCoordsAttrib(aTexCoordAttribIndex);
      else
        QuadVBOTexCoordsAttrib(aTexCoordAttribIndex);

      mGLContext->fEnableVertexAttribArray(aTexCoordAttribIndex);
    }

    mGLContext->fEnableVertexAttribArray(aVertAttribIndex);

    mGLContext->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);

    mGLContext->fDisableVertexAttribArray(aVertAttribIndex);

    if (aTexCoordAttribIndex != GLuint(-1)) {
      mGLContext->fDisableVertexAttribArray(aTexCoordAttribIndex);
    }
  }

  void BindAndDrawQuad(ShaderProgramOGL *aProg,
                       bool aFlipped = false)
  {
    NS_ASSERTION(aProg->HasInitialized(), "Shader program not correctly initialized");
    BindAndDrawQuad(aProg->AttribLocation(ShaderProgramOGL::VertexCoordAttrib),
                    aProg->AttribLocation(ShaderProgramOGL::TexCoordAttrib),
                    aFlipped);
  }

  void BindAndDrawQuadWithTextureRect(ShaderProgramOGL *aProg,
                                      const gfx::IntRect& aTexCoordRect,
                                      const gfx::IntSize& aTexSize,
                                      GLenum aWrapMode = LOCAL_GL_REPEAT,
                                      bool aFlipped = false);

  /**
   * Setup the viewport and projection matrix for rendering
   * to a window of the given dimensions.
   */
  void SetupPipeline(int aWidth, int aHeight, const gfxMatrix& aWorldTransform);

  void CleanupResources();

  /**
   * Copies the content of our backbuffer to the set transaction target.
   */
  void CopyToTarget(gfxContext *aTarget);

  bool mDestroyed;

  struct FPSState
  {
      GLuint texture;
      int fps;
      bool initialized;
      int fcount;
      TimeStamp last;

      FPSState()
        : texture(0)
        , fps(0)
        , initialized(false)
        , fcount(0)
      {
        last = TimeStamp::Now();
      }
      void DrawFPS(GLContext*, ShaderProgramOGL*);
  } mFPS;

  static bool sDrawFPS;

  //TODO[nrc] remove this when we are using only the compositor API in LayerManagerOGL
  friend class LayerManagerOGL;
};

}
}

#endif /* MOZILLA_GFX_COMPOSITOROGL_H */
