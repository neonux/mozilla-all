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
 * The Original Code is Thebes gfx.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef _GFXALPHARECOVERY_H_
#define _GFXALPHARECOVERY_H_

#include "gfxContext.h"
#include "gfxImageSurface.h"

class THEBES_API gfxAlphaRecovery {
public:
    struct Analysis {
        PRBool uniformColor;
        PRBool uniformAlpha;
        gfxFloat alpha;
        gfxFloat r, g, b;
    };

    /* Given two surfaces of equal size with the same rendering, one onto a
     * black background and the other onto white, recovers alpha values from
     * the difference and sets the alpha values on the black surface.
     * The surfaces must have format RGB24 or ARGB32.
     * Returns PR_TRUE on success.
     */
    static PRBool RecoverAlpha (gfxImageSurface *blackSurface,
                                const gfxImageSurface *whiteSurface,
                                Analysis *analysis = nsnull);
};

#endif /* _GFXALPHARECOVERY_H_ */
