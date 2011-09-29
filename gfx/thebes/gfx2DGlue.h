
#include "gfxRect.h"
#include "gfxMatrix.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace gfx {
class DrawTarget;
class SourceSurface;
class ScaledFont;
}
}

namespace mozilla {
namespace gfx {

inline Rect ToRect(const gfxRect &aRect)
{
  return Rect(Float(aRect.x), Float(aRect.y),
              Float(aRect.width), Float(aRect.height));
}

inline Color ToColor(const gfxRGBA &aRGBA)
{
  return Color(Float(aRGBA.r), Float(aRGBA.g),
               Float(aRGBA.b), Float(aRGBA.a));
}

inline Matrix ToMatrix(const gfxMatrix &aMatrix)
{
  return Matrix(Float(aMatrix.xx), Float(aMatrix.xy), Float(aMatrix.yx),
                Float(aMatrix.yy), Float(aMatrix.x0), Float(aMatrix.y0));
}

inline Point ToPoint(const gfxPoint &aPoint)
{
  return Point(Float(aPoint.x), Float(aPoint.y));
}

inline gfxPoint GFXPoint(const Point &aPoint)
{
  return gfxPoint(aPoint.x, aPoint.y);
}

inline gfxRect GFXRect(const Rect &aRect)
{
  return gfxRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline gfxRGBA GFXRGBA(const Color &aColor)
{
  return gfxRGBA(aColor.r, aColor.g, aColor.b, aColor.a);
}

inline gfxMatrix GFXMatrix(const Matrix &aMatrix)
{
  return gfxMatrix(aMatrix._11, aMatrix._12, aMatrix._21,
                   aMatrix._22, aMatrix._31, aMatrix._32);
}

inline gfxASurface::gfxContentType ContentForFormat(const SurfaceFormat &aFormat)
{
  switch (aFormat) {
  case FORMAT_B8G8R8X8:
    return gfxASurface::CONTENT_COLOR;
  case FORMAT_A8:
    return gfxASurface::CONTENT_ALPHA;
  default:
    return gfxASurface::CONTENT_COLOR_ALPHA;
  }
}

inline SurfaceFormat FormatForContent(gfxASurface::gfxContentType aContent)
{
  switch (aContent) {
  case gfxASurface::CONTENT_COLOR:
    return FORMAT_B8G8R8X8;
  case gfxASurface::CONTENT_ALPHA:
    return FORMAT_A8;
  case gfxASurface::CONTENT_COLOR_ALPHA:
    return FORMAT_B8G8R8A8;
  }
}

}
}
