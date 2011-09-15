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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "DrawTargetCairo.h"
#include "SourceSurfaceCairo.h"

#include "cairo.h"

#ifdef CAIRO_HAS_QUARTZ_SURFACE
#include "cairo-quartz.h"
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef CAIRO_HAS_XLIB_SURFACE
#include "cairo-xlib.h"
#endif

#include <algorithm>

namespace mozilla {
namespace gfx {

static cairo_operator_t
GfxOpToCairoOp(CompositionOp op)
{
  switch (op)
  {
    case OP_OVER:
      return CAIRO_OPERATOR_OVER;
    case OP_SOURCE:
      return CAIRO_OPERATOR_SOURCE;
    case OP_ADD:
      return CAIRO_OPERATOR_ADD;
    case OP_ATOP:
      return CAIRO_OPERATOR_ATOP;
    case OP_COUNT:
      break;
  }

  return CAIRO_OPERATOR_OVER;
}

static cairo_filter_t
GfxFilterToCairoFilter(Filter filter)
{
  switch (filter)
  {
    case FILTER_LINEAR:
      return CAIRO_FILTER_BILINEAR;
    case FILTER_POINT:
      return CAIRO_FILTER_NEAREST;
  }

  return CAIRO_FILTER_BILINEAR;
}

static cairo_format_t
GfxFormatToCairoFormat(SurfaceFormat format)
{
  switch (format)
  {
    case FORMAT_B8G8R8A8:
      return CAIRO_FORMAT_ARGB32;
    case FORMAT_B8G8R8X8:
      return CAIRO_FORMAT_RGB24;
    case FORMAT_A8:
      return CAIRO_FORMAT_A8;
  }

  return CAIRO_FORMAT_ARGB32;
}

static cairo_content_t
GfxFormatToCairoContent(SurfaceFormat format)
{
  switch(format)
  {
    case FORMAT_B8G8R8A8:
      return CAIRO_CONTENT_COLOR_ALPHA;
    case FORMAT_B8G8R8X8:
      return CAIRO_CONTENT_COLOR;
    case FORMAT_A8:
      return CAIRO_CONTENT_ALPHA;
  }

  return CAIRO_CONTENT_COLOR_ALPHA;
}

static SurfaceFormat
CairoContentToGfxFormat(cairo_content_t content)
{
  switch(content)
  {
    case CAIRO_CONTENT_COLOR_ALPHA:
      return FORMAT_B8G8R8A8;
    case CAIRO_CONTENT_COLOR:
      return FORMAT_B8G8R8X8;
    case CAIRO_CONTENT_ALPHA:
      return FORMAT_A8;
  }

  return FORMAT_B8G8R8A8;
}

static bool
GetCairoSurfaceSize(cairo_surface_t* surface, IntSize& size)
{
  switch (cairo_surface_get_type(surface)) {
    case CAIRO_SURFACE_TYPE_IMAGE:
    {
      size.width = cairo_image_surface_get_width(surface);
      size.height = cairo_image_surface_get_height(surface);
      return true;
    }

#ifdef CAIRO_HAS_XLIB_SURFACE
    case CAIRO_SURFACE_TYPE_XLIB:
    {
      size.width = cairo_xlib_surface_get_width(surface);
      size.height = cairo_xlib_surface_get_height(surface);
      return true;
    }
#endif

#ifdef CAIRO_HAS_QUARTZ_SURFACE
    case CAIRO_SURFACE_TYPE_QUARTZ:
    {
      CGContextRef cgc = cairo_quartz_surface_get_cg_context(surface);

      // It's valid to call these CGBitmapContext functions on non-bitmap
      // contexts; they'll just return 0 in that case.
      size.width = CGBitmapContextGetWidth(cgc);
      size.height = CGBitmapContextGetWidth(cgc);
      return size.width != 0;
    }
#endif

    default:
      return false;
  }
}

void
GfxMatrixToCairoMatrix(const Matrix& mat, cairo_matrix_t& retval)
{
  cairo_matrix_init(&retval, mat._11, mat._12, mat._21, mat._22, mat._31, mat._32);
}

DrawTargetCairo::DrawTargetCairo()
  : mContext(NULL)
{
}

DrawTargetCairo::~DrawTargetCairo()
{
  MarkSnapshotsIndependent();
  cairo_destroy(mContext);
}

IntSize
DrawTargetCairo::GetSize()
{
  return IntSize();
}

TemporaryRef<SourceSurface>
DrawTargetCairo::Snapshot()
{
  cairo_surface_t* csurf = cairo_get_target(mContext);
  IntSize size;
  if (GetCairoSurfaceSize(csurf, size)) {
    cairo_content_t content = cairo_surface_get_content(csurf);
    RefPtr<SourceSurfaceCairo> surf = new SourceSurfaceCairo(csurf, size,
                                                             CairoContentToGfxFormat(content),
                                                             this);
    AppendSnapshot(surf);
    return surf;
  }

  return NULL;
}

void
DrawTargetCairo::Flush()
{
  cairo_surface_t* surf = cairo_get_target(mContext);
  cairo_surface_flush(surf);
}

void
DrawTargetCairo::PrepareForDrawing(cairo_t* aContext)
{
  MarkChanged();

  cairo_matrix_t mat;
  GfxMatrixToCairoMatrix(mTransform, mat);
  cairo_set_matrix(aContext, &mat);
}

void
DrawTargetCairo::DrawSurface(SourceSurface *aSurface,
                             const Rect &aDest,
                             const Rect &aSource,
                             const DrawSurfaceOptions &aSurfOptions,
                             const DrawOptions &aOptions)
{
  PrepareForDrawing(mContext);

  float sx = aSource.Width() / aDest.Width();
  float sy = aSource.Height() / aDest.Height();

  cairo_matrix_t src_mat;
  cairo_matrix_init_scale(&src_mat, sx, sy);
  cairo_matrix_translate(&src_mat, aSource.X(), aSource.Y());

  cairo_surface_t* surf = NULL;
  if (aSurface->GetType() == SURFACE_CAIRO) {
    surf = static_cast<SourceSurfaceCairo*>(aSurface)->GetSurface();
  }

  cairo_pattern_t* pat = cairo_pattern_create_for_surface(surf);
  cairo_pattern_set_matrix(pat, &src_mat);
  cairo_pattern_set_filter(pat, GfxFilterToCairoFilter(aSurfOptions.mFilter));

  cairo_save(mContext);

  cairo_set_operator(mContext, GfxOpToCairoOp(aOptions.mCompositionOp));

  cairo_translate(mContext, aDest.X(), aDest.Y());

  cairo_set_source(mContext, pat);

  cairo_new_path(mContext);
  cairo_rectangle(mContext, 0, 0, aDest.Width(), aDest.Height());
  cairo_clip(mContext);
  cairo_paint(mContext);

  cairo_restore(mContext);

  cairo_pattern_destroy(pat);
}

void
DrawTargetCairo::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                       const Point &aDest,
                                       const Color &aColor,
                                       const Point &aOffset,
                                       Float aSigma,
                                       CompositionOp aOperator)
{
  PrepareForDrawing(mContext);
}

void
DrawTargetCairo::FillRect(const Rect &aRect,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions)
{
  PrepareForDrawing(mContext);

  cairo_save(mContext);

  cairo_new_path(mContext);
  cairo_rectangle(mContext, aRect.x, aRect.y, aRect.Width(), aRect.Height());

  cairo_set_operator(mContext, GfxOpToCairoOp(aOptions.mCompositionOp));

  if (aPattern.GetType() == PATTERN_COLOR) {
    Color color = static_cast<const ColorPattern&>(aPattern).mColor;
    cairo_set_source_rgba(mContext, color.r, color.g,
                                    color.b, color.a);
  }

  cairo_fill(mContext);

  cairo_restore(mContext);
}

void
DrawTargetCairo::CopySurface(SourceSurface *aSurface,
                             const IntRect &aSourceRect,
                             const IntPoint &aDestination)
{
  PrepareForDrawing(mContext);
}

void
DrawTargetCairo::ClearRect(const Rect& aRect)
{
  PrepareForDrawing(mContext);

  cairo_save(mContext);

  cairo_new_path(mContext);
  cairo_set_operator(mContext, CAIRO_OPERATOR_CLEAR);
  cairo_rectangle(mContext, aRect.X(), aRect.Y(),
                  aRect.Width(), aRect.Height());
  cairo_fill(mContext);

  cairo_restore(mContext);
}

void
DrawTargetCairo::StrokeRect(const Rect &aRect,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions /* = StrokeOptions() */,
                          const DrawOptions &aOptions /* = DrawOptions() */)
{
  PrepareForDrawing(mContext);
}

void
DrawTargetCairo::StrokeLine(const Point &aStart,
                            const Point &aEnd,
                            const Pattern &aPattern,
                            const StrokeOptions &aStrokeOptions /* = StrokeOptions() */,
                            const DrawOptions &aOptions /* = DrawOptions() */)
{
  PrepareForDrawing(mContext);
}

void
DrawTargetCairo::Stroke(const Path *aPath,
                        const Pattern &aPattern,
                        const StrokeOptions &aStrokeOptions /* = StrokeOptions() */,
                        const DrawOptions &aOptions /* = DrawOptions() */)
{
  PrepareForDrawing(mContext);
}

void
DrawTargetCairo::Fill(const Path *aPath,
                      const Pattern &aPattern,
                      const DrawOptions &aOptions /* = DrawOptions() */)
{
  PrepareForDrawing(mContext);
}

void
DrawTargetCairo::FillGlyphs(ScaledFont *aFont,
                            const GlyphBuffer &aBuffer,
                            const Pattern &aPattern,
                            const DrawOptions &aOptions)
{
  PrepareForDrawing(mContext);
}

void
DrawTargetCairo::PushClip(const Path *aPath)
{
}

void
DrawTargetCairo::PopClip()
{
}

TemporaryRef<PathBuilder>
DrawTargetCairo::CreatePathBuilder(FillRule aFillRule /* = FILL_WINDING */) const
{
  return NULL;
}

TemporaryRef<GradientStops>
DrawTargetCairo::CreateGradientStops(GradientStop *aStops, uint32_t aNumStops) const
{
  return NULL;
}

TemporaryRef<SourceSurface>
DrawTargetCairo::CreateSourceSurfaceFromData(unsigned char *aData,
                                             const IntSize &aSize,
                                             int32_t aStride,
                                             SurfaceFormat aFormat) const
{
  cairo_surface_t* surf = cairo_image_surface_create_for_data(aData,
                                                              GfxFormatToCairoFormat(aFormat),
                                                              aSize.width,
                                                              aSize.height,
                                                              aStride);
  RefPtr<SourceSurfaceCairo> source_surf = new SourceSurfaceCairo(surf, aSize, aFormat);
  cairo_surface_destroy(surf);
  return source_surf;
}

TemporaryRef<SourceSurface>
DrawTargetCairo::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  return aSurface;
}

TemporaryRef<SourceSurface>
DrawTargetCairo::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
  if (aSurface.mType == NATIVE_SURFACE_CAIRO_SURFACE) {
    IntSize size;
    cairo_surface_t* surf = static_cast<cairo_surface_t*>(aSurface.mSurface);
    if (GetCairoSurfaceSize(surf, size)) {
      RefPtr<SourceSurfaceCairo> sourcesurf =
        new SourceSurfaceCairo(surf, size, aSurface.mFormat);
      return sourcesurf;
    }
  }

  return NULL;
}

TemporaryRef<DrawTarget>
DrawTargetCairo::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  cairo_surface_t* similar = cairo_surface_create_similar(cairo_get_target(mContext),
                                                          GfxFormatToCairoContent(aFormat),
                                                          aSize.width, aSize.height);

  if (!cairo_surface_status(similar)) {
    RefPtr<DrawTargetCairo> target = new DrawTargetCairo();
    target->Init(similar);
    return target;
  }

  return NULL;
}

bool
DrawTargetCairo::Init(cairo_surface_t* aSurface)
{
  mContext = cairo_create(aSurface);

  return true;
}

void *
DrawTargetCairo::GetNativeSurface(NativeSurfaceType aType)
{
  if (aType == NATIVE_SURFACE_CAIRO_SURFACE) {
    return cairo_get_target(mContext);
  }

  return NULL;
}

void
DrawTargetCairo::MarkSnapshotsIndependent()
{
  for (std::vector<SourceSurfaceCairo*>::iterator iter = mSnapshots.begin();
       iter != mSnapshots.end();
       ++iter) {
    (*iter)->MarkIndependent();
  }
}

void
DrawTargetCairo::AppendSnapshot(SourceSurfaceCairo* aSnapshot)
{
  mSnapshots.push_back(aSnapshot);
}

void
DrawTargetCairo::RemoveSnapshot(SourceSurfaceCairo* aSnapshot)
{
  std::vector<SourceSurfaceCairo*>::iterator iter = std::find(mSnapshots.begin(),
                                                              mSnapshots.end(),
                                                              aSnapshot);
  if (iter != mSnapshots.end()) {
    mSnapshots.erase(iter);
  }
}

void
DrawTargetCairo::MarkChanged()
{
  if (!mSnapshots.empty()) {
    for (std::vector<SourceSurfaceCairo*>::iterator iter = mSnapshots.begin();
         iter != mSnapshots.end(); ++iter) {
      (*iter)->DrawTargetWillChange();
    }
    // All snapshots will now have copied data.
    mSnapshots.clear();
  }
}


}
}
