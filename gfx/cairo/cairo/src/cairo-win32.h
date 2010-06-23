/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Owen Taylor <otaylor@redhat.com>
 */

#ifndef _CAIRO_WIN32_H_
#define _CAIRO_WIN32_H_

#include "cairo.h"

#if CAIRO_HAS_WIN32_SURFACE

#include <windows.h>

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_win32_surface_create (HDC hdc);

cairo_public cairo_surface_t *
cairo_win32_printing_surface_create (HDC hdc);

cairo_public cairo_surface_t *
cairo_win32_surface_create_with_ddb (HDC hdc,
                                     cairo_format_t format,
                                     int width,
                                     int height);

cairo_public cairo_surface_t *
cairo_win32_surface_create_with_dib (cairo_format_t format,
                                     int width,
                                     int height);

cairo_public HDC
cairo_win32_surface_get_dc (cairo_surface_t *surface);

cairo_public HDC
cairo_win32_get_dc_with_clip (cairo_t *cr);

cairo_public cairo_surface_t *
cairo_win32_surface_get_image (cairo_surface_t *surface);

cairo_public cairo_status_t
cairo_win32_surface_set_can_convert_to_dib (cairo_surface_t *surface, cairo_bool_t can_convert);

cairo_public cairo_status_t
cairo_win32_surface_get_can_convert_to_dib (cairo_surface_t *surface, cairo_bool_t *can_convert);

#if CAIRO_HAS_WIN32_FONT

/*
 * Win32 font support
 */

cairo_public cairo_font_face_t *
cairo_win32_font_face_create_for_logfontw (LOGFONTW *logfont);

cairo_public cairo_font_face_t *
cairo_win32_font_face_create_for_hfont (HFONT font);

cairo_public cairo_font_face_t *
cairo_win32_font_face_create_for_logfontw_hfont (LOGFONTW *logfont, HFONT font);

cairo_public cairo_status_t
cairo_win32_scaled_font_select_font (cairo_scaled_font_t *scaled_font,
				     HDC                  hdc);

cairo_public void
cairo_win32_scaled_font_done_font (cairo_scaled_font_t *scaled_font);

cairo_public double
cairo_win32_scaled_font_get_metrics_factor (cairo_scaled_font_t *scaled_font);

cairo_public void
cairo_win32_scaled_font_get_logical_to_device (cairo_scaled_font_t *scaled_font,
					       cairo_matrix_t *logical_to_device);

cairo_public void
cairo_win32_scaled_font_get_device_to_logical (cairo_scaled_font_t *scaled_font,
					       cairo_matrix_t *device_to_logical);

#endif /* CAIRO_HAS_WIN32_FONT */

#if CAIRO_HAS_DWRITE_FONT

/*
 * Win32 DirectWrite font support
 */
cairo_public cairo_font_face_t *
cairo_dwrite_font_face_create_for_dwrite_fontface(void *dwrite_font, void *dwrite_font_face);

#endif /* CAIRO_HAS_DWRITE_FONT */

#if CAIRO_HAS_D2D_SURFACE

/**
 * Create a D2D surface for an HWND
 *
 * \param wnd Handle for the window
 * \param content Content of the window, should be COLOR_ALPHA for transparent windows
 * \return New cairo surface
 */
cairo_public cairo_surface_t *
cairo_d2d_surface_create_for_hwnd(HWND wnd, cairo_content_t content);

/**
 * Create a D2D surface of a certain size.
 *
 * \param format Cairo format of the surface
 * \param width Width of the surface
 * \param height Height of the surface
 * \return New cairo surface
 */
cairo_public cairo_surface_t *
cairo_d2d_surface_create(cairo_format_t format,
                         int width,
                         int height);

/**
 * Present the backbuffer for a surface create for an HWND. This needs
 * to be called when the owner of the original window surface wants to
 * actually present the executed drawing operations to the screen.
 *
 * \param surface D2D surface.
 */
void cairo_d2d_present_backbuffer(cairo_surface_t *surface);

/**
 * Scroll the surface, this only moves the surface graphics, it does not
 * actually scroll child windows or anything like that. Nor does it invalidate
 * that area of the window.
 *
 * \param surface The d2d surface this operation should apply to.
 * \param x The x delta for the movement
 * \param y The y delta for the movement
 * \param clip The clip rectangle, the is the 'part' of the surface that needs
 * scrolling.
 */
void cairo_d2d_scroll(cairo_surface_t *surface, int x, int y, cairo_rectangle_t *clip);

/**
 * Verify if D2D surfaces are actually supported. This will confirm the needed
 * hardware is available.
 *
 * \return True if the support is available. If false surface creation will
 * return error surfaces.
 */
cairo_bool_t cairo_d2d_has_support();

/**
 * Get a DC for the current render target. When selecting the retention option this
 * call can be relatively slow, since it may require reading back contents from the
 * hardware surface.
 *
 * \note This must be matched by a call to ReleaseDC!
 *
 * \param retain_contents If true the current contents of the RT is copied to the DC,
 * otherwise the DC is initialized to transparent black.
 */
HDC cairo_d2d_get_dc(cairo_surface_t *surface, cairo_bool_t retain_contents);

/**
 * Release the DC acquired through GetDC(). Optionally an update region may be specified
 *
 * \param updated_rect The area of the DC that was updated, if null the entire dc will
 * be updated.
 */
void cairo_d2d_release_dc(cairo_surface_t *surcace, const cairo_rectangle_int_t *updated_rect);
#endif

CAIRO_END_DECLS

#else  /* CAIRO_HAS_WIN32_SURFACE */
# error Cairo was not compiled with support for the win32 backend
#endif /* CAIRO_HAS_WIN32_SURFACE */

#endif /* _CAIRO_WIN32_H_ */
