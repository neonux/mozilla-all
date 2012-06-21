/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "critical_section_wrapper.h"
#include "time_scheduler.h"

namespace webrtc {
TimeScheduler::TimeScheduler(const WebRtc_UWord32 periodicityInMs)
    : _crit(CriticalSectionWrapper::CreateCriticalSection()),
      _isStarted(false),
      _lastPeriodMark(),
      _periodicityInMs(periodicityInMs),
      _periodicityInTicks(TickTime::MillisecondsToTicks(periodicityInMs)),
      _missedPeriods(0)
 {
 }

TimeScheduler::~TimeScheduler()
{
    delete _crit;
}

WebRtc_Word32 TimeScheduler::UpdateScheduler()
{
    CriticalSectionScoped cs(_crit);
    if(!_isStarted)
    {
        _isStarted = true;
        _lastPeriodMark = TickTime::Now();
        return 0;
    }
    // Don't perform any calculations until the debt of pending periods have
    // been worked off.
    if(_missedPeriods > 0)
    {
        _missedPeriods--;
        return 0;
    }

    // Calculate the time that has past since previous call to this function.
    TickTime tickNow = TickTime::Now();
    TickInterval amassedTicks = tickNow - _lastPeriodMark;
    WebRtc_Word64 amassedMs = amassedTicks.Milliseconds();

    // Calculate the number of periods the time that has passed correspond to.
    WebRtc_Word32 periodsToClaim = (WebRtc_Word32)amassedMs /
        ((WebRtc_Word32)_periodicityInMs);

    // One period will be worked off by this call. Make sure that the number of
    // pending periods don't end up being negative (e.g. if this function is
    // called to often).
    if(periodsToClaim < 1)
    {
        periodsToClaim = 1;
    }

    // Update the last period mark without introducing any drifting.
    // Note that if this fuunction is called to often _lastPeriodMark can
    // refer to a time in the future which in turn will yield TimeToNextUpdate
    // that is greater than the periodicity
    for(WebRtc_Word32 i = 0; i < periodsToClaim; i++)
    {
        _lastPeriodMark += _periodicityInTicks;
    }

    // Update the total amount of missed periods note that we have processed
    // one period hence the - 1
    _missedPeriods += periodsToClaim - 1;
    return 0;
}

WebRtc_Word32 TimeScheduler::TimeToNextUpdate(
    WebRtc_Word32& updateTimeInMS) const
{
    CriticalSectionScoped cs(_crit);
    // Missed periods means that the next UpdateScheduler() should happen
    // immediately.
    if(_missedPeriods > 0)
    {
        updateTimeInMS = 0;
        return 0;
    }

    // Calculate the time (in ms) that has past since last call to
    // UpdateScheduler()
    TickTime tickNow = TickTime::Now();
    TickInterval ticksSinceLastUpdate = tickNow - _lastPeriodMark;
    const WebRtc_Word32 millisecondsSinceLastUpdate =
        (WebRtc_Word32) ticksSinceLastUpdate.Milliseconds();

    updateTimeInMS = _periodicityInMs - millisecondsSinceLastUpdate;
    updateTimeInMS =  (updateTimeInMS < 0) ? 0 : updateTimeInMS;
    return 0;
}
} // namespace webrtc
