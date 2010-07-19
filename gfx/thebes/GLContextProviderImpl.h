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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *   Matt Woodrow <mwoodrow@mozilla.com>
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

#ifndef IN_GL_CONTEXT_PROVIDER_H
#error GLContextProviderImpl.h must only be included from GLContextProvider.h
#endif

#ifndef GL_CONTEXT_PROVIDER_NAME
#error GL_CONTEXT_PROVIDER_NAME not defined
#endif

class THEBES_API GL_CONTEXT_PROVIDER_NAME
{
public:
    /**
     * Create a context that renders to the surface of the widget that is
     * passed in.
     *
     * @param Widget whose surface to create a context for
     * @return Context to use for this window
     */
    static already_AddRefed<GLContext>
    CreateForWindow(nsIWidget *aWidget);

    /**
     * Creates a PBuffer.
     *
     * @param aSize Size of the pbuffer to create
     * @param aFormat A ContextFormat describing the desired context attributes.  Defaults to a basic RGBA32 context.
     *
     * @return Context to use for this Pbuffer
     */
    static already_AddRefed<GLContext>
    CreatePBuffer(const gfxIntSize &aSize,
                  const ContextFormat& aFormat = ContextFormat::BasicRGBA32Format);

    /**
     * Try to create a GL context from native surface for arbitrary gfxASurface
     * If surface not compatible this will return NULL
     *
     * @param aSurface surface to create a context for
     * @return Context to use for this surface
     */
    static already_AddRefed<GLContext>
    CreateForNativePixmapSurface(gfxASurface *aSurface);
};
