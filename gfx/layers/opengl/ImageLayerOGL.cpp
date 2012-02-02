/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.org>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "gfxSharedImageSurface.h"

#include "ImageLayerOGL.h"
#include "gfxImageSurface.h"
#include "yuv_convert.h"
#include "GLContextProvider.h"
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
# include "GLXLibrary.h"
# include "mozilla/X11Util.h"
#endif

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

/**
 * This is an event used to unref a GLContext on the main thread and
 * optionally delete a texture associated with that context.
 */
class TextureDeleter : public nsRunnable {
public:
  TextureDeleter(already_AddRefed<GLContext> aContext,
                 GLuint aTexture)
      : mContext(aContext), mTexture(aTexture)
  {
    NS_ASSERTION(aTexture, "TextureDeleter instantiated with nothing to do");
  }

  NS_IMETHOD Run() {
    mContext->MakeCurrent();
    mContext->fDeleteTextures(1, &mTexture);

    // Ensure context is released on the main thread
    mContext = nsnull;
    return NS_OK;
  }

  nsRefPtr<GLContext> mContext;
  GLuint mTexture;
};

void
GLTexture::Allocate(GLContext *aContext)
{
  NS_ASSERTION(aContext->IsGlobalSharedContext() ||
               NS_IsMainThread(), "Can only allocate texture on main thread or with cx sharing");

  Release();

  mContext = aContext;

  mContext->MakeCurrent();
  mContext->fGenTextures(1, &mTexture);
}

void
GLTexture::TakeFrom(GLTexture *aOther)
{
  Release();

  mContext = aOther->mContext.forget();
  mTexture = aOther->mTexture;
  aOther->mTexture = 0;
}

void
GLTexture::Release()
{
  if (!mContext) {
    NS_ASSERTION(!mTexture, "Can't delete texture without a context");
    return;
  }

  if (mContext->IsDestroyed() && !mContext->IsGlobalSharedContext()) {
    mContext = mContext->GetSharedContext();
    if (!mContext) {
      NS_ASSERTION(!mTexture, 
                   "Context has been destroyed and couldn't find a shared context!");
      return;
    }
  }

  if (mTexture) {
    if (NS_IsMainThread() || mContext->IsGlobalSharedContext()) {
      mContext->MakeCurrent();
      mContext->fDeleteTextures(1, &mTexture);
    } else {
      nsCOMPtr<nsIRunnable> runnable =
        new TextureDeleter(mContext.forget(), mTexture);
      NS_DispatchToMainThread(runnable);
    }

    mTexture = 0;
  }

  mContext = nsnull;
}

TextureRecycleBin::TextureRecycleBin()
  : mLock("mozilla.layers.TextureRecycleBin.mLock")
{
}

void
TextureRecycleBin::RecycleTexture(GLTexture *aTexture, TextureType aType,
                           const gfxIntSize& aSize)
{
  MutexAutoLock lock(mLock);

  if (!aTexture->IsAllocated())
    return;

  if (!mRecycledTextures[aType].IsEmpty() && aSize != mRecycledTextureSizes[aType]) {
    mRecycledTextures[aType].Clear();
  }
  mRecycledTextureSizes[aType] = aSize;
  mRecycledTextures[aType].AppendElement()->TakeFrom(aTexture);
}

void
TextureRecycleBin::GetTexture(TextureType aType, const gfxIntSize& aSize,
                       GLContext *aContext, GLTexture *aOutTexture)
{
  MutexAutoLock lock(mLock);

  if (mRecycledTextures[aType].IsEmpty() || mRecycledTextureSizes[aType] != aSize) {
    aOutTexture->Allocate(aContext);
    return;
  }
  PRUint32 last = mRecycledTextures[aType].Length() - 1;
  aOutTexture->TakeFrom(&mRecycledTextures[aType].ElementAt(last));
  mRecycledTextures[aType].RemoveElementAt(last);
}

struct THEBES_API MacIOSurfaceImageOGLBackendData : public ImageBackendData
{
  GLTexture mTexture;
};

#ifdef XP_MACOSX
void
AllocateTextureIOSurface(MacIOSurfaceImage *aIOImage, mozilla::gl::GLContext* aGL)
{
  nsAutoPtr<MacIOSurfaceImageOGLBackendData> backendData(
    new MacIOSurfaceImageOGLBackendData);

  backendData->mTexture.Allocate(aGL);
  aGL->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, backendData->mTexture.GetTextureID());
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                     LOCAL_GL_TEXTURE_MIN_FILTER,
                     LOCAL_GL_NEAREST);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                     LOCAL_GL_TEXTURE_MAG_FILTER,
                     LOCAL_GL_NEAREST);

  void *nativeCtx = aGL->GetNativeData(GLContext::NativeGLContext);
  NSOpenGLContext* nsCtx = (NSOpenGLContext*)nativeCtx;

  aIOImage->GetIOSurface()->CGLTexImageIOSurface2D(nsCtx,
                                     LOCAL_GL_RGBA, LOCAL_GL_BGRA,
                                     LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV, 0);

  aGL->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);

  aIOImage->SetBackendData(LayerManager::LAYERS_OPENGL, backendData.forget());
}
#endif

Layer*
ImageLayerOGL::GetLayer()
{
  return this;
}

void
ImageLayerOGL::RenderLayer(int,
                           const nsIntPoint& aOffset)
{
  if (!GetContainer())
    return;

  mOGLManager->MakeCurrent();

  nsRefPtr<Image> image = GetContainer()->GetCurrentImage();
  if (!image) {
    return;
  }

  if (image->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImage *yuvImage =
      static_cast<PlanarYCbCrImage*>(image.get());

    if (!yuvImage->mBufferSize) {
      return;
    }

    if (!yuvImage->GetBackendData(LayerManager::LAYERS_OPENGL)) {
      AllocateTexturesYCbCr(yuvImage);
    }

    PlanarYCbCrOGLBackendData *data =
      static_cast<PlanarYCbCrOGLBackendData*>(yuvImage->GetBackendData(LayerManager::LAYERS_OPENGL));

    if (!data || data->mTextures->GetGLContext() != mOGLManager->glForResources()) {
      // XXX - Can this ever happen? If so I need to fix this!
      return;
    }

    gl()->MakeCurrent();
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, data->mTextures[0].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE1);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, data->mTextures[1].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE2);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, data->mTextures[2].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);
    
    YCbCrTextureLayerProgram *program = mOGLManager->GetYCbCrLayerProgram();

    program->Activate();
    program->SetLayerQuadRect(nsIntRect(0, 0,
                                        yuvImage->mSize.width,
                                        yuvImage->mSize.height));
    program->SetLayerTransform(GetEffectiveTransform());
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetRenderOffset(aOffset);
    program->SetYCbCrTextureUnits(0, 1, 2);

    mOGLManager->BindAndDrawQuadWithTextureRect(program,
                                                yuvImage->mData.GetPictureRect(),
                                                nsIntSize(yuvImage->mData.mYSize.width,
                                                          yuvImage->mData.mYSize.height));

    // We shouldn't need to do this, but do it anyway just in case
    // someone else forgets.
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  } else if (image->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImage *cairoImage =
      static_cast<CairoImage*>(image.get());

    if (!cairoImage->mSurface) {
      return;
    }

    if (!cairoImage->GetBackendData(LayerManager::LAYERS_OPENGL)) {
      AllocateTexturesCairo(cairoImage);
    }

    CairoOGLBackendData *data =
      static_cast<CairoOGLBackendData*>(cairoImage->GetBackendData(LayerManager::LAYERS_OPENGL));

    if (!data || data->mTexture.GetGLContext() != mOGLManager->glForResources()) {
      // XXX - Can this ever happen? If so I need to fix this!
      return;
    }

    data->SetTiling(mUseTileSourceRect);
    gl()->MakeCurrent();
    unsigned int iwidth  = cairoImage->mSize.width;
    unsigned int iheight = cairoImage->mSize.height;

    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, data->mTexture.GetTextureID());

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
    GLXPixmap pixmap;

    if (cairoImage->mSurface) {
        pixmap = sGLXLibrary.CreatePixmap(cairoImage->mSurface);
        NS_ASSERTION(pixmap, "Failed to create pixmap!");
        if (pixmap) {
            sGLXLibrary.BindTexImage(pixmap);
        }
    }
#endif

    ColorTextureLayerProgram *program = 
      mOGLManager->GetColorTextureLayerProgram(data->mLayerProgram);

    gl()->ApplyFilterToBoundTexture(mFilter);

    program->Activate();
    // The following uniform controls the scaling of the vertex coords.
    // Instead of setting the scale here and using coords in the range [0,1], we
    // set an identity transform and use pixel coordinates below
    program->SetLayerQuadRect(nsIntRect(0, 0, 1, 1));
    program->SetLayerTransform(GetEffectiveTransform());
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetRenderOffset(aOffset);
    program->SetTextureUnit(0);

    nsIntRect rect = GetVisibleRegion().GetBounds();

    bool tileIsWholeImage = (mTileSourceRect == nsIntRect(0, 0, iwidth, iheight)) 
                            || !mUseTileSourceRect;
    bool imageIsPowerOfTwo = ((iwidth  & (iwidth - 1)) == 0 &&
                              (iheight & (iheight - 1)) == 0);
    bool canDoNPOT = (
          gl()->IsExtensionSupported(GLContext::ARB_texture_non_power_of_two) ||
          gl()->IsExtensionSupported(GLContext::OES_texture_npot));

    GLContext::RectTriangles triangleBuffer;
    // do GL_REPEAT if we can - should be the fastest option.
    // draw a single rect for the whole region, a little overdraw
    // on the gpu should be faster than tesselating
    // maybe we can write a shader that can also handle texture subrects
    // and repeat?
    if (tileIsWholeImage && (imageIsPowerOfTwo || canDoNPOT)) {
        // we need to anchor the repeating texture appropriately
        // otherwise it will start from the region border instead
        // of the layer origin. This is the offset into the texture
        // that the region border represents
        float tex_offset_u = (float)(rect.x % iwidth) / iwidth;
        float tex_offset_v = (float)(rect.y % iheight) / iheight;
        triangleBuffer.addRect(rect.x, rect.y,
                               rect.x + rect.width, rect.y + rect.height,
                               tex_offset_u, tex_offset_v,
                               tex_offset_u + (float)rect.width / (float)iwidth,
                               tex_offset_v + (float)rect.height / (float)iheight);
    }
    // can't do fast path via GL_REPEAT - we have to tessellate individual rects.
    else {
        unsigned int twidth = mTileSourceRect.width;
        unsigned int theight = mTileSourceRect.height;

        nsIntRegion region = GetVisibleRegion();
        // image subrect in texture coordinates
        float subrect_tl_u = float(mTileSourceRect.x) / float(iwidth);
        float subrect_tl_v = float(mTileSourceRect.y) / float(iheight);
        float subrect_br_u = float(mTileSourceRect.width + mTileSourceRect.x) / float(iwidth);
        float subrect_br_v = float(mTileSourceRect.height + mTileSourceRect.y) / float(iheight);

        // round rect position down to multiples of texture size
        // this way we start at multiples of rect positions
        rect.x = (rect.x / iwidth) * iwidth;
        rect.y = (rect.y / iheight) * iheight;
        // round up size to accomodate for rounding down above
        rect.width  = (rect.width / iwidth + 2) * iwidth;
        rect.height = (rect.height / iheight + 2) * iheight;

        // tesselate the visible region with tiles of subrect size
        for (int y = rect.y; y < rect.y + rect.height;  y += theight) {
            for (int x = rect.x; x < rect.x + rect.width; x += twidth) {
                // when we already tessellate, we might as well save on overdraw here
                if (!region.Intersects(nsIntRect(x, y, twidth, theight))) {
                    continue;
                }
                triangleBuffer.addRect(x, y,
                                       x + twidth, y + theight,
                                       subrect_tl_u, subrect_tl_v,
                                       subrect_br_u, subrect_br_v);
            }
        }
    }
    GLuint vertAttribIndex =
        program->AttribLocation(LayerProgram::VertexAttrib);
    GLuint texCoordAttribIndex =
        program->AttribLocation(LayerProgram::TexCoordAttrib);
    NS_ASSERTION(texCoordAttribIndex != GLuint(-1), "no texture coords?");

    gl()->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    gl()->fVertexAttribPointer(vertAttribIndex, 2,
                               LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                               triangleBuffer.vertexPointer());

    gl()->fVertexAttribPointer(texCoordAttribIndex, 2,
                               LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                               triangleBuffer.texCoordPointer());
    {
        gl()->fEnableVertexAttribArray(texCoordAttribIndex);
        {
            gl()->fEnableVertexAttribArray(vertAttribIndex);
            gl()->fDrawArrays(LOCAL_GL_TRIANGLES, 0, triangleBuffer.elements());
            gl()->fDisableVertexAttribArray(vertAttribIndex);
        }
        gl()->fDisableVertexAttribArray(texCoordAttribIndex);
    }
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
    if (cairoImage->mSurface && pixmap) {
        sGLXLibrary.ReleaseTexImage(pixmap);
        sGLXLibrary.DestroyPixmap(pixmap);
    }
#endif
#ifdef XP_MACOSX
  } else if (image->GetFormat() == Image::MAC_IO_SURFACE) {
     MacIOSurfaceImage *ioImage =
       static_cast<MacIOSurfaceImage*>(image.get());

     if (!mOGLManager->GetThebesLayerCallback()) {
       // If its an empty transaction we still need to update
       // the plugin IO Surface and make sure we grab the
       // new image
       ioImage->Update(GetContainer());
       image = GetContainer()->GetCurrentImage();
       gl()->MakeCurrent();
       ioImage = static_cast<MacIOSurfaceImage*>(image.get());
     }

     if (!ioImage) {
       return;
     }

     gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

     if (!ioImage->GetBackendData(LayerManager::LAYERS_OPENGL)) {
       AllocateTextureIOSurface(ioImage, gl());
     }

     MacIOSurfaceImageOGLBackendData *data =
      static_cast<MacIOSurfaceImageOGLBackendData*>(ioImage->GetBackendData(LayerManager::LAYERS_OPENGL));

     gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
     gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, data->mTexture.GetTextureID());

     ColorTextureLayerProgram *program = 
       mOGLManager->GetRGBARectLayerProgram();
     
     program->Activate();
     if (program->GetTexCoordMultiplierUniformLocation() != -1) {
       // 2DRect case, get the multiplier right for a sampler2DRect
       float f[] = { float(ioImage->GetSize().width), float(ioImage->GetSize().height) };
       program->SetUniform(program->GetTexCoordMultiplierUniformLocation(),
                           2, f);
     } else {
       NS_ASSERTION(0, "no rects?");
     }
     
     program->SetLayerQuadRect(nsIntRect(0, 0, 
                                         ioImage->GetSize().width, 
                                         ioImage->GetSize().height));
     program->SetLayerTransform(GetEffectiveTransform());
     program->SetLayerOpacity(GetEffectiveOpacity());
     program->SetRenderOffset(aOffset);
     program->SetTextureUnit(0);
    
     mOGLManager->BindAndDrawQuad(program);
     gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
#endif
  }
  GetContainer()->NotifyPaintedImage(image);
}

static void
InitTexture(GLContext* aGL, GLuint aTexture, GLenum aFormat, const gfxIntSize& aSize)
{
  aGL->fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  aGL->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                   0,
                   aFormat,
                   aSize.width,
                   aSize.height,
                   0,
                   aFormat,
                   LOCAL_GL_UNSIGNED_BYTE,
                   NULL);
}

static void
UploadYUVToTexture(GLContext* gl, const PlanarYCbCrImage::Data& aData, 
                   GLTexture* aYTexture,
                   GLTexture* aUTexture,
                   GLTexture* aVTexture)
{
  nsIntRect size(0, 0, aData.mYSize.width, aData.mYSize.height);
  GLuint texture = aYTexture->GetTextureID();
  nsRefPtr<gfxASurface> surf = new gfxImageSurface(aData.mYChannel,
                                                   aData.mYSize,
                                                   aData.mYStride,
                                                   gfxASurface::ImageFormatA8);
  gl->UploadSurfaceToTexture(surf, size, texture, true);
  
  size = nsIntRect(0, 0, aData.mCbCrSize.width, aData.mCbCrSize.height);
  texture = aUTexture->GetTextureID();
  surf = new gfxImageSurface(aData.mCbChannel,
                             aData.mCbCrSize,
                             aData.mCbCrStride,
                             gfxASurface::ImageFormatA8);
  gl->UploadSurfaceToTexture(surf, size, texture, true);

  texture = aVTexture->GetTextureID();
  surf = new gfxImageSurface(aData.mCrChannel,
                             aData.mCbCrSize,
                             aData.mCbCrStride,
                             gfxASurface::ImageFormatA8);
  gl->UploadSurfaceToTexture(surf, size, texture, true);
}

ImageLayerOGL::ImageLayerOGL(LayerManagerOGL *aManager)
  : ImageLayer(aManager, NULL)
  , LayerOGL(aManager)
  , mTextureRecycleBin(new TextureRecycleBin())
{ 
  mImplData = static_cast<LayerOGL*>(this);
}

void
ImageLayerOGL::AllocateTexturesYCbCr(PlanarYCbCrImage *aImage)
{
  if (!aImage->mBufferSize)
    return;

  nsAutoPtr<PlanarYCbCrOGLBackendData> backendData(
    new PlanarYCbCrOGLBackendData);

  PlanarYCbCrImage::Data &data = aImage->mData;

  GLContext *gl = mOGLManager->glForResources();

  gl->MakeCurrent();
 
  mTextureRecycleBin->GetTexture(TextureRecycleBin::TEXTURE_Y, data.mYSize, gl, &backendData->mTextures[0]);
  InitTexture(gl, backendData->mTextures[0].GetTextureID(), LOCAL_GL_LUMINANCE, data.mYSize);

  mTextureRecycleBin->GetTexture(TextureRecycleBin::TEXTURE_C, data.mCbCrSize, gl, &backendData->mTextures[1]);
  InitTexture(gl, backendData->mTextures[1].GetTextureID(), LOCAL_GL_LUMINANCE, data.mCbCrSize);

  mTextureRecycleBin->GetTexture(TextureRecycleBin::TEXTURE_C, data.mCbCrSize, gl, &backendData->mTextures[2]);
  InitTexture(gl, backendData->mTextures[2].GetTextureID(), LOCAL_GL_LUMINANCE, data.mCbCrSize);

  UploadYUVToTexture(gl, aImage->mData,
                     &backendData->mTextures[0],
                     &backendData->mTextures[1],
                     &backendData->mTextures[2]);

  backendData->mYSize = aImage->mData.mYSize;
  backendData->mCbCrSize = aImage->mData.mCbCrSize;
  backendData->mTextureRecycleBin = mTextureRecycleBin;

  aImage->SetBackendData(LayerManager::LAYERS_OPENGL, backendData.forget());
}

void
ImageLayerOGL::AllocateTexturesCairo(CairoImage *aImage)
{
  nsAutoPtr<CairoOGLBackendData> backendData(
    new CairoOGLBackendData);

  GLTexture &texture = backendData->mTexture;

  texture.Allocate(mOGLManager->glForResources());

  if (!texture.IsAllocated()) {
    return;
  }

  mozilla::gl::GLContext *gl = texture.GetGLContext();
  gl->MakeCurrent();

  GLuint tex = texture.GetTextureID();
  gl->fActiveTexture(LOCAL_GL_TEXTURE0);

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
  if (sGLXLibrary.SupportsTextureFromPixmap(aImage->mSurface)) {
    if (aImage->mSurface->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA) {
      backendData->mLayerProgram = gl::RGBALayerProgramType;
    } else {
      backendData->mLayerProgram = gl::RGBXLayerProgramType;
    }

    aImage->SetBackendData(LayerManager::LAYERS_OPENGL, backendData.forget());
    return;
  }
#endif
  backendData->mLayerProgram =
    gl->UploadSurfaceToTexture(aImage->mSurface,
                               nsIntRect(0,0, aImage->mSize.width, aImage->mSize.height),
                               tex, true);

  aImage->SetBackendData(LayerManager::LAYERS_OPENGL, backendData.forget());
}

void CairoOGLBackendData::SetTiling(bool aTiling)
{
  if (aTiling == mTiling)
      return;
  mozilla::gl::GLContext *gl = mTexture.GetGLContext();
  gl->MakeCurrent();
  gl->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture.GetTextureID());
  mTiling = aTiling;

  if (aTiling) {
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_REPEAT);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_REPEAT);
  } else {
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
  }
}

ShadowImageLayerOGL::ShadowImageLayerOGL(LayerManagerOGL* aManager)
  : ShadowImageLayer(aManager, nsnull)
  , LayerOGL(aManager)
{
  mImplData = static_cast<LayerOGL*>(this);
}

ShadowImageLayerOGL::~ShadowImageLayerOGL()
{}

bool
ShadowImageLayerOGL::Init(const SharedImage& aFront)
{
  if (aFront.type() == SharedImage::TSurfaceDescriptor) {
    SurfaceDescriptor desc = aFront.get_SurfaceDescriptor();
    nsRefPtr<gfxASurface> surf =
      ShadowLayerForwarder::OpenDescriptor(desc);
    mSize = surf->GetSize();
    mTexImage = gl()->CreateTextureImage(nsIntSize(mSize.width, mSize.height),
                                         surf->GetContentType(),
                                         LOCAL_GL_CLAMP_TO_EDGE);
    return true;
  } else {
    YUVImage yuv = aFront.get_YUVImage();

    nsRefPtr<gfxSharedImageSurface> surfY =
      gfxSharedImageSurface::Open(yuv.Ydata());
    nsRefPtr<gfxSharedImageSurface> surfU =
      gfxSharedImageSurface::Open(yuv.Udata());
    nsRefPtr<gfxSharedImageSurface> surfV =
      gfxSharedImageSurface::Open(yuv.Vdata());

    mSize = surfY->GetSize();
    mCbCrSize = surfU->GetSize();

    if (!mYUVTexture[0].IsAllocated()) {
      mYUVTexture[0].Allocate(mOGLManager->glForResources());
      mYUVTexture[1].Allocate(mOGLManager->glForResources());
      mYUVTexture[2].Allocate(mOGLManager->glForResources());
    }

    NS_ASSERTION(mYUVTexture[0].IsAllocated() &&
                 mYUVTexture[1].IsAllocated() &&
                 mYUVTexture[2].IsAllocated(),
                 "Texture allocation failed!");

    gl()->MakeCurrent();
    InitTexture(gl(), mYUVTexture[0].GetTextureID(), LOCAL_GL_LUMINANCE, mSize);
    InitTexture(gl(), mYUVTexture[1].GetTextureID(), LOCAL_GL_LUMINANCE, mCbCrSize);
    InitTexture(gl(), mYUVTexture[2].GetTextureID(), LOCAL_GL_LUMINANCE, mCbCrSize);
    return true;
  }
  return false;
}

void
ShadowImageLayerOGL::Swap(const SharedImage& aNewFront,
                          SharedImage* aNewBack)
{
  if (!mDestroyed) {
    if (aNewFront.type() == SharedImage::TSurfaceDescriptor) {
      nsRefPtr<gfxASurface> surf =
        ShadowLayerForwarder::OpenDescriptor(aNewFront.get_SurfaceDescriptor());
      gfxIntSize size = surf->GetSize();
      if (mSize != size || !mTexImage ||
          mTexImage->GetContentType() != surf->GetContentType()) {
        Init(aNewFront);
      }
      // XXX this is always just ridiculously slow
      nsIntRegion updateRegion(nsIntRect(0, 0, size.width, size.height));
      mTexImage->DirectUpdate(surf, updateRegion);
    } else {
      const YUVImage& yuv = aNewFront.get_YUVImage();

      nsRefPtr<gfxSharedImageSurface> surfY =
        gfxSharedImageSurface::Open(yuv.Ydata());
      nsRefPtr<gfxSharedImageSurface> surfU =
        gfxSharedImageSurface::Open(yuv.Udata());
      nsRefPtr<gfxSharedImageSurface> surfV =
        gfxSharedImageSurface::Open(yuv.Vdata());
      mPictureRect = yuv.picture();

      gfxIntSize size = surfY->GetSize();
      gfxIntSize CbCrSize = surfU->GetSize();
      if (size != mSize || mCbCrSize != CbCrSize || !mYUVTexture[0].IsAllocated()) {
        Init(aNewFront);
      }

      PlanarYCbCrImage::Data data;
      data.mYChannel = surfY->Data();
      data.mYStride = surfY->Stride();
      data.mYSize = surfY->GetSize();
      data.mCbChannel = surfU->Data();
      data.mCrChannel = surfV->Data();
      data.mCbCrStride = surfU->Stride();
      data.mCbCrSize = surfU->GetSize();

      UploadYUVToTexture(gl(), data, &mYUVTexture[0], &mYUVTexture[1], &mYUVTexture[2]);
    }
  }

  *aNewBack = aNewFront;
}

void
ShadowImageLayerOGL::Disconnect()
{
  Destroy();
}

void
ShadowImageLayerOGL::Destroy()
{
  if (!mDestroyed) {
    mDestroyed = true;
    CleanupResources();
  }
}

Layer*
ShadowImageLayerOGL::GetLayer()
{
  return this;
}

void
ShadowImageLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                                 const nsIntPoint& aOffset)
{
  mOGLManager->MakeCurrent();

  if (mTexImage) {
    ColorTextureLayerProgram *colorProgram =
      mOGLManager->GetColorTextureLayerProgram(mTexImage->GetShaderProgramType());

    colorProgram->Activate();
    colorProgram->SetTextureUnit(0);
    colorProgram->SetLayerTransform(GetEffectiveTransform());
    colorProgram->SetLayerOpacity(GetEffectiveOpacity());
    colorProgram->SetRenderOffset(aOffset);

    mTexImage->SetFilter(mFilter);
    mTexImage->BeginTileIteration();
    do {
      TextureImage::ScopedBindTextureAndApplyFilter texBind(mTexImage, LOCAL_GL_TEXTURE0);
      colorProgram->SetLayerQuadRect(mTexImage->GetTileRect());
      mOGLManager->BindAndDrawQuad(colorProgram);
    } while (mTexImage->NextTile());
  } else {
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mYUVTexture[0].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE1);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mYUVTexture[1].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE2);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mYUVTexture[2].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);

    YCbCrTextureLayerProgram *yuvProgram = mOGLManager->GetYCbCrLayerProgram();

    yuvProgram->Activate();
    yuvProgram->SetLayerQuadRect(nsIntRect(0, 0,
                                           mPictureRect.width,
                                           mPictureRect.height));
    yuvProgram->SetYCbCrTextureUnits(0, 1, 2);
    yuvProgram->SetLayerTransform(GetEffectiveTransform());
    yuvProgram->SetLayerOpacity(GetEffectiveOpacity());
    yuvProgram->SetRenderOffset(aOffset);

    mOGLManager->BindAndDrawQuadWithTextureRect(yuvProgram,
                                                mPictureRect,
                                                nsIntSize(mSize.width, mSize.height));
 }
}

void
ShadowImageLayerOGL::CleanupResources()
{
  mTexImage = nsnull;
}

} /* layers */
} /* mozilla */
