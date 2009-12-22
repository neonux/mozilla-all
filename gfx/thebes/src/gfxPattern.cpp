/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
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

#include "gfxTypes.h"
#include "gfxPattern.h"
#include "gfxASurface.h"
#include "gfxPlatform.h"

#include "cairo.h"

gfxPattern::gfxPattern(cairo_pattern_t *aPattern)
{
    mPattern = cairo_pattern_reference(aPattern);
}

gfxPattern::gfxPattern(const gfxRGBA& aColor)
{
    mPattern = cairo_pattern_create_rgba(aColor.r, aColor.g, aColor.b, aColor.a);
}

// from another surface
gfxPattern::gfxPattern(gfxASurface *surface)
{
    mPattern = cairo_pattern_create_for_surface(surface->CairoSurface());
}

// linear
gfxPattern::gfxPattern(gfxFloat x0, gfxFloat y0, gfxFloat x1, gfxFloat y1)
{
    mPattern = cairo_pattern_create_linear(x0, y0, x1, y1);
}

// radial
gfxPattern::gfxPattern(gfxFloat cx0, gfxFloat cy0, gfxFloat radius0,
                       gfxFloat cx1, gfxFloat cy1, gfxFloat radius1)
{
    mPattern = cairo_pattern_create_radial(cx0, cy0, radius0,
                                           cx1, cy1, radius1);
}

gfxPattern::~gfxPattern()
{
    cairo_pattern_destroy(mPattern);
}

cairo_pattern_t *
gfxPattern::CairoPattern()
{
    return mPattern;
}

void
gfxPattern::AddColorStop(gfxFloat offset, const gfxRGBA& c)
{
    if (gfxPlatform::GetCMSMode() == eCMSMode_All) {
        gfxRGBA cms;
        gfxPlatform::TransformPixel(c, cms, gfxPlatform::GetCMSRGBTransform());

        // Use the original alpha to avoid unnecessary float->byte->float
        // conversion errors
        cairo_pattern_add_color_stop_rgba(mPattern, offset,
                                          cms.r, cms.g, cms.b, c.a);
    }
    else
        cairo_pattern_add_color_stop_rgba(mPattern, offset, c.r, c.g, c.b, c.a);
}

void
gfxPattern::SetMatrix(const gfxMatrix& matrix)
{
    cairo_matrix_t mat = *reinterpret_cast<const cairo_matrix_t*>(&matrix);
    cairo_pattern_set_matrix(mPattern, &mat);
}

gfxMatrix
gfxPattern::GetMatrix() const
{
    cairo_matrix_t mat;
    cairo_pattern_get_matrix(mPattern, &mat);
    return gfxMatrix(*reinterpret_cast<gfxMatrix*>(&mat));
}

void
gfxPattern::SetExtend(GraphicsExtend extend)
{
    if (extend == EXTEND_PAD_EDGE) {
        if (cairo_pattern_get_type(mPattern) == CAIRO_PATTERN_TYPE_SURFACE) {
            cairo_surface_t *surf = NULL;

            cairo_pattern_get_surface (mPattern, &surf);
            if (surf) {
                switch (cairo_surface_get_type(surf)) {
                    case CAIRO_SURFACE_TYPE_WIN32_PRINTING:
                    case CAIRO_SURFACE_TYPE_QUARTZ:
                        extend = EXTEND_NONE;
                        break;

                    case CAIRO_SURFACE_TYPE_WIN32:
                    case CAIRO_SURFACE_TYPE_XLIB:
                    default:
                        extend = EXTEND_PAD;
                        break;
                }
            }
        }

        // if something went wrong, or not a surface pattern, use PAD
        if (extend == EXTEND_PAD_EDGE)
            extend = EXTEND_PAD;
    }

    cairo_pattern_set_extend(mPattern, (cairo_extend_t)extend);
}

gfxPattern::GraphicsExtend
gfxPattern::Extend() const
{
    return (GraphicsExtend)cairo_pattern_get_extend(mPattern);
}

void
gfxPattern::SetFilter(GraphicsFilter filter)
{
    cairo_pattern_set_filter(mPattern, (cairo_filter_t)filter);
}

gfxPattern::GraphicsFilter
gfxPattern::Filter() const
{
    return (GraphicsFilter)cairo_pattern_get_filter(mPattern);
}

PRBool
gfxPattern::GetSolidColor(gfxRGBA& aColor)
{
    return cairo_pattern_get_rgba(mPattern,
                                  &aColor.r,
                                  &aColor.g,
                                  &aColor.b,
                                  &aColor.a) == CAIRO_STATUS_SUCCESS;
}

already_AddRefed<gfxASurface>
gfxPattern::GetSurface()
{
    cairo_surface_t *surf = nsnull;

    if (cairo_pattern_get_surface (mPattern, &surf) != CAIRO_STATUS_SUCCESS)
        return nsnull;

    return gfxASurface::Wrap(surf);
}

gfxPattern::GraphicsPatternType
gfxPattern::GetType() const
{
    return (GraphicsPatternType) cairo_pattern_get_type(mPattern);
}

int
gfxPattern::CairoStatus()
{
    return cairo_pattern_status(mPattern);
}
