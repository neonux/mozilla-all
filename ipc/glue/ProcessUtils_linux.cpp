/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessUtils.h"

#include "nsString.h"

#include <sys/prctl.h>

namespace mozilla {
namespace ipc {

void SetThisProcessName(const char *aName)
{
  prctl(PR_SET_NAME, (unsigned long)aName, 0uL, 0uL, 0uL);
}

} // namespace ipc
} // namespace mozilla
