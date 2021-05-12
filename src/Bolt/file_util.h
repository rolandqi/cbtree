// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef FILEUTIL_H_
#define FILEUTIL_H_

#include "string_piece.h"

long GetAvailableSpace(const char *path);
long GetAvailableSpace(int fd);

// read small file < 64KB
class ReadSmallFile {
public:
  ReadSmallFile(StringArg filename);
  ~ReadSmallFile();

  // return errno
  template <typename String>
  int readToString(int maxSize, String *content, int64_t *fileSize,
                   int64_t *modifyTime, int64_t *createTime);

  /// Read at maxium kBufferSize into buf_
  // return errno
  int readToBuffer(int *size);

  const char *buffer() const { return buf_; }

  static const int kBufferSize = 64 * 1024;

private:
  int fd_;
  int err_;
  char buf_[kBufferSize];
};

// read the file content, returns errno if error happens.
template <typename String>
int readFile(StringArg filename, int maxSize, String *content,
             int64_t *fileSize = NULL, int64_t *modifyTime = NULL,
             int64_t *createTime = NULL) {
  ReadSmallFile file(filename);
  return file.readToString(maxSize, content, fileSize, modifyTime, createTime);
}

// not thread safe
class AppendFile {
public:
  explicit AppendFile(StringArg filename);

  ~AppendFile();

  void DummyIncWrittenBytes(const size_t len);
  void append(const char *logline, const size_t len);

  void flush();

  size_t writtenBytes() const { return writtenBytes_; }
  FILE *fp() const { return fp_; }

private:
  size_t write(const char *logline, size_t len);

  FILE *fp_;
  char buffer_[64 * 1024];
  size_t writtenBytes_;
};

#endif // FILEUTIL_H_
