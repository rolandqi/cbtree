#ifndef UTIL_H_
#define UTIL_H_

#include "sys/file.h"
#include <glog/logging.h>
#include <iostream>
#include <unistd.h>
#include <string>

using namespace std;

class FlockUtil {
public:
  explicit FlockUtil(const std::string lock_file) : lock_file_(lock_file) {}

  ~FlockUtil() { ::close(fd_); }

  FlockUtil(const FlockUtil &) = delete;
  FlockUtil &operator=(const FlockUtil &) = delete;

  bool lock() {
    fd_ = ::open(lock_file_.c_str(), O_RDWR | O_CREAT, 0600);
    if (fd_ < 0) {
      LOG(INFO) << "open lock file failed, file name:" << lock_file_;
      ;
    }
    if (0 == flock(fd_, LOCK_EX | LOCK_NB)) {
      return true;
    }
    if (errno == EAGAIN) {
      ::close(fd_);
      fd_ = -1;
      return false;
    } else {
      LOG(INFO) << "lock file error";
      ;
      close(fd_);
      fd_ = -1;
      return false;
    }
    return false;
  }

  void unlock() {
    flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
  }

private:
  std::string lock_file_;
  int fd_;
};

#endif // UTIL_H_