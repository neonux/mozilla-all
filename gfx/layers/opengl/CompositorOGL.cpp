/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorOGL.h"
#include "TextureOGL.h"
#include "SurfaceOGL.h"
#include "mozilla/Preferences.h"

#include "gfxUtils.h"

#include "GLContextProvider.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"

#include "gfxCrashReporterUtils.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gl;

#ifdef CHECK_CURRENT_PROGRAM
int ShaderProgramOGL::sCurrentProgramKey = 0;
#endif

CompositorOGL::CompositorOGL(nsIWidget *aWidget, int aSurfaceWidth,
                             int aSurfaceHeight, bool aIsRenderingToEGLSurface)
  : mWidget(aWidget)
  , mWidgetSize(-1, -1)
  , mSurfaceSize(aSurfaceWidth, aSurfaceHeight)
  , mBackBufferFBO(0)
  , mBackBufferTexture(0)
  , mBoundFBO(0)
  , mHasBGRA(0)
  , mIsRenderingToEGLSurface(aIsRenderingToEGLSurface)
  , mFrameInProgress(false)
  , mDestroyed(false)
{
}

CompositorOGL::~CompositorOGL()
{
  Destroy();
}

already_AddRefed<mozilla::gl::GLContext>
CompositorOGL::CreateContext()
{
  nsRefPtr<GLContext> context;

#ifdef XP_WIN
  if (PR_GetEnv("MOZ_LAYERS_PREFER_EGL")) {
    printf_stderr("Trying GL layers...\n");
    context = gl::GLContextProviderEGL::CreateForWindow(mWidget);
  }
#endif

  if (!context)
    context = gl::GLContextProvider::CreateForWindow(mWidget);

  if (!context) {
    NS_WARNING("Failed to create CompositorOGL context");
  }
  return context.forget();
}

void
CompositorOGL::AddPrograms(ShaderProgramType aType)
{
  for (PRUint32 maskType = MaskNone; maskType < NumMaskTypes; ++maskType) {
    if (ProgramProfileOGL::ProgramExists(aType, static_cast<MaskType>(maskType))) {
      mPrograms[aType].mVariations[maskType] = new ShaderProgramOGL(this->gl(),
        ProgramProfileOGL::GetProfileFor(aType, static_cast<MaskType>(maskType)));
    } else {
      mPrograms[aType].mVariations[maskType] = nullptr;
    }
  }
}

void
CompositorOGL::Destroy()
{
  if (!mDestroyed) {
    mDestroyed = true;
    CleanupResources();
  }
}

void
CompositorOGL::CleanupResources()
{
  if (!mGLContext)
    return;

  nsRefPtr<GLContext> ctx = mGLContext->GetSharedContext();
  if (!ctx) {
    ctx = mGLContext;
  }

  ctx->MakeCurrent();

  for (PRUint32 i = 0; i < mPrograms.Length(); ++i) {
    for (PRUint32 type = MaskNone; type < NumMaskTypes; ++type) {
      delete mPrograms[i].mVariations[type];
    }
  }
  mPrograms.Clear();

  ctx->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  if (mBackBufferFBO) {
    ctx->fDeleteFramebuffers(1, &mBackBufferFBO);
    mBackBufferFBO = 0;
  }

  if (mBackBufferTexture) {
    ctx->fDeleteTextures(1, &mBackBufferTexture);
    mBackBufferTexture = 0;
  }

  if (mQuadVBO) {
    ctx->fDeleteBuffers(1, &mQuadVBO);
    mQuadVBO = 0;
  }

  mGLContext = nullptr;
}

bool
CompositorOGL::Initialize(bool force, nsRefPtr<GLContext> aContext)
{
  ScopedGfxFeatureReporter reporter("GL Layers", force);

  // Do not allow double initialization
  NS_ABORT_IF_FALSE(mGLContext == nsnull, "Don't reinitialize CompositorOGL");

  if (aContext) {
    mGLContext = aContext;
  } else {
    mGLContext = CreateContext();
  }

#ifdef MOZ_WIDGET_ANDROID
  if (!mGLContext)
    NS_RUNTIMEABORT("We need a context on Android");
#endif

  if (!mGLContext)
    return false;

  mGLContext->SetFlipped(true);

  MakeCurrent();

  mHasBGRA =
    mGLContext->IsExtensionSupported(gl::GLContext::EXT_texture_format_BGRA8888) ||
    mGLContext->IsExtensionSupported(gl::GLContext::EXT_bgra);

  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  mPrograms.AppendElements(NumProgramTypes);
  for (int type = 0; type < NumProgramTypes; ++type) {
    AddPrograms(static_cast<ShaderProgramType>(type));
  }

  // initialise a common shader to check that we can actually compile a shader
  if (!mPrograms[gl::RGBALayerProgramType].mVariations[MaskNone]->Initialize()) {
    return false;
  }

  //TODO can we skip some of this initialisation because we won't do double buffering?
  mGLContext->fGenFramebuffers(1, &mBackBufferFBO);

  if (mGLContext->WorkAroundDriverBugs()) {

    /**
    * We'll test the ability here to bind NPOT textures to a framebuffer, if
    * this fails we'll try ARB_texture_rectangle.
    */

    GLenum textureTargets[] = {
      LOCAL_GL_TEXTURE_2D,
      LOCAL_GL_NONE
    };

    if (mGLContext->IsGLES2()) {
        textureTargets[1] = LOCAL_GL_TEXTURE_RECTANGLE_ARB;
    }

    mFBOTextureTarget = LOCAL_GL_NONE;

    for (PRUint32 i = 0; i < ArrayLength(textureTargets); i++) {
      GLenum target = textureTargets[i];
      if (!target)
          continue;

      mGLContext->fGenTextures(1, &mBackBufferTexture);
      mGLContext->fBindTexture(target, mBackBufferTexture);
      mGLContext->fTexParameteri(target,
                                LOCAL_GL_TEXTURE_MIN_FILTER,
                                LOCAL_GL_NEAREST);
      mGLContext->fTexParameteri(target,
                                LOCAL_GL_TEXTURE_MAG_FILTER,
                                LOCAL_GL_NEAREST);
      mGLContext->fTexImage2D(target,
                              0,
                              LOCAL_GL_RGBA,
                              5, 3, /* sufficiently NPOT */
                              0,
                              LOCAL_GL_RGBA,
                              LOCAL_GL_UNSIGNED_BYTE,
                              NULL);

      // unbind this texture, in preparation for binding it to the FBO
      mGLContext->fBindTexture(target, 0);

      mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mBackBufferFBO);
      mGLContext->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                        LOCAL_GL_COLOR_ATTACHMENT0,
                                        target,
                                        mBackBufferTexture,
                                        0);

      if (mGLContext->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) ==
          LOCAL_GL_FRAMEBUFFER_COMPLETE)
      {
        mFBOTextureTarget = target;
        break;
      }

      // We weren't succesful with this texture, so we don't need it
      // any more.
      mGLContext->fDeleteTextures(1, &mBackBufferTexture);
    }

    if (mFBOTextureTarget == LOCAL_GL_NONE) {
      /* Unable to find a texture target that works with FBOs and NPOT textures */
      return false;
    }
  } else {
    // not trying to work around driver bugs, so TEXTURE_2D should just work
    mFBOTextureTarget = LOCAL_GL_TEXTURE_2D;
  }

  // back to default framebuffer, to avoid confusion
  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  if (mFBOTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB) {
    /* If we're using TEXTURE_RECTANGLE, then we must have the ARB
     * extension -- the EXT variant does not provide support for
     * texture rectangle access inside GLSL (sampler2DRect,
     * texture2DRect).
     */
    if (!mGLContext->IsExtensionSupported(gl::GLContext::ARB_texture_rectangle))
      return false;
  }

  // If we're double-buffered, we don't need this fbo anymore.
  mGLContext->fDeleteFramebuffers(1, &mBackBufferFBO);
  mBackBufferFBO = 0;

  /* Create a simple quad VBO */

  mGLContext->fGenBuffers(1, &mQuadVBO);
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);

  GLfloat vertices[] = {
    /* First quad vertices */
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    /* Then quad texcoords */
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    /* Then flipped quad texcoords */
    0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
  };
  mGLContext->fBufferData(LOCAL_GL_ARRAY_BUFFER, sizeof(vertices), vertices, LOCAL_GL_STATIC_DRAW);
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  nsCOMPtr<nsIConsoleService>
    console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));

  if (console) {
    nsString msg;
    msg +=
      NS_LITERAL_STRING("OpenGL LayerManager Initialized Succesfully.\nVersion: ");
    msg += NS_ConvertUTF8toUTF16(
      nsDependentCString((const char*)mGLContext->fGetString(LOCAL_GL_VERSION)));
    msg += NS_LITERAL_STRING("\nVendor: ");
    msg += NS_ConvertUTF8toUTF16(
      nsDependentCString((const char*)mGLContext->fGetString(LOCAL_GL_VENDOR)));
    msg += NS_LITERAL_STRING("\nRenderer: ");
    msg += NS_ConvertUTF8toUTF16(
      nsDependentCString((const char*)mGLContext->fGetString(LOCAL_GL_RENDERER)));
    msg += NS_LITERAL_STRING("\nFBO Texture Target: ");
    if (mFBOTextureTarget == LOCAL_GL_TEXTURE_2D)
      msg += NS_LITERAL_STRING("TEXTURE_2D");
    else
      msg += NS_LITERAL_STRING("TEXTURE_RECTANGLE");
    console->LogStringMessage(msg.get());
  }

  if (NS_IsMainThread()) {
    Preferences::AddBoolVarCache(&sDrawFPS, "layers.acceleration.draw-fps");
  } else {
    // We have to dispatch an event to the main thread to read the pref.
    class ReadDrawFPSPref : public nsRunnable {
    public:
      NS_IMETHOD Run()
      {
        Preferences::AddBoolVarCache(&sDrawFPS, "layers.acceleration.draw-fps");
        return NS_OK;
      }
    };
    NS_DispatchToMainThread(new ReadDrawFPSPref());
  }

  reporter.SetSuccessful();
  return true;
}

// |aTexCoordRect| is the rectangle from the texture that we want to
// draw using the given program.  The program already has a necessary
// offset and scale, so the geometry that needs to be drawn is a unit
// square from 0,0 to 1,1.
//
// |aTexSize| is the actual size of the texture, as it can be larger
// than the rectangle given by |aTexCoordRect|.
void 
CompositorOGL::BindAndDrawQuadWithTextureRect(ShaderProgramOGL *aProg,
                                                const gfx::IntRect& aTexCoordRect,
                                                const gfx::IntSize& aTexSize,
                                                GLenum aWrapMode /* = LOCAL_GL_REPEAT */,
                                                bool aFlipped /* = false */)
{
  NS_ASSERTION(aProg->HasInitialized(), "Shader program not correctly initialized");
  GLuint vertAttribIndex =
    aProg->AttribLocation(ShaderProgramOGL::VertexCoordAttrib);
  GLuint texCoordAttribIndex =
    aProg->AttribLocation(ShaderProgramOGL::TexCoordAttrib);
  NS_ASSERTION(texCoordAttribIndex != GLuint(-1), "no texture coords?");

  // clear any bound VBO so that glVertexAttribPointer() goes back to
  // "pointer mode"
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // Given what we know about these textures and coordinates, we can
  // compute fmod(t, 1.0f) to get the same texture coordinate out.  If
  // the texCoordRect dimension is < 0 or > width/height, then we have
  // wraparound that we need to deal with by drawing multiple quads,
  // because we can't rely on full non-power-of-two texture support
  // (which is required for the REPEAT wrap mode).

  GLContext::RectTriangles rects;

  gfx::IntSize realTexSize = aTexSize;
  if (!mGLContext->CanUploadNonPowerOfTwo()) {
    realTexSize = gfx::IntSize(gfx::NextPowerOfTwo(aTexSize.width),
                               gfx::NextPowerOfTwo(aTexSize.height));
  }

  if (aWrapMode == LOCAL_GL_REPEAT) {
    rects.addRect(/* dest rectangle */
                  0.0f, 0.0f, 1.0f, 1.0f,
                  /* tex coords */
                  aTexCoordRect.x / GLfloat(realTexSize.width),
                  aTexCoordRect.y / GLfloat(realTexSize.height),
                  aTexCoordRect.XMost() / GLfloat(realTexSize.width),
                  aTexCoordRect.YMost() / GLfloat(realTexSize.height),
                  aFlipped);
  } else {
    nsIntRect tcRect(aTexCoordRect.x, aTexCoordRect.y,
                     aTexCoordRect.width, aTexCoordRect.height);
    GLContext::DecomposeIntoNoRepeatTriangles(tcRect,
                                              nsIntSize(realTexSize.width, realTexSize.height),
                                              rects, aFlipped);
  }

  mGLContext->fVertexAttribPointer(vertAttribIndex, 2,
                                   LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                   rects.vertexPointer());

  mGLContext->fVertexAttribPointer(texCoordAttribIndex, 2,
                                   LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                   rects.texCoordPointer());

  {
    mGLContext->fEnableVertexAttribArray(texCoordAttribIndex);
    {
      mGLContext->fEnableVertexAttribArray(vertAttribIndex);

      mGLContext->fDrawArrays(LOCAL_GL_TRIANGLES, 0, rects.elements());

      mGLContext->fDisableVertexAttribArray(vertAttribIndex);
    }
    mGLContext->fDisableVertexAttribArray(texCoordAttribIndex);
  }
}

void
CompositorOGL::SetupPipeline(int aWidth, int aHeight, const gfxMatrix& aWorldTransform)
{
  // Set the viewport correctly.
  mGLContext->fViewport(0, 0, aWidth, aHeight);

  // We flip the view matrix around so that everything is right-side up; we're
  // drawing directly into the window's back buffer, so this keeps things
  // looking correct.
  // XXX: We keep track of whether the window size changed, so we could skip
  // this update if it hadn't changed since the last call. We will need to
  // track changes to aTransformPolicy and mWorldMatrix for this to work
  // though.

  // Matrix to transform (0, 0, aWidth, aHeight) to viewport space (-1.0, 1.0,
  // 2, 2) and flip the contents.
  gfxMatrix viewMatrix;
  viewMatrix.Translate(-gfxPoint(1.0, -1.0));
  viewMatrix.Scale(2.0f / float(aWidth), 2.0f / float(aHeight));
  viewMatrix.Scale(1.0f, -1.0f);

  viewMatrix = aWorldTransform * viewMatrix;

  gfx3DMatrix matrix3d = gfx3DMatrix::From2D(viewMatrix);
  matrix3d._33 = 0.0f;

  SetLayerProgramProjectionMatrix(matrix3d);
}

void
CompositorOGL::SetLayerProgramProjectionMatrix(const gfx3DMatrix& aMatrix)
{
  for (unsigned int i = 0; i < mPrograms.Length(); ++i) {
    for (PRUint32 mask = MaskNone; mask < NumMaskTypes; ++mask) {
      if (mPrograms[i].mVariations[mask]) {
        mPrograms[i].mVariations[mask]->CheckAndSetProjectionMatrix(aMatrix);
      }
    }
  }
}

TemporaryRef<Texture>
CompositorOGL::CreateTextureForData(const gfx::IntSize &aSize, PRInt8 *aData, PRUint32 aStride,
                                    TextureFormat aFormat)
{
  RefPtr<TextureOGL> texture = new TextureOGL();
  texture->mCompositorOGL = this;
  texture->mSize = aSize;
  mGLContext->fGenTextures(1, &(texture->mTextureHandle)); //TODO[nrc]
  mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, texture->GetTextureHandle()); 
  mGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                             LOCAL_GL_LINEAR);
  mGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                             LOCAL_GL_LINEAR);
  mGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S,
                             LOCAL_GL_CLAMP_TO_EDGE);
  mGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T,
                             LOCAL_GL_CLAMP_TO_EDGE);


  switch (aFormat) {
    case TEXTUREFORMAT_BGRX32:
    case TEXTUREFORMAT_BGRA32:
      texture->mFormat = LOCAL_GL_RGBA;
      texture->mType = LOCAL_GL_UNSIGNED_BYTE;
      texture->mPixelSize = 4;
      break;
    case TEXTUREFORMAT_BGR16:
      texture->mFormat = LOCAL_GL_RGB;
      texture->mType = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
      texture->mPixelSize = 2;
      break;
    case TEXTUREFORMAT_Y8:
      texture->mFormat = LOCAL_GL_LUMINANCE;
      texture->mType = LOCAL_GL_UNSIGNED_BYTE;
      texture->mPixelSize = 1;
      break;
    default:
      MOZ_NOT_REACHED("aFormat is not a valid TextureFormat");
  }

  if (mGLContext->IsGLES2()) {
    texture->mInternalFormat = texture->mFormat;
  } else {
    texture->mInternalFormat = LOCAL_GL_RGBA;
  }

  mGLContext->TexImage2D(LOCAL_GL_TEXTURE_2D, 0, texture->mInternalFormat,
                         aSize.width, aSize.height, aStride, texture->mPixelSize,
                         0, texture->mFormat, texture->mType, aData);

  return texture.forget();
}

TemporaryRef<Surface>
CompositorOGL::CreateSurface(const gfx::IntRect &aRect, SurfaceInitMode aInit)
{
  RefPtr<SurfaceOGL> surface = new SurfaceOGL();
  CreateFBOWithTexture(aRect, aInit, 0, &(surface->mFBO), &(surface->mTexture));
  return surface.forget();
}

TemporaryRef<Surface>
CompositorOGL::CreateSurfaceFromSurface(const gfx::IntRect &aRect, const Surface *aSource)
{
  RefPtr<SurfaceOGL> surface = new SurfaceOGL();
  const SurfaceOGL* sourceSurface = static_cast<const SurfaceOGL*>(aSource);
  CreateFBOWithTexture(aRect, INIT_MODE_COPY, sourceSurface->mFBO,
                       &(surface->mFBO), &(surface->mTexture));
  return surface.forget();
}

void
CompositorOGL::SetSurfaceTarget(Surface *aSurface)
{
  SurfaceOGL* surface = static_cast<SurfaceOGL*>(aSurface);
  if (mBoundFBO != surface->mFBO) {
    mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, surface->mFBO);
    mBoundFBO = surface->mFBO;
  }
}

void
CompositorOGL::RemoveSurfaceTarget()
{
  if (mBoundFBO != 0) {
    mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
    mBoundFBO = 0;
  }
}

static GLenum
GetFrameBufferInternalFormat(GLContext* gl,
                             GLuint aFrameBuffer,
                             nsIWidget* aWidget)
{
  if (aFrameBuffer == 0) { // default framebuffer
    return aWidget->GetGLFrameBufferFormat();
  }
  return LOCAL_GL_RGBA;
}

void
CompositorOGL::CreateFBOWithTexture(const gfx::IntRect& aRect, SurfaceInitMode aInit,
                                    GLuint aSourceFrameBuffer,
                                    GLuint *aFBO, GLuint *aTexture)
{
  GLuint tex, fbo;

  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGLContext->fGenTextures(1, &tex);
  mGLContext->fBindTexture(mFBOTextureTarget, tex);

  if (aInit == INIT_MODE_COPY) {

    if (mBoundFBO != aSourceFrameBuffer) {
      mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, aSourceFrameBuffer);
    }

    // We're going to create an RGBA temporary fbo.  But to
    // CopyTexImage() from the current framebuffer, the framebuffer's
    // format has to be compatible with the new texture's.  So we
    // check the format of the framebuffer here and take a slow path
    // if it's incompatible.
    GLenum format =
      GetFrameBufferInternalFormat(gl(), aSourceFrameBuffer, mWidget);

    bool isFormatCompatibleWithRGBA
        = gl()->IsGLES2() ? (format == LOCAL_GL_RGBA)
                          : true;

    if (isFormatCompatibleWithRGBA) {
      mGLContext->fCopyTexImage2D(mFBOTextureTarget,
                                  0,
                                  LOCAL_GL_RGBA,
                                  aRect.x, aRect.y,
                                  aRect.width, aRect.height,
                                  0);
    } else {
      // Curses, incompatible formats.  Take a slow path.
      //
      // XXX Technically CopyTexSubImage2D also has the requirement of
      // matching formats, but it doesn't seem to affect us in the
      // real world.
      mGLContext->fTexImage2D(mFBOTextureTarget,
                              0,
                              LOCAL_GL_RGBA,
                              aRect.width, aRect.height,
                              0,
                              LOCAL_GL_RGBA,
                              LOCAL_GL_UNSIGNED_BYTE,
                              NULL);
      mGLContext->fCopyTexSubImage2D(mFBOTextureTarget,
                                     0,    // level
                                     0, 0, // offset
                                     aRect.x, aRect.y,
                                     aRect.width, aRect.height);
    }
  } else {
    mGLContext->fTexImage2D(mFBOTextureTarget,
                            0,
                            LOCAL_GL_RGBA,
                            aRect.width, aRect.height,
                            0,
                            LOCAL_GL_RGBA,
                            LOCAL_GL_UNSIGNED_BYTE,
                            NULL);
  }
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_MIN_FILTER,
                             LOCAL_GL_LINEAR);
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_MAG_FILTER,
                             LOCAL_GL_LINEAR);
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_WRAP_S, 
                             LOCAL_GL_CLAMP_TO_EDGE);
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_WRAP_T, 
                             LOCAL_GL_CLAMP_TO_EDGE);
  mGLContext->fBindTexture(mFBOTextureTarget, 0);

  mGLContext->fGenFramebuffers(1, &fbo);
  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, fbo);
  mGLContext->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                    LOCAL_GL_COLOR_ATTACHMENT0,
                                    mFBOTextureTarget,
                                    tex,
                                    0);

  // Making this call to fCheckFramebufferStatus prevents a crash on
  // PowerVR. See bug 695246.
  GLenum result = mGLContext->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
  if (result != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
    nsCAutoString msg;
    msg.Append("Framebuffer not complete -- error 0x");
    msg.AppendInt(result, 16);
    msg.Append(", mFBOTextureTarget 0x");
    msg.AppendInt(mFBOTextureTarget, 16);
    msg.Append(", aRect.width ");
    msg.AppendInt(aRect.width);
    msg.Append(", aRect.height ");
    msg.AppendInt(aRect.height);
    NS_RUNTIMEABORT(msg.get());
  }

  SetupPipeline(aRect.width, aRect.height, gfxMatrix());
  mGLContext->fScissor(0, 0, aRect.width, aRect.height);

  if (aInit == INIT_MODE_CLEAR) {
    mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
    mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT);
  }

  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mBoundFBO);

  *aFBO = fbo;
  *aTexture = tex;
}

bool CompositorOGL::sDrawFPS = false;

/* This function tries to stick to portable C89 as much as possible
 * so that it can be easily copied into other applications */
void
CompositorOGL::FPSState::DrawFPS(GLContext* context, ShaderProgramOGL* copyprog)
{
  fcount++;

  int rate = 30;
  if (fcount >= rate) {
    TimeStamp now = TimeStamp::Now();
    TimeDuration duration = now - last;
    last = now;
    fps = rate / duration.ToSeconds() + .5;
    fcount = 0;
  }

  GLint viewport[4];
  context->fGetIntegerv(LOCAL_GL_VIEWPORT, viewport);

  static GLuint texture;
  if (!initialized) {
    // Bind the number of textures we need, in this case one.
    context->fGenTextures(1, &texture);
    context->fBindTexture(LOCAL_GL_TEXTURE_2D, texture);
    context->fTexParameteri(LOCAL_GL_TEXTURE_2D,LOCAL_GL_TEXTURE_MIN_FILTER,LOCAL_GL_NEAREST);
    context->fTexParameteri(LOCAL_GL_TEXTURE_2D,LOCAL_GL_TEXTURE_MAG_FILTER,LOCAL_GL_NEAREST);

    unsigned char text[] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0, 255, 255, 255,   0, 255, 255,   0,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255,   0, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0,
      0, 255,   0, 255,   0,   0, 255,   0,   0,   0,   0, 255,   0,   0,   0, 255,   0, 255,   0, 255,   0, 255,   0,   0,   0, 255,   0,   0,   0,   0,   0, 255,   0, 255,   0, 255,   0, 255,   0, 255,   0,
      0, 255,   0, 255,   0,   0, 255,   0,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0,   0,   0, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0,
      0, 255,   0, 255,   0,   0, 255,   0,   0, 255,   0,   0,   0,   0,   0, 255,   0,   0,   0, 255,   0,   0,   0, 255,   0, 255,   0, 255,   0,   0,   0, 255,   0, 255,   0, 255,   0,   0,   0, 255,   0,
      0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0,   0,   0, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0,   0,   0, 255,   0, 255, 255, 255,   0,   0,   0, 255,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    };

    // convert from 8 bit to 32 bit so that don't have to write the text above out in 32 bit format
    // we rely on int being 32 bits
    unsigned int* buf = (unsigned int*)malloc(64 * 8 * 4);
    for (int i = 0; i < 7; i++) {
      for (int j = 0; j < 41; j++) {
        unsigned int purple = 0xfff000ff;
        unsigned int white  = 0xffffffff;
        buf[i * 64 + j] = (text[i * 41 + j] == 0) ? purple : white;
      }
    }
    context->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA, 64, 8, 0, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, buf);
    free(buf);
    initialized = true;
  }

  struct Vertex2D {
    float x,y;
  };
  const Vertex2D vertices[] = {
    { -1.0f, 1.0f - 42.f / viewport[3] },
    { -1.0f, 1.0f},
    { -1.0f + 22.f / viewport[2], 1.0f - 42.f / viewport[3] },
    { -1.0f + 22.f / viewport[2], 1.0f },

    {  -1.0f + 22.f / viewport[2], 1.0f - 42.f / viewport[3] },
    {  -1.0f + 22.f / viewport[2], 1.0f },
    {  -1.0f + 44.f / viewport[2], 1.0f - 42.f / viewport[3] },
    {  -1.0f + 44.f / viewport[2], 1.0f },

    { -1.0f + 44.f / viewport[2], 1.0f - 42.f / viewport[3] },
    { -1.0f + 44.f / viewport[2], 1.0f },
    { -1.0f + 66.f / viewport[2], 1.0f - 42.f / viewport[3] },
    { -1.0f + 66.f / viewport[2], 1.0f }
  };

  int v1   = fps % 10;
  int v10  = (fps % 100) / 10;
  int v100 = (fps % 1000) / 100;

  // Feel free to comment these texture coordinates out and use one
  // of the ones below instead, or play around with your own values.
  const GLfloat texCoords[] = {
    (v100 * 4.f) / 64, 7.f / 8,
    (v100 * 4.f) / 64, 0.0f,
    (v100 * 4.f + 4) / 64, 7.f / 8,
    (v100 * 4.f + 4) / 64, 0.0f,

    (v10 * 4.f) / 64, 7.f / 8,
    (v10 * 4.f) / 64, 0.0f,
    (v10 * 4.f + 4) / 64, 7.f / 8,
    (v10 * 4.f + 4) / 64, 0.0f,

    (v1 * 4.f) / 64, 7.f / 8,
    (v1 * 4.f) / 64, 0.0f,
    (v1 * 4.f + 4) / 64, 7.f / 8,
    (v1 * 4.f + 4) / 64, 0.0f,
  };

  // Turn necessary features on
  context->fEnable(LOCAL_GL_BLEND);
  context->fBlendFunc(LOCAL_GL_ONE, LOCAL_GL_SRC_COLOR);

  context->fActiveTexture(LOCAL_GL_TEXTURE0);
  context->fBindTexture(LOCAL_GL_TEXTURE_2D, texture);

  copyprog->Activate();
  copyprog->SetTextureUnit(0);

  // we're going to use client-side vertex arrays for this.
  context->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // "COPY"
  context->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ZERO,
                              LOCAL_GL_ONE, LOCAL_GL_ZERO);

  // enable our vertex attribs; we'll call glVertexPointer below
  // to fill with the correct data.
  GLint vcattr = copyprog->AttribLocation(ShaderProgramOGL::VertexCoordAttrib);
  GLint tcattr = copyprog->AttribLocation(ShaderProgramOGL::TexCoordAttrib);

  context->fEnableVertexAttribArray(vcattr);
  context->fEnableVertexAttribArray(tcattr);

  context->fVertexAttribPointer(vcattr,
                                2, LOCAL_GL_FLOAT,
                                LOCAL_GL_FALSE,
                                0, vertices);

  context->fVertexAttribPointer(tcattr,
                                2, LOCAL_GL_FLOAT,
                                LOCAL_GL_FALSE,
                                0, texCoords);

  context->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 12);
}

void
CompositorOGL::BeginFrame(const gfx::Rect *aClipRect, const gfxMatrix& aTransform)
{
  mFrameInProgress = true;
  nsIntRect rect;
  if (mIsRenderingToEGLSurface) {
    rect = nsIntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
  } else {
    mWidget->GetClientBounds(rect);
  }

  //TODO[nrc] sort this rect bullshit
  gfxRect grect(rect);
  grect = aTransform.TransformBounds(grect);
  rect.SetRect(grect.X(), grect.Y(), grect.Width(), grect.Height());

  GLint width = rect.width;
  GLint height = rect.height;

  // We can't draw anything to something with no area
  // so just return
  if (width == 0 || height == 0)
    return;

  // If the widget size changed, we have to force a MakeCurrent
  // to make sure that GL sees the updated widget size.
  if (mWidgetSize.width != width ||
      mWidgetSize.height != height)
  {
    MakeCurrent(true);

    mWidgetSize.width = width;
    mWidgetSize.height = height;
  } else {
    MakeCurrent();
  }

#if MOZ_WIDGET_ANDROID
  TexturePoolOGL::Fill(gl());
#endif

  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
  SetupPipeline(width, height, aTransform);

  // Default blend function implements "OVER"
  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  if (!aClipRect) {
    mGLContext->fScissor(0, 0, width, height);
  }

  mGLContext->fEnable(LOCAL_GL_SCISSOR_TEST);

  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);

  // Allow widget to render a custom background.
  //TODO[nrc] DrawWindowUnderlay doesn't use its params, can we change its interface?
  mWidget->DrawWindowUnderlay(nullptr, nsIntRect());
}

void
CompositorOGL::DrawQuad(const gfx::Rect &aRect, const gfx::Rect *aSourceRect,
                        const gfx::Rect *aClipRect, const EffectChain &aEffectChain,
                        gfx::Float aOpacity, const gfx::Matrix4x4 &aTransform,
                        const gfx::Point &aOffset)
{
  if (!mFrameInProgress) {
    BeginFrame(aClipRect, gfxMatrix());
  }

  gfx::IntRect intSourceRect;
  if (aSourceRect) {
    aSourceRect->ToIntRect(&intSourceRect);
  }

  gfx::IntRect intClipRect;
  if (aClipRect) {
    aClipRect->ToIntRect(&intClipRect);
    mGLContext->fScissor(intClipRect.x, intClipRect.y,
                         intClipRect.width, intClipRect.height);
  }

  MaskType maskType;
  EffectMask* effectMask;
  RefPtr<ATextureOGL> textureMask;
  if (aEffectChain.mEffects[EFFECT_MASK]) {
    effectMask = static_cast<EffectMask*>(aEffectChain.mEffects[EFFECT_MASK]);
    textureMask = static_cast<ATextureOGL*>(effectMask->mMaskTexture.get());
    if (effectMask->mMaskTransform.Is2D()) {
      maskType = Mask2d;
    } else {
      maskType = Mask3d;
    }
  } else {
    maskType = MaskNone;
  }

  if (aEffectChain.mEffects[EFFECT_SOLID_COLOR]) {
    EffectSolidColor* effectSolidColor =
      static_cast<EffectSolidColor*>(aEffectChain.mEffects[EFFECT_SOLID_COLOR]);

    gfx::Color color = effectSolidColor->mColor;

    /* Multiply color by the layer opacity, as the shader
     * ignores layer opacity and expects a final color to
     * write to the color buffer.  This saves a needless
     * multiply in the fragment shader.
     */
    gfx::Float opacity = aOpacity * color.a;
    color.r *= opacity;
    color.g *= opacity;
    color.b *= opacity;
    color.a = opacity;
    ShaderProgramOGL *program = GetProgram(gl::ColorLayerProgramType, maskType);
    program->Activate();
    program->SetLayerQuadRect(aRect);
    program->SetRenderColor(effectSolidColor->mColor);
    program->SetLayerTransform(aTransform);
    program->SetRenderOffset(aOffset.x, aOffset.y);
    if (maskType != MaskNone) {
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, textureMask->GetTextureHandle());
      program->SetMaskTextureUnit(0);
      program->SetMaskLayerTransform(effectMask->mMaskTransform);
    }
    BindAndDrawQuad(program);

  } else if (aEffectChain.mEffects[EFFECT_BGRA] || aEffectChain.mEffects[EFFECT_BGRX]) {
    RefPtr<ATextureOGL> texture;
    bool premultiplied;
    gfxPattern::GraphicsFilter filter;
    ShaderProgramOGL *program;
    bool flipped;

    if (aEffectChain.mEffects[EFFECT_BGRA]) {
      EffectBGRA* effectBGRA =
        static_cast<EffectBGRA*>(aEffectChain.mEffects[EFFECT_BGRA]);
      texture = static_cast<ATextureOGL*>(effectBGRA->mBGRATexture.get());
      premultiplied = effectBGRA->mPremultiplied;
      flipped = effectBGRA->mFlipped;
      filter = gfx::ThebesFilter(effectBGRA->mFilter);
      program = GetProgram(gl::BGRALayerProgramType, maskType);
    } else if (aEffectChain.mEffects[EFFECT_BGRX]) {
      EffectBGRX* effectBGRX =
        static_cast<EffectBGRX*>(aEffectChain.mEffects[EFFECT_BGRX]);
      texture = static_cast<ATextureOGL*>(effectBGRX->mBGRXTexture.get());
      premultiplied = effectBGRX->mPremultiplied;
      flipped = effectBGRX->mFlipped;
      filter = gfx::ThebesFilter(effectBGRX->mFilter);
      program = GetProgram(gl::BGRXLayerProgramType, maskType);
    }

    if (!premultiplied) {
      mGLContext->fBlendFuncSeparate(LOCAL_GL_SRC_ALPHA, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                     LOCAL_GL_ONE, LOCAL_GL_ONE);
    }

    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, texture->GetTextureHandle());

    mGLContext->ApplyFilterToBoundTexture(filter);

    program->Activate();
    program->SetTextureUnit(0);
    program->SetLayerOpacity(aOpacity);
    program->SetLayerTransform(aTransform);
    program->SetRenderOffset(aOffset.x, aOffset.y);
    program->SetLayerQuadRect(aRect);
    if (maskType != MaskNone) {
      mGLContext->fActiveTexture(LOCAL_GL_TEXTURE1);
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, textureMask->GetTextureHandle());
      program->SetMaskTextureUnit(1);
      program->SetMaskLayerTransform(effectMask->mMaskTransform);
    }

    //TODO[nrc] texture fields
    //BindAndDrawQuadWithTextureRect(program, intSourceRect, texture->mSize, texture->mWrapMode, flipped);

    if (!premultiplied) {
      mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                     LOCAL_GL_ONE, LOCAL_GL_ONE);
    }


  } else if (aEffectChain.mEffects[EFFECT_RGBA] || aEffectChain.mEffects[EFFECT_RGBX] ||
             aEffectChain.mEffects[EFFECT_RGBA_EXTERNAL]) {
    RefPtr<ATextureOGL> texture;
    bool premultiplied;
    gfxPattern::GraphicsFilter filter;
    ShaderProgramOGL *program;
    bool flipped;

    if (aEffectChain.mEffects[EFFECT_RGBA]) {
      EffectRGBA* effectRGBA =
        static_cast<EffectRGBA*>(aEffectChain.mEffects[EFFECT_RGBA]);
      texture = static_cast<ATextureOGL*>(effectRGBA->mRGBATexture.get());
      premultiplied = effectRGBA->mPremultiplied;
      flipped = effectRGBA->mFlipped;
      filter = gfx::ThebesFilter(effectRGBA->mFilter);
      program = GetProgram(gl::RGBALayerProgramType, maskType);
    } else if (aEffectChain.mEffects[EFFECT_RGBX]) {
      EffectRGBX* effectRGBX =
        static_cast<EffectRGBX*>(aEffectChain.mEffects[EFFECT_RGBX]);
      texture = static_cast<ATextureOGL*>(effectRGBX->mRGBXTexture.get());
      premultiplied = effectRGBX->mPremultiplied;
      flipped = effectRGBX->mFlipped;
      filter = gfx::ThebesFilter(effectRGBX->mFilter);
      program = GetProgram(gl::RGBXLayerProgramType, maskType);
    } else {
      EffectRGBAExternal* effectRGBAExternal =
        static_cast<EffectRGBAExternal*>(aEffectChain.mEffects[EFFECT_RGBA_EXTERNAL]);
      texture = static_cast<ATextureOGL*>(effectRGBAExternal->mRGBATexture.get());
      premultiplied = effectRGBAExternal->mPremultiplied;
      flipped = effectRGBAExternal->mFlipped;
      filter = gfx::ThebesFilter(effectRGBAExternal->mFilter);
      program = GetProgram(gl::RGBALayerExternalProgramType, maskType);
      program->SetTextureTransform(effectRGBAExternal->mTextureTransform);
    }

    if (!premultiplied) {
      mGLContext->fBlendFuncSeparate(LOCAL_GL_SRC_ALPHA, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                     LOCAL_GL_ONE, LOCAL_GL_ONE);
    }

    if (aEffectChain.mEffects[EFFECT_RGBA_EXTERNAL]) {
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, texture->GetTextureHandle());
    } else {
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, texture->GetTextureHandle());
    }

    mGLContext->ApplyFilterToBoundTexture(filter);

    program->Activate();
    program->SetTextureUnit(0);
    program->SetLayerOpacity(aOpacity);
    program->SetLayerTransform(aTransform);
    program->SetRenderOffset(aOffset.x, aOffset.y);
    program->SetLayerQuadRect(aRect);
    if (maskType != MaskNone) {
      mGLContext->fActiveTexture(LOCAL_GL_TEXTURE1);
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, textureMask->GetTextureHandle());
      program->SetMaskTextureUnit(1);
      program->SetMaskLayerTransform(effectMask->mMaskTransform);
    }
    BindAndDrawQuad(program, flipped);

    if (!premultiplied) {
      mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                     LOCAL_GL_ONE, LOCAL_GL_ONE);
    }

    if (aEffectChain.mEffects[EFFECT_RGBA_EXTERNAL]) {
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, 0);
    }
  } else if (aEffectChain.mEffects[EFFECT_YCBCR]) {
    EffectYCbCr* effectYCbCr =
      static_cast<EffectYCbCr*>(aEffectChain.mEffects[EFFECT_YCBCR]);
    RefPtr<ATextureOGL> textureY = static_cast<ATextureOGL*>(effectYCbCr->mY.get());
    RefPtr<ATextureOGL> textureCb = static_cast<ATextureOGL*>(effectYCbCr->mCb.get());
    RefPtr<ATextureOGL> textureCr = static_cast<ATextureOGL*>(effectYCbCr->mCr.get());
    gfxPattern::GraphicsFilter filter = gfx::ThebesFilter(effectYCbCr->mFilter);

    mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, textureY->GetTextureHandle());
    mGLContext->ApplyFilterToBoundTexture(filter);
    mGLContext->fActiveTexture(LOCAL_GL_TEXTURE1);
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, textureCb->GetTextureHandle());
    mGLContext->ApplyFilterToBoundTexture(filter);
    mGLContext->fActiveTexture(LOCAL_GL_TEXTURE2);
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, textureCr->GetTextureHandle());
    mGLContext->ApplyFilterToBoundTexture(filter);

    ShaderProgramOGL *program = GetProgram(YCbCrLayerProgramType, maskType);

    program->Activate();
    program->SetYCbCrTextureUnits(0, 1, 2);
    program->SetLayerOpacity(aOpacity);
    program->SetLayerTransform(aTransform);
    program->SetRenderOffset(aOffset.x, aOffset.y);
    program->SetLayerQuadRect(aRect);
    if (maskType != MaskNone) {
      mGLContext->fActiveTexture(LOCAL_GL_TEXTURE3);
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, textureMask->GetTextureHandle());
      program->SetMaskTextureUnit(3);
      program->SetMaskLayerTransform(effectMask->mMaskTransform);
    }
    //TODO[nrc] texture fields
    //BindAndDrawQuadWithTextureRect(program, intSourceRect, textureY->mSize);

  } else if (aEffectChain.mEffects[EFFECT_COMPONENT_ALPHA]) {
    EffectComponentAlpha* effectComponentAlpha =
      static_cast<EffectComponentAlpha*>(aEffectChain.mEffects[EFFECT_COMPONENT_ALPHA]);
    RefPtr<ATextureOGL> textureOnWhite =
      static_cast<ATextureOGL*>(effectComponentAlpha->mOnWhite.get());
    RefPtr<ATextureOGL> textureOnBlack =
      static_cast<ATextureOGL*>(effectComponentAlpha->mOnBlack.get());

    for (PRInt32 pass = 1; pass <=2; ++pass) {
      ShaderProgramOGL* program;
      if (pass == 1) {
        program = GetProgram(gl::ComponentAlphaPass1ProgramType, maskType);
        gl()->fBlendFuncSeparate(LOCAL_GL_ZERO, LOCAL_GL_ONE_MINUS_SRC_COLOR,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
      } else {
        program = GetProgram(gl::ComponentAlphaPass2ProgramType, maskType);
        gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
      }

      mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, textureOnBlack->GetTextureHandle());
      mGLContext->fActiveTexture(LOCAL_GL_TEXTURE1);
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, textureOnWhite->GetTextureHandle());

      program->Activate();
      program->SetBlackTextureUnit(0);
      program->SetWhiteTextureUnit(1);
      program->SetLayerOpacity(aOpacity);
      program->SetLayerTransform(aTransform);
      program->SetRenderOffset(aOffset.x, aOffset.y);
      program->SetLayerQuadRect(aRect);
      if (maskType != MaskNone) {
        mGLContext->fActiveTexture(LOCAL_GL_TEXTURE2);
        mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, textureMask->GetTextureHandle());
        program->SetMaskTextureUnit(2);
        program->SetMaskLayerTransform(effectMask->mMaskTransform);
      }

      //TODO[nrc] texture fields
      //BindAndDrawQuadWithTextureRect(program, intSourceRect, textureOnBlack->mSize, textureOnBlack->mWrapMode);

      mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                     LOCAL_GL_ONE, LOCAL_GL_ONE);
    }
  } else if (aEffectChain.mEffects[EFFECT_SURFACE]) {
    EffectSurface* effectSurface =
      static_cast<EffectSurface*>(aEffectChain.mEffects[EFFECT_SURFACE]);
    RefPtr<SurfaceOGL> surface = static_cast<SurfaceOGL*>(effectSurface->mSurface.get());

    ShaderProgramOGL *program = GetProgram(GetFBOLayerProgramType(), maskType);

    mGLContext->fBindTexture(mFBOTextureTarget, surface->mTexture);

    program->Activate();
    program->SetTextureUnit(0);
    program->SetLayerOpacity(aOpacity);
    program->SetLayerTransform(aTransform);
    program->SetRenderOffset(aOffset.x, aOffset.y);
    program->SetLayerQuadRect(aRect);
    if (maskType != MaskNone) {
      mGLContext->fActiveTexture(LOCAL_GL_TEXTURE1);
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, textureMask->GetTextureHandle());
      program->SetMaskTextureUnit(1);
      program->SetMaskLayerTransform(effectMask->mMaskTransform);
    }
    if (program->GetTexCoordMultiplierUniformLocation() != -1) {
      // 2DRect case, get the multiplier right for a sampler2DRect
      program->SetTexCoordMultiplier(aRect.width, aRect.height);
    }

    // Drawing is always flipped, but when copying between surfaces we want to avoid
    // this. Pass true for the flip parameter to introduce a second flip
    // that cancels the other one out.
    BindAndDrawQuad(program, true);
  }

  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
}

void
CompositorOGL::EndFrame()
{
  // Allow widget to render a custom foreground.
  //TODO[nrc] DrawWindowOverlay does not use its params, can we change its interface?
  mWidget->DrawWindowOverlay(nullptr, nsIntRect());

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    nsIntRect rect;
    if (mIsRenderingToEGLSurface) {
      rect = nsIntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
    } else {
      mWidget->GetBounds(rect);
    }
    nsRefPtr<gfxASurface> surf = gfxPlatform::GetPlatform()->CreateOffscreenSurface(rect.Size(), gfxASurface::CONTENT_COLOR_ALPHA);
    nsRefPtr<gfxContext> ctx = new gfxContext(surf);
    CopyToTarget(ctx);

    WriteSnapshotToDumpFile(this, surf);
  }
#endif

  if (mTarget) {
    CopyToTarget(mTarget);
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    mFrameInProgress = false;
    return;
  }

  if (sDrawFPS) {
    mFPS.DrawFPS(mGLContext, GetProgram(Copy2DProgramType));
  }

  mGLContext->SwapBuffers();
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
  mFrameInProgress = false;
}


void
CompositorOGL::CopyToTarget(gfxContext *aTarget)
{
  nsIntRect rect;
  if (mIsRenderingToEGLSurface) {
    rect = nsIntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
  } else {
    mWidget->GetBounds(rect);
  }
  GLint width = rect.width;
  GLint height = rect.height;

  if ((PRInt64(width) * PRInt64(height) * PRInt64(4)) > PR_INT32_MAX) {
    NS_ERROR("Widget size too big - integer overflow!");
    return;
  }

  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(gfxIntSize(width, height),
                        gfxASurface::ImageFormatARGB32);

  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  if (!mGLContext->IsGLES2()) {
    // GLES2 promises that binding to any custom FBO will attach
    // to GL_COLOR_ATTACHMENT0 attachment point.
    mGLContext->fReadBuffer(LOCAL_GL_BACK);
  }

  NS_ASSERTION(imageSurface->Stride() == width * 4,
               "Image Surfaces being created with weird stride!");

  mGLContext->ReadPixelsIntoImageSurface(0, 0, width, height, imageSurface);

  aTarget->SetOperator(gfxContext::OPERATOR_SOURCE);
  aTarget->Scale(1.0, -1.0);
  aTarget->Translate(-gfxPoint(0.0, height));
  aTarget->SetSource(imageSurface);
  aTarget->Paint();
}


} /* layers */
} /* mozilla */
