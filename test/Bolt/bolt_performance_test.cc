#include <util.h>
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include <sys/time.h>
#include <stdlib.h> /* srand, rand */
#include "db.h"
#include "testBase.h"

static std::string bucketname = "roland_test";
static uint64_t max_recursion = 10;

uint64_t usec_now() // 获取当前时间, us
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

void test_8byte_random_insertion(std::shared_ptr<DB> db) {
  auto func = [](TxPtr tx)->int {
    uint64_t intervals = 0, begin, end;
    int ret;
    auto b = tx->getBucket(bucketname);
    for (uint64_t i = 0; i < max_recursion; ++i) {
      std::ostringstream ss;
      ss << std::setw(8) << std::setfill('0') << (rand() % max_recursion);
      std::string str = ss.str();

      begin = usec_now();

      ret = b->put(str, str);

      end = usec_now();
      intervals += end - begin;
      if (ret != 0) {
        LOG(ERROR) << "insertion failed!";
        return -1;
      }
    }
    LOG(WARNING) << "finishing 8byte_random_insertion with " << max_recursion
                 << " recursion, time used(usec): " << intervals;
    LOG(WARNING) << "insertion per second: " << max_recursion * 1000000 /
                                                    intervals;
    return 0;
  };
  int ret = db->update(func);
  if (ret != 0) {
    LOG(ERROR) << "test_8byte_random_insertion failed!";
  }
}

void test_8byte_random_transaction(std::shared_ptr<DB> db) {
  uint64_t intervals = 0, begin, end;
  int ret;

  auto func = [](const Item & str, TxPtr tx)->int {
    auto b = tx->getBucket(bucketname);

    int ret = b->put(str, str);

    if (ret != 0) {
      LOG(ERROR) << "insertion failed!";
      return -1;
    }
    return 0;
  };

  for (uint64_t i = 0; i < max_recursion; ++i) {
    std::ostringstream ss;
    ss << std::setw(8) << std::setfill('0') << (rand() % max_recursion);
    Item str = Item(ss.str());
    auto func_1 = std::bind(func, str, std::placeholders::_1);
    begin = usec_now();

    ret = db->update(func_1);
    if (ret != 0) {
      LOG(ERROR) << "test_8byte_random_transaction failed!";
    }
    end = usec_now();
    intervals += end - begin;
  }
  LOG(WARNING) << "finishing test_8byte_random_transaction with "
               << max_recursion << " recursion, time used(usec): " << intervals;
  LOG(WARNING) << "transaction per second: " << max_recursion * 1000000 /
                                                    intervals;
}

void test_8byte_seq_insertion(std::shared_ptr<DB> db) {
  auto func = [](TxPtr tx)->int {
    uint64_t intervals = 0, begin, end;
    int ret;
    auto b = tx->getBucket(bucketname);
    for (uint64_t i = 0; i < max_recursion; ++i) {
      std::ostringstream ss;
      ss << std::setw(8) << std::setfill('0') << i;
      std::string str = ss.str();

      begin = usec_now();

      ret = b->put(str, str);

      end = usec_now();
      intervals += end - begin;
      if (ret != 0) {
        LOG(ERROR) << "insertion failed!";
        return -1;
      }
    }
    LOG(WARNING) << "finishing test_8byte_seq_insertion with " << max_recursion
                 << " recursion, time used(usec): " << intervals;
    LOG(WARNING) << "insertion per second: " << max_recursion * 1000000 /
                                                    intervals;
    return 0;
  };
  int ret = db->update(func);
  if (ret != 0) {
    LOG(ERROR) << "test_8byte_seq_insertion failed!";
  }
}

void test_8byte_seq_transaction(std::shared_ptr<DB> db) {
  uint64_t intervals = 0, begin, end;
  int ret;

  auto func = [](const Item & str, TxPtr tx)->int {
    auto b = tx->getBucket(bucketname);

    int ret = b->put(str, str);

    if (ret != 0) {
      LOG(ERROR) << "insertion failed!";
      return -1;
    }
    return 0;
  };

  for (uint64_t i = 0; i < max_recursion; ++i) {
    std::ostringstream ss;
    ss << std::setw(8) << std::setfill('0') << i;
    Item str = Item(ss.str());
    auto func_1 = std::bind(func, str, std::placeholders::_1);
    begin = usec_now();

    ret = db->update(func_1);
    if (ret != 0) {
      LOG(ERROR) << "test_8byte_seq_transaction failed!";
    }

    end = usec_now();
    intervals += end - begin;
  }
  LOG(WARNING) << "finishing test_8byte_seq_transaction with " << max_recursion
               << " recursion, time used(usec): " << intervals;
  LOG(WARNING) << "transaction per second: " << max_recursion * 1000000 /
                                                    intervals;
}

void test_8byte_query_transaction(std::shared_ptr<DB> db) {
  uint64_t intervals = 0, begin, end;

  auto func = [](const Item & str, TxPtr tx)->int {
    auto b = tx->getBucket(bucketname);

    auto ret = b->get(str);

    if (ret.empty()) {
      LOG(ERROR) << "query failed!" << str.data_;
      return -1;
    }
    return 0;
  };
  int ret = 0;
  for (uint64_t i = 0; i < max_recursion; ++i) {
    std::ostringstream ss;
    ss << std::setw(8) << std::setfill('0') << i;
    Item str = Item(ss.str());
    auto func_1 = std::bind(func, str, std::placeholders::_1);
    begin = usec_now();

    ret = db->view(func_1);
    if (ret != 0) {
      LOG(FATAL) << "test_8byte_query_transaction failed!";
    }

    end = usec_now();
    intervals += end - begin;
  }
  LOG(WARNING) << "finishing test_8byte_query_transaction with "
               << max_recursion << " recursion, time used(usec): " << intervals;
  LOG(WARNING) << "transaction per second: " << max_recursion * 1000000 /
                                                    intervals;
}

GTEST_API_ int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true; //设置日志消息是否转到标准输出而不是日志文件

  FLAGS_alsologtostderr = true; //设置日志消息除了日志文件之外是否去标准输出

  FLAGS_colorlogtostderr = true; //设置记录到标准输出的颜色消息（如果终端支持）

  FLAGS_log_prefix = true; //设置日志前缀是否应该添加到每行输出

  FLAGS_logbufsecs = 0; //设置可以缓冲日志的最大秒数，0指实时输出

  FLAGS_max_log_size = 50; //设置最大日志文件大小（以MB为单位）

  FLAGS_stop_logging_if_full_disk =
      true; //设置是否在磁盘已满时避免日志记录到磁盘
  FLAGS_minloglevel = google::GLOG_WARNING; //记录的最小级别
  // FLAGS_minloglevel = google::GLOG_INFO; //记录的最小级别
  FLAGS_log_dir = "./logs/";
  // google::COUNTER 统计某行代码被执行了多少次

  if (argc >= 2) {
    max_recursion = atoi(argv[1]);
    LOG(INFO) << "set max recursion: " << max_recursion;
  }

  /* initialize random seed: */
  srand(time(NULL));

  LOG(INFO) << "bolt performance test begin.";

  std::shared_ptr<DB> db = make_shared<DB>(newFileName());
  int ret = db->Open(Options());
  if (ret != 0) {
    LOG(FATAL) << "open DB failed!";
    return -1;
  }
  auto func = [](TxPtr tx)->int {
    auto b = tx->createBucket(bucketname);
    return b != nullptr ? 0 : -1;
  };
  ret = db->update(func);
  if (ret != 0) {
    LOG(FATAL) << "create bucket failed!";
    return -1;
  }
  // LOG(WARNING) << "test_8byte_random_insertion.";
  // test_8byte_random_insertion(db);
  // LOG(WARNING) << "test_8byte_random_transaction.";
  // test_8byte_random_transaction(db);
  LOG(WARNING) << "test_8byte_seq_insertion.";
  test_8byte_seq_insertion(db);
  // LOG(WARNING) << "test_8byte_seq_transaction.";
  // test_8byte_seq_transaction(db);
  LOG(WARNING) << "test_8byte_query_transaction.";
  test_8byte_query_transaction(db);

  db->DbClose();
  return 0;
}
