/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIdleServiceX.h"
#include "nsObjCExceptions.h"
#include "nsIServiceManager.h"
#import <Foundation/Foundation.h>

NS_IMPL_ISUPPORTS2(nsIdleServiceX, nsIIdleService, nsIdleService)

bool
nsIdleServiceX::PollIdleTime(PRUint32 *aIdleTime)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  kern_return_t rval;
  mach_port_t masterPort;

  rval = IOMasterPort(kIOMasterPortDefault, &masterPort);
  if (rval != KERN_SUCCESS)
    return false;

  io_iterator_t hidItr;
  rval = IOServiceGetMatchingServices(masterPort,
                                      IOServiceMatching("IOHIDSystem"),
                                      &hidItr);

  if (rval != KERN_SUCCESS)
    return false;
  NS_ASSERTION(hidItr, "Our iterator is null, but it ought not to be!");

  io_registry_entry_t entry = IOIteratorNext(hidItr);
  NS_ASSERTION(entry, "Our IO Registry Entry is null, but it shouldn't be!");

  IOObjectRelease(hidItr);

  NSMutableDictionary *hidProps;
  rval = IORegistryEntryCreateCFProperties(entry,
                                           (CFMutableDictionaryRef*)&hidProps,
                                           kCFAllocatorDefault, 0);
  if (rval != KERN_SUCCESS)
    return false;
  NS_ASSERTION(hidProps, "HIDProperties is null, but no error was returned.");
  [hidProps autorelease];

  id idleObj = [hidProps objectForKey:@"HIDIdleTime"];
  NS_ASSERTION([idleObj isKindOfClass: [NSData class]] ||
               [idleObj isKindOfClass: [NSNumber class]],
               "What we got for the idle object is not what we expect!");

  uint64_t time;
  if ([idleObj isKindOfClass: [NSData class]])
    [idleObj getBytes: &time];
  else
    time = [idleObj unsignedLongLongValue];

  IOObjectRelease(entry);

  // convert to ms from ns
  time /= 1000000;
  if (time > PR_UINT32_MAX) // Overflow will occur
    return false;

  *aIdleTime = static_cast<PRUint32>(time);

  return true;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

bool
nsIdleServiceX::UsePollMode()
{
  return true;
}

