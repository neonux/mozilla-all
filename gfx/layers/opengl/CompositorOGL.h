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
                        const gfx::Matrix3x3 &aTransform) MOZ_OVERRIDE;

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

private:
  /** Widget associated with this compositor */
  nsIWidget *mWidget;  // TODO: Do we really need to keep this?
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
   * Helper method for Initialize, creates all valid variations of a program
   * and adds them to mPrograms
   */
  void AddPrograms(gl::ShaderProgramType aType);

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
