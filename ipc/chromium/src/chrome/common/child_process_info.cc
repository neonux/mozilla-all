// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_info.h"

#include <limits>

#include "base/logging.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"

std::wstring ChildProcessInfo::GetTypeNameInEnglish(
    ChildProcessInfo::ProcessType type) {
  switch (type) {
    case BROWSER_PROCESS:
      return L"Browser";
    case RENDER_PROCESS:
      return L"Tab";
    case PLUGIN_PROCESS:
      return L"Plug-in";
    case WORKER_PROCESS:
      return L"Web Worker";
    case UNKNOWN_PROCESS:
      default:
      DCHECK(false) << "Unknown child process type!";
      return L"Unknown";
    }
}

std::wstring ChildProcessInfo::GetLocalizedTitle() const {
  return name_;
}

ChildProcessInfo::ChildProcessInfo(ProcessType type) {
  // This constructor is only used by objects which derive from this class,
  // which means *this* is a real object that refers to a child process, and not
  // just a simple object that contains information about it.  So add it to our
  // list of running processes.
  type_ = type;
  pid_ = -1;
}


ChildProcessInfo::~ChildProcessInfo() {
}

std::wstring ChildProcessInfo::GenerateRandomChannelID(void* instance) {
  // Note: the string must start with the current process id, this is how
  // child processes determine the pid of the parent.
  // Build the channel ID.  This is composed of a unique identifier for the
  // parent browser process, an identifier for the child instance, and a random
  // component. We use a random component so that a hacked child process can't
  // cause denial of service by causing future named pipe creation to fail.
  return StringPrintf(L"%d.%x.%d",
                      base::GetCurrentProcId(), instance,
                      base::RandInt(0, std::numeric_limits<int>::max()));
}
