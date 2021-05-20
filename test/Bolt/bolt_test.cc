#include <gmock/gtest/gtest.h>
#include <util.h>
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include "db.h"
#include "testBase.h"

static const int SLEEP_WAIT_TIME = 1;
// open an empty file as db
TEST(dbtest, opentest) {
  std::unique_ptr<DB> ptr(new DB(newFileName()));
  int ret = ptr->Open(Options());
  EXPECT_EQ(ret, 0);
  ptr->DbClose();
}

// //re-open a db
// TEST(dbtest, dbtest_reopendb_Test) {
//   auto name = newFileName();
//   std::unique_ptr<DB> db1(new DB);
//   db1->openDB(name, S_IRWXU);
//   int ret = 0;
//   db1->view([&ret](Tx *txn) {
//     ret = txn->isFreelistCheckOK();
//     return ret;
//   });
//   EXPECT_EQ(ret, 0);
//   db1->closeDB();
//   db1.reset();

//   std::unique_ptr<DB> db2(new DB);
//   db2->openDB(name, S_IRWXU);
//   db2->view([&ret](Tx *txn) {
//     ret = txn->isFreelistCheckOK();
//     return ret;
//   });
//   EXPECT_EQ(ret, 0);
//   db2->closeDB();
// }

// test freeList Function free() can adding accord page to pending_
// and release() can release it to ids_
// and allocate() can get contigious pages
TEST(dbtest, freeListTest) {
  std::unique_ptr<freeList> f(new freeList());
  pgid index = 12;
  uint32_t overflow = 3;
  Page p{ .id = index, .flag=0, .count=0, .overflow = overflow };
  f->free(100, &p);
  auto it = f->pending_[100].begin();
  for (uint32_t i = 0; i <= overflow; i++) {
    EXPECT_EQ(index + i, *it++);
  }
  f->release(100);
  auto it_ids = f->ids_.begin();
  for (uint32_t i = 0; i <= overflow; i++) {
    EXPECT_EQ(index + i, *it_ids++);
  }
  pgid pg = f->allocate(4);
  EXPECT_EQ(pg, index);
}