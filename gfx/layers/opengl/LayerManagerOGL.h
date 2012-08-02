/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERMANAGEROGL_H
#define GFX_LAYERMANAGEROGL_H

#include "CompositorOGL.h"
#include "LayerManagerOGLProgram.h"

#include "mozilla/layers/ShadowLayers.h"

#include "mozilla/TimeStamp.h"


#ifdef XP_WIN
#include <windows.h>
#endif

/**
 * We don't include GLDefs.h here since we don't want to drag in all defines
 * in for all our users.
 */
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#include "gfxContext.h"
#include "gfx3DMatrix.h"
#include "nsIWidget.h"
#include "GLContext.h"

namespace mozilla {
namespace layers {

class LayerOGL;
class ShadowThebesLayer;
class ShadowContainerLayer;
class ShadowImageLayer;
class ShadowCanvasLayer;
class ShadowColorLayer;

/**
 * This is the LayerManager used for OpenGL 2.1 and OpenGL ES 2.0.
 * This can be used either on the main thread or the compositor.
 */
class THEBES_API LayerManagerOGL :
    public ShadowLayerManager
{
  typedef mozilla::gl::GLContext GLContext;
  typedef mozilla::gl::ShaderProgramType ProgramType;

public:
  LayerManagerOGL(nsIWidget *aWidget, int aSurfaceWidth = -1, int aSurfaceHeight = -1,
                  bool aIsRenderingToEGLSurface = false);
  virtual ~LayerManagerOGL()
  {
    Destroy();
  }

  virtual void Destroy();

  /**
   * Initializes the layer manager with a given GLContext. If aContext is null
   * then the layer manager will try to create one for the associated widget.
   *
   * \param aContext an existing GL context to use. Can be created with CreateContext()
   *
   * \return True is initialization was succesful, false when it was not.
   */
  bool Initialize(bool force = false)
  {
    return mCompositor->Initialize(force, nullptr);
  }

  bool Initialize(nsRefPtr<GLContext> aContext, bool force = false)
  {
    return mCompositor->Initialize(force, aContext);
  }

  //TODO[nrc] should this be here?
  // should we be able to have our own GLContext as well as a compositor?
  GLContext* gl() const { return mCompositor->mGLContext; }


  /**
   * Sets the clipping region for this layer manager. This is important on 
   * windows because using OGL we no longer have GDI's native clipping. Therefor
   * widget must tell us what part of the screen is being invalidated,
   * and we should clip to this.
   *
   * \param aClippingRegion Region to clip to. Setting an empty region
   * will disable clipping.
   */
  void SetClippingRegion(const nsIntRegion& aClippingRegion)
  {
    mClippingRegion = aClippingRegion;
  }

  /**
   * LayerManager implementation.
   */
  virtual ShadowLayerManager* AsShadowManager()
  {
    return this;
  }

  void BeginTransaction();

  void BeginTransactionWithTarget(gfxContext* aTarget);

  void EndConstruction();

  virtual bool EndEmptyTransaction();
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT);

  virtual void SetRoot(Layer* aLayer) { mRoot = aLayer; }

  virtual bool CanUseCanvasLayerForSize(const gfxIntSize &aSize)
  {
    return mCompositor->CanUseCanvasLayerForSize(aSize);
  }

  virtual PRInt32 GetMaxTextureSize() const
  {
    return mCompositor->GetMaxTextureSize();
  }

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();

  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();

  virtual already_AddRefed<ImageLayer> CreateImageLayer();

  virtual already_AddRefed<ColorLayer> CreateColorLayer();

  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();

  virtual already_AddRefed<ShadowThebesLayer> CreateShadowThebesLayer();
  virtual already_AddRefed<ShadowContainerLayer> CreateShadowContainerLayer();
  virtual already_AddRefed<ShadowImageLayer> CreateShadowImageLayer();
  virtual already_AddRefed<ShadowColorLayer> CreateShadowColorLayer();
  virtual already_AddRefed<ShadowCanvasLayer> CreateShadowCanvasLayer();
  virtual already_AddRefed<ShadowRefLayer> CreateShadowRefLayer();

  virtual LayersBackend GetBackendType() { return LAYERS_OPENGL; }
  virtual void GetBackendName(nsAString& name) { name.AssignLiteral("OpenGL"); }

  virtual already_AddRefed<gfxASurface>
    CreateOptimalMaskSurface(const gfxIntSize &aSize);

  /**
   * Helper methods.
   */
  void MakeCurrent(bool aForce = false) {
    mCompositor->MakeCurrent(aForce);
  }

  ShaderProgramOGL* GetBasicLayerProgram(bool aOpaque, bool aIsRGB,
                                         MaskType aMask = MaskNone)
  {
    return mCompositor->GetBasicLayerProgram(aOpaque, aIsRGB, aMask);
  }

  ShaderProgramOGL* GetProgram(gl::ShaderProgramType aType,
                               Layer* aMaskLayer) {
    if (aMaskLayer)
      return mCompositor->GetProgram(aType, Mask2d);
    return mCompositor->GetProgram(aType, MaskNone);
  }

  ShaderProgramOGL* GetProgram(gl::ShaderProgramType aType,
                               MaskType aMask = MaskNone) {
    return mCompositor->GetProgram(aType, aMask);
  }

  ShaderProgramOGL* GetFBOLayerProgram(MaskType aMask = MaskNone) {
    return GetProgram(GetFBOLayerProgramType(), aMask);
  }

  gl::ShaderProgramType GetFBOLayerProgramType() {
    return mCompositor->GetFBOLayerProgramType();
  }

  DrawThebesLayerCallback GetThebesLayerCallback() const
  { return mThebesLayerCallback; }

  void* GetThebesLayerCallbackData() const
  { return mThebesLayerCallbackData; }

  /*
   * Helper functions for our layers
   */
  void CallThebesLayerDrawCallback(ThebesLayer* aLayer,
                                   gfxContext* aContext,
                                   const nsIntRegion& aRegionToDraw)
  {
    NS_ASSERTION(mThebesLayerCallback,
                 "CallThebesLayerDrawCallback without callback!");
    mThebesLayerCallback(aLayer, aContext,
                         aRegionToDraw, nsIntRegion(),
                         mThebesLayerCallbackData);
  }

  /**
   * Controls how to initialize the texture / FBO created by
   * CreateFBOWithTexture.
   *  - InitModeNone: No initialization, contents are undefined.
   *  - InitModeClear: Clears the FBO.
   *  - InitModeCopy: Copies the contents of the current glReadBuffer into the
   *    texture.
   */
  enum InitMode {
    InitModeNone,
    InitModeClear,
    InitModeCopy
  };

  //TODO[nrc] get rid of this
  SurfaceInitMode InitModeToSurfaceInitMode(InitMode aInit)
  {
    switch (aInit) {
    case InitModeNone:
      return INIT_MODE_NONE;
    case InitModeClear:
      return INIT_MODE_CLEAR;
    case InitModeCopy:
      return INIT_MODE_COPY;
    default:
      return INIT_MODE_NONE;
    }
  }

  /* Create a FBO backed by a texture; will leave the FBO
   * bound.  Note that the texture target type will be
   * of the type returned by FBOTextureTarget; different
   * shaders are required to sample from the different
   * texture types.
   */
  void CreateFBOWithTexture(const nsIntRect& aRect, InitMode aInit,
                            GLuint aCurrentFrameBuffer,
                            GLuint *aFBO, GLuint *aTexture)
  {
    mCompositor->CreateFBOWithTexture(gfx::IntRect(aRect.x, aRect.y, aRect.width, aRect.height), InitModeToSurfaceInitMode(aInit), aCurrentFrameBuffer, aFBO, aTexture);
  }

  GLenum FBOTextureTarget() { return mCompositor->mFBOTextureTarget; }

  GLuint QuadVBO() { return mCompositor->QuadVBO(); }
  GLintptr QuadVBOVertexOffset() { return mCompositor->QuadVBOVertexOffset(); }
  GLintptr QuadVBOTexCoordOffset() { return mCompositor->QuadVBOTexCoordOffset(); }
  GLintptr QuadVBOFlippedTexCoordOffset() { mCompositor->QuadVBOFlippedTexCoordOffset(); }

  void BindQuadVBO() {
    mCompositor->BindQuadVBO();
  }

  void QuadVBOVerticesAttrib(GLuint aAttribIndex) {
    mCompositor->QuadVBOVerticesAttrib(aAttribIndex);
  }

  void QuadVBOTexCoordsAttrib(GLuint aAttribIndex) {
    mCompositor->QuadVBOTexCoordsAttrib(aAttribIndex);
  }

  void QuadVBOFlippedTexCoordsAttrib(GLuint aAttribIndex) {
    mCompositor->QuadVBOFlippedTexCoordsAttrib(aAttribIndex);
  }

  // Super common

  void BindAndDrawQuad(GLuint aVertAttribIndex,
                       GLuint aTexCoordAttribIndex,
                       bool aFlipped = false)
  {
    mCompositor->BindAndDrawQuad(aVertAttribIndex, aTexCoordAttribIndex, aFlipped);
  }

  void BindAndDrawQuad(ShaderProgramOGL *aProg,
                       bool aFlipped = false)
  {
    mCompositor->BindAndDrawQuad(aProg, aFlipped);
  }

  // |aTexCoordRect| is the rectangle from the texture that we want to
  // draw using the given program.  The program already has a necessary
  // offset and scale, so the geometry that needs to be drawn is a unit
  // square from 0,0 to 1,1.
  //
  // |aTexSize| is the actual size of the texture, as it can be larger
  // than the rectangle given by |aTexCoordRect|.
  void BindAndDrawQuadWithTextureRect(ShaderProgramOGL *aProg,
                                      const nsIntRect& aTexCoordRect,
                                      const nsIntSize& aTexSize,
                                      GLenum aWrapMode = LOCAL_GL_REPEAT,
                                      bool aFlipped = false)
  {
    mCompositor->BindAndDrawQuadWithTextureRect(aProg,
                   gfx::IntRect(aTexCoordRect.x, aTexCoordRect.y, aTexCoordRect.width, aTexCoordRect.height),
                   gfx::IntSize(aTexSize.width, aTexSize.height),
                   aWrapMode, aFlipped);
  }


#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const { return "OGL"; }
#endif // MOZ_LAYERS_HAVE_LOG

  const nsIntSize& GetWidgetSize() {
    return mCompositor->mWidgetSize;
  }

  enum WorldTransforPolicy {
    ApplyWorldTransform,
    DontApplyWorldTransform
  };

  /**
   * Setup the viewport and projection matrix for rendering
   * to a window of the given dimensions.
   */
  void SetupPipeline(int aWidth, int aHeight, WorldTransforPolicy aTransformPolicy);

  /**
   * Setup World transform matrix.
   * Transform will be ignored if it is not PreservesAxisAlignedRectangles
   * or has non integer scale
   */
  void SetWorldTransform(const gfxMatrix& aMatrix);
  gfxMatrix& GetWorldTransform(void);
  void WorldTransformRect(nsIntRect& aRect);

  /**
   * Set the size of the surface we're rendering to.
   */
  void SetSurfaceSize(int width, int height)
  {
    mCompositor->SetSurfaceSize(width, height);
  }


private:
  /** 
   * Context target, NULL when drawing directly to our swap chain.
   */
  nsRefPtr<gfxContext> mTarget;

  //TODO[nrc] comment
  RefPtr<CompositorOGL> mCompositor;

  /** Region we're clipping our current drawing to. */
  nsIntRegion mClippingRegion;

  /** Current root layer. */
  LayerOGL *RootLayer() const;

  /**
   * Render the current layer tree to the active target.
   */
  void Render();

  /**
   * Copies the content of our backbuffer to the set transaction target.
   */
  void CopyToTarget(gfxContext *aTarget);

  /* Thebes layer callbacks; valid at the end of a transaciton,
   * while rendering */
  DrawThebesLayerCallback mThebesLayerCallback;
  void *mThebesLayerCallbackData;
  gfxMatrix mWorldMatrix;
};









/**
 * General information and tree management for OGL layers.
 */
class LayerOGL
{
public:
  LayerOGL(LayerManagerOGL *aManager)
    : mOGLManager(aManager), mDestroyed(false)
  { }

  virtual ~LayerOGL() { }

  virtual LayerOGL *GetFirstChildOGL() {
    return nullptr;
  }

  /* Do NOT call this from the generic LayerOGL destructor.  Only from the
   * concrete class destructor
   */
  virtual void Destroy() = 0;

  virtual Layer* GetLayer() = 0;

  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset) = 0;

  typedef mozilla::gl::GLContext GLContext;

  LayerManagerOGL* OGLManager() const { return mOGLManager; }
  GLContext *gl() const { return mOGLManager->gl(); }
  virtual void CleanupResources() = 0;

  /*
   * Loads the result of rendering the layer as an OpenGL texture in aTextureUnit.
   * Will try to use an existing texture if possible, or a temporary
   * one if not. It is the callee's responsibility to release the texture.
   * Will return true if a texture could be constructed and loaded, false otherwise.
   * The texture will not be transformed, i.e., it will be in the same coord
   * space as this.
   * Any layer that can be used as a mask layer should override this method.
   * aSize will contain the size of the image.
   */
  virtual bool LoadAsTexture(GLuint aTextureUnit, gfxIntSize* aSize)
  {
    NS_WARNING("LoadAsTexture called without being overriden");
    return false;
  }

protected:
  LayerManagerOGL *mOGLManager;
  bool mDestroyed;
};

} /* layers */
} /* mozilla */

#endif /* GFX_LAYERMANAGEROGL_H */
