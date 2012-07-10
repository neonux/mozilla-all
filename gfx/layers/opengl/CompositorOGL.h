/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOROGL_H
#define MOZILLA_GFX_COMPOSITOROGL_H

#include "Compositor.h"
#include "GLContext.h"
#include "LayerManagerOGLProgram.h"

namespace mozilla {

namespace layers {

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

  virtual TextureHostIdentifier GetTextureHostIdentifier() MOZ_OVERRIDE;

  virtual TemporaryRef<Texture>
    CreateTextureForData(const gfx::IntSize &aSize, PRInt8 *aData, PRUint32 aStride,
                         TextureFormat aFormat) MOZ_OVERRIDE;

  virtual TemporaryRef<DrawableTextureHost>
    CreateDrawableTexture(const TextureIdentifier &aIdentifier) MOZ_OVERRIDE;

  virtual void DrawQuad(const gfx::Rect &aRect, const gfx::Rect *aSourceRect,
                        const gfx::Rect *aClipRect, const EffectChain &aEffectChain,
                        gfx::Float aOpacity, const gfx::Matrix4x4 &aTransform) MOZ_OVERRIDE;

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

  void FlushToScreen();

private:
  /** Widget associated with this compositor */
  nsIWidget *mWidget;  // TODO: Do we really need to keep this?
  nsIntSize mWidgetSize;
  nsRefPtr<GLContext> mGLContext;

  /** The size of the surface we are rendering to */
  nsIntSize mSurfaceSize;

  already_AddRefed<mozilla::gl::GLContext> CreateContext();

  /** Backbuffer */
  GLuint mBackBufferFBO;
  GLuint mBackBufferTexture;
  nsIntSize mBackBufferSize;

  /** Shader Programs */
  struct ShaderProgramVariations {
    ShaderProgramOGL* mVariations[NumMaskTypes];
  };
  nsTArray<ShaderProgramVariations> mPrograms;

  /** Texture target to use for FBOs */
  GLenum mFBOTextureTarget;

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

  void BeginFrame(const gfx::Rect *aClipRect);

  /**
   * Updates all layer programs with a new projection matrix.
   */
  void SetLayerProgramProjectionMatrix(const gfx3DMatrix& aMatrix);

  /**
   * Helper method for Initialize, creates all valid variations of a program
   * and adds them to mPrograms
   */
  void AddPrograms(gl::ShaderProgramType aType);

  ShaderProgramOGL* GetProgram(gl::ShaderProgramType aType,
                               MaskType aMask = MaskNone) {
    NS_ASSERTION(ProgramProfileOGL::ProgramExists(aType, aMask),
                 "Invalid program type.");
    return mPrograms[aType].mVariations[aMask];
  }

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
  void SetupPipeline(int aWidth, int aHeight);

  void CleanupResources();

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
};

}
}

#endif /* MOZILLA_GFX_COMPOSITOROGL_H */
