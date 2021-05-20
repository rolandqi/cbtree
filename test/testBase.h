#ifndef TEST_BASE_H_
#define TEST_BASE_H_
#include "gtest/gtest.h"
#include "db.h"
std::string newFileName() {
  auto ret = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch());
  int64_t currentTime = ret.count();
  return std::to_string(currentTime);
}

struct TmpFile : testing::Test {
  std::unique_ptr<DB> db;
  void SetUp() override {
    // create a tmp db file
    db.reset(new DB(newFileName()));
    auto ret = db->Open(Options());
    assert(ret == 0);
  }
  void TearDown() override {
    // close the tmp db file
    db->DbClose();
  }
};
#endif // TEST_BASE_H_
