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
//   db1->view([&ret](Txn *txn) {
//     ret = txn->isFreelistCheckOK();
//     return ret;
//   });
//   EXPECT_EQ(ret, 0);
//   db1->closeDB();
//   db1.reset();

//   std::unique_ptr<DB> db2(new DB);
//   db2->openDB(name, S_IRWXU);
//   db2->view([&ret](Txn *txn) {
//     ret = txn->isFreelistCheckOK();
//     return ret;
//   });
//   EXPECT_EQ(ret, 0);
//   db2->closeDB();
// }