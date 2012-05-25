/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMai.h"
#include "nsDocAccessibleWrap.h"

////////////////////////////////////////////////////////////////////////////////
// nsDocAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

nsDocAccessibleWrap::
  nsDocAccessibleWrap(nsIDocument* aDocument, nsIContent* aRootContent,
                      nsIPresShell* aPresShell) :
  nsDocAccessible(aDocument, aRootContent, aPresShell), mActivated(false)
{
}

nsDocAccessibleWrap::~nsDocAccessibleWrap()
{
}
