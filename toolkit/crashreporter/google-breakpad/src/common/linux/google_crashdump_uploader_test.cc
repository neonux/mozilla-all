// Copyright (c) 2009, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Unit test for crash dump uploader.

#include "common/linux/google_crashdump_uploader.h"
#include "common/linux/libcurl_wrapper.h"
#include "breakpad_googletest_includes.h"

namespace google_breakpad {

using ::testing::Return;
using ::testing::_;

class MockLibcurlWrapper : public LibcurlWrapper {
 public:
  MOCK_METHOD0(Init, bool());
  MOCK_METHOD2(SetProxy, bool(const std::string& proxy_host,
                              const std::string& proxy_userpwd));
  MOCK_METHOD2(AddFile, bool(const std::string& upload_file_path,
                             const std::string& basename));
  MOCK_METHOD3(SendRequest,
               bool(const std::string& url,
                    const std::map<std::string, std::string>& parameters,
                    std::string* server_response));
};

class GoogleCrashdumpUploaderTest : public ::testing::Test {
};

TEST_F(GoogleCrashdumpUploaderTest, InitFailsCausesUploadFailure) {
  MockLibcurlWrapper m;
  EXPECT_CALL(m, Init()).Times(1).WillOnce(Return(false));
  GoogleCrashdumpUploader *uploader = new GoogleCrashdumpUploader("foobar",
                                                                  "1.0",
                                                                  "AAA-BBB",
                                                                  "",
                                                                  "",
                                                                  "test@test.com",
                                                                  "none",
                                                                  "/tmp/foo.dmp",
                                                                  "http://foo.com",
                                                                  "",
                                                                  "",
                                                                  &m);
  ASSERT_FALSE(uploader->Upload());
}

TEST_F(GoogleCrashdumpUploaderTest, TestSendRequestHappensWithValidParameters) {
  // Create a temp file
  char tempfn[80] = "/tmp/googletest-upload-XXXXXX";
  int fd = mkstemp(tempfn);
  ASSERT_NE(fd, -1);
  close(fd);

  MockLibcurlWrapper m;
  EXPECT_CALL(m, Init()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(m, AddFile(tempfn, _)).WillOnce(Return(true));
  EXPECT_CALL(m,
              SendRequest("http://foo.com",_,_)).Times(1).WillOnce(Return(true));
  GoogleCrashdumpUploader *uploader = new GoogleCrashdumpUploader("foobar",
                                                                  "1.0",
                                                                  "AAA-BBB",
                                                                  "",
                                                                  "",
                                                                  "test@test.com",
                                                                  "none",
                                                                  tempfn,
                                                                  "http://foo.com",
                                                                  "",
                                                                  "",
                                                                  &m);
  ASSERT_TRUE(uploader->Upload());
}


TEST_F(GoogleCrashdumpUploaderTest, InvalidPathname) {
  MockLibcurlWrapper m;
  EXPECT_CALL(m, Init()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(m, SendRequest(_,_,_)).Times(0);
  GoogleCrashdumpUploader *uploader = new GoogleCrashdumpUploader("foobar",
                                                                  "1.0",
                                                                  "AAA-BBB",
                                                                  "",
                                                                  "",
                                                                  "test@test.com",
                                                                  "none",
                                                                  "/tmp/foo.dmp",
                                                                  "http://foo.com",
                                                                  "",
                                                                  "",
                                                                  &m);
  ASSERT_FALSE(uploader->Upload());
}

TEST_F(GoogleCrashdumpUploaderTest, TestRequiredParametersMustBePresent) {
  // Test with empty product name.
  GoogleCrashdumpUploader uploader("",
                                   "1.0",
                                   "AAA-BBB",
                                   "",
                                   "",
                                   "test@test.com",
                                   "none",
                                   "/tmp/foo.dmp",
                                   "http://foo.com",
                                   "",
                                   "");
  ASSERT_FALSE(uploader.Upload());

  // Test with empty product version.
  GoogleCrashdumpUploader uploader1("product",
                                    "",
                                    "AAA-BBB",
                                    "",
                                    "",
                                    "",
                                    "",
                                    "/tmp/foo.dmp",
                                    "",
                                    "",
                                    "");

  ASSERT_FALSE(uploader1.Upload());

  // Test with empty client GUID.
  GoogleCrashdumpUploader uploader2("product",
                                    "1.0",
                                    "",
                                    "",
                                    "",
                                    "",
                                    "",
                                    "/tmp/foo.dmp",
                                    "",
                                    "",
                                    "");
  ASSERT_FALSE(uploader2.Upload());
}
}
