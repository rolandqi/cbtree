#include <gmock/gtest/gtest.h>
#include <util.h>
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include "db.h"
#include "testBase.h"

static const int SLEEP_WAIT_TIME = 1;
//open an empty file as db
TEST(dbtest, opentest) {
  std::unique_ptr<DB> ptr(new DB(newFileName()));
  cout << "testing!!!" << endl;
  int ret = ptr->Open(Options());
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(ret, 1);
  ptr->DbClose();
}
TEST(dbtest, opentest1) {
  int ret = 1;
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(ret, 1);
}
