/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Compositor.h"

namespace mozilla {
namespace layers {

/* static */ TemporaryRef<TextureClient>
CompositingFactory::CreateTextureClient(const TextureHostType &aHostType, const ImageSourceType& aImageSourceType)
{
  //TODO[nrc]
  RefPtr<TextureClient> result = nullptr;
  switch (aHostType) {
  case HOST_D3D10:
    break;
  case HOST_GL:
    break;
  case HOST_SHMEM:
    break;
  default:
    break;
  }
  return result.forget();
}

}
}