/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_TEST_DEFINES_H
#define WEBRTC_VOICE_ENGINE_VOE_TEST_DEFINES_H

// Read WEBRTC_VOICE_ENGINE_XXX_API compiler flags
#include "engine_configurations.h"

#ifdef WEBRTC_ANDROID
#include <android/log.h>
#define ANDROID_LOG_TAG "VoiceEngine Auto Test"
#define TEST_LOG(...) \
    __android_log_print(ANDROID_LOG_DEBUG, ANDROID_LOG_TAG, __VA_ARGS__)
#define TEST_LOG_ERROR(...) \
    __android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG, __VA_ARGS__)
#define TEST_LOG_FLUSH
#else
#define TEST_LOG printf
#define TEST_LOG_ERROR printf
#define TEST_LOG_FLUSH fflush(NULL)
#endif

// Select the tests to execute, list order below is same as they will be
// executed. Note that, all settings below will be overriden by sub-API
// settings in engine_configurations.h.
#define _TEST_BASE_
#define _TEST_RTP_RTCP_
#define _TEST_HARDWARE_
#define _TEST_CODEC_
#define _TEST_DTMF_
#define _TEST_VOLUME_
#define _TEST_AUDIO_PROCESSING_
#define _TEST_FILE_
#define _TEST_NETWORK_
#define _TEST_CALL_REPORT_
#define _TEST_VIDEO_SYNC_
#define _TEST_ENCRYPT_
#define _TEST_NETEQ_STATS_
#define _TEST_XMEDIA_

#define TESTED_AUDIO_LAYER kAudioPlatformDefault
//#define TESTED_AUDIO_LAYER kAudioLinuxPulse

// #define _ENABLE_VISUAL_LEAK_DETECTOR_ // Enables VLD to find memory leaks
// #define _ENABLE_IPV6_TESTS_      // Enables IPv6 tests in network xtest
// #define _USE_EXTENDED_TRACE_     // Adds unique trace files for extended test
// #define _MEMORY_TEST_

// Enable this when running instrumentation of some kind to exclude tests
// that will not pass due to slowed down execution.
// #define _INSTRUMENTATION_TESTING_

// Exclude (override) API tests given preprocessor settings in
// engine_configurations.h
#ifndef WEBRTC_VOICE_ENGINE_CODEC_API
#undef _TEST_CODEC_
#endif
#ifndef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
#undef _TEST_VOLUME_
#endif
#ifndef WEBRTC_VOICE_ENGINE_DTMF_API
#undef _TEST_DTMF_
#endif
#ifndef WEBRTC_VOICE_ENGINE_RTP_RTCP_API
#undef _TEST_RTP_RTCP_
#endif
#ifndef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
#undef _TEST_AUDIO_PROCESSING_
#endif
#ifndef WEBRTC_VOICE_ENGINE_FILE_API
#undef _TEST_FILE_
#endif
#ifndef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
#undef _TEST_VIDEO_SYNC_
#endif
#ifndef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
#undef _TEST_ENCRYPT_
#endif
#ifndef WEBRTC_VOICE_ENGINE_HARDWARE_API
#undef _TEST_HARDWARE_
#endif
#ifndef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
#undef _TEST_XMEDIA_
#endif
#ifndef WEBRTC_VOICE_ENGINE_NETWORK_API
#undef _TEST_NETWORK_
#endif
#ifndef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
#undef _TEST_NETEQ_STATS_
#endif
#ifndef WEBRTC_VOICE_ENGINE_CALL_REPORT_API
#undef _TEST_CALL_REPORT_
#endif

// Some parts can cause problems while running Insure
#ifdef __INSURE__
#define _INSTRUMENTATION_TESTING_
#undef WEBRTC_SRTP
#endif

// Time in ms to test each packet size for each codec
#define CODEC_TEST_TIME 400

#define MARK() TEST_LOG("."); fflush(NULL);             // Add test marker
#define ANL() TEST_LOG("\n")                            // Add New Line
#define AOK() TEST_LOG("[Test is OK]"); fflush(NULL);   // Add OK
#if defined(_WIN32)
#define PAUSE                                      \
    {                                               \
        TEST_LOG("Press any key to continue...");   \
        _getch();                                   \
        TEST_LOG("\n");                             \
    }
#else
#define PAUSE                                          \
    {                                                   \
        TEST_LOG("Continuing (pause not supported)\n"); \
    }
#endif

#define TEST(s)                         \
    {                                   \
        TEST_LOG("Testing: %s", #s);    \
    }                                   \

#ifdef _INSTRUMENTATION_TESTING_
// Don't stop execution if error occurs
#define TEST_MUSTPASS(expr)                                               \
    {                                                                     \
        if ((expr))                                                       \
        {                                                                 \
            TEST_LOG_ERROR("Error at line:%i, %s \n",__LINE__, #expr);    \
            TEST_LOG_ERROR("Error code: %i\n",voe_base_->LastError());    \
        }                                                                 \
    }
#define TEST_ERROR(code)                                                \
    {                                                                   \
        int err = voe_base_->LastError();                               \
        if (err != code)                                                \
        {                                                               \
            TEST_LOG_ERROR("Invalid error code (%d, should be %d) at line %d\n",
                           code, err, __LINE__);
}
}
#else
#define ASSERT_TRUE(expr) TEST_MUSTPASS(!(expr))
#define ASSERT_FALSE(expr) TEST_MUSTPASS(expr)
#define TEST_MUSTFAIL(expr) TEST_MUSTPASS(!((expr) == -1))
#define TEST_MUSTPASS(expr)                                              \
    {                                                                    \
        if ((expr))                                                      \
        {                                                                \
            TEST_LOG_ERROR("\nError at line:%i, %s \n",__LINE__, #expr); \
            TEST_LOG_ERROR("Error code: %i\n", voe_base_->LastError());  \
            PAUSE                                                        \
            return -1;                                                   \
        }                                                                \
    }
#define TEST_ERROR(code) \
    {																                                         \
      int err = voe_base_->LastError();                                      \
      if (err != code)                                                       \
      {                                                                      \
        TEST_LOG_ERROR("Invalid error code (%d, should be %d) at line %d\n", \
                       err, code, __LINE__);                                 \
        PAUSE                                                                \
        return -1;                                                           \
      }															                                         \
    }
#endif  // #ifdef _INSTRUMENTATION_TESTING_
#define EXCLUDE()                                                   \
    {                                                               \
        TEST_LOG("\n>>> Excluding test at line: %i <<<\n\n",__LINE__);  \
    }

#define INCOMPLETE()                                                \
    {                                                               \
        TEST_LOG("\n>>> Incomplete test at line: %i <<<\n\n",__LINE__);  \
    }

#endif // WEBRTC_VOICE_ENGINE_VOE_TEST_DEFINES_H
