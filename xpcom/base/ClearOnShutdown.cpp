/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"

namespace mozilla {
namespace ClearOnShutdown_Internal {

bool sHasShutDown = false;
LinkedList<ShutdownObserver> sShutdownObservers;

} // namespace ClearOnShutdown_Internal
} // namespace mozilla
