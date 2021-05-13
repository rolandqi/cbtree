// Copyright 2008, Google Inc.
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
//
// Author: wan@google.com (Zhanyong Wan)

#include <iostream>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <glog/logging.h>

// MS C++ compiler/linker has a bug on Windows (not on Windows CE), which
// causes a link error when _tmain is defined in a static library and UNICODE
// is enabled. For this reason instead of _tmain, main function is used on
// Windows. See the following link to track the current status of this bug:
// http://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=394464  // NOLINT
#if GTEST_OS_WINDOWS_MOBILE
# include <tchar.h>  // NOLINT

GTEST_API_ int _tmain(int argc, TCHAR** argv) {
#else
GTEST_API_ int main(int argc, char** argv) {
#endif  // GTEST_OS_WINDOWS_MOBILE
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;  //设置日志消息是否转到标准输出而不是日志文件

  FLAGS_alsologtostderr = true;  //设置日志消息除了日志文件之外是否去标准输出

  FLAGS_colorlogtostderr = true;  //设置记录到标准输出的颜色消息（如果终端支持）

  FLAGS_log_prefix = true;  //设置日志前缀是否应该添加到每行输出

  FLAGS_logbufsecs = 0;  //设置可以缓冲日志的最大秒数，0指实时输出

  FLAGS_max_log_size = 50;  //设置最大日志文件大小（以MB为单位）

  FLAGS_stop_logging_if_full_disk = true;  //设置是否在磁盘已满时避免日志记录到磁盘
  FLAGS_log_dir = "./logs/";
  // google::COUNTER 统计某行代码被执行了多少次

  LOG(INFO) << "Running main() from gmock_main.cc";
  // Since Google Mock depends on Google Test, InitGoogleMock() is
  // also responsible for initializing Google Test.  Therefore there's
  // no need for calling testing::InitGoogleTest() separately.
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
