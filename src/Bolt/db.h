#ifndef DB_H_
#define DB_H_

#include "type.h"
#include "freeList.h"
#include "meta.h"
#include "bucket.h"
#include "cursor.h"
#include "file_util.h"
#include "util.h"
#include "tx.h"
#include "page.h"
#include <pthread.h>
#include <obj_pool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <cassert>
#include <iostream>
#include <map>
#include <mutex>
#include <stack>
#include <vector>
#include <memory>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef char byte;

struct Options {
  uint64_t timeout;
  bool NoGrowSync;
  bool ReadOnly;
  int MmapFlags;
  uint32_t InitialMMapSize;
  Options()
      : timeout(0), NoGrowSync(false), ReadOnly(false), MmapFlags(0),
        InitialMMapSize(0) {}
};

class DB {
public:
  DB(const string &path);
  ~DB();
  int Open(const Options &options);
  int Init();
  void DbClose();
  Page *pageInBuffer(char *ptr, size_t length, pgid pageId);
  const std::function<ssize_t(char *, size_t, off_t)> writeAt = [this](
      char *buf, size_t len, off_t offset) {
    auto ret = ::pwrite(file_, buf, len, offset);
    if (ret == -1) {
      LOG(INFO) << "pwrite";
    }
    return ret;
  };
  inline bool fileSync() { return fdatasync(file_) == 0; }
  inline freeList *getFreeList() { return freeList_; }
  uint32_t getPageSize() { return pageSize_; }
  int initMeta(uint32_t InitialMMapSize);
  meta *getMeta();
  int getMmapSize(uint32_t &targetSize);
  bool DbMunmap();
  bool mmapDbFile(int targetSize);
  Page *getPagePtr(pgid pgid);
  Page *allocate(uint32_t numPages,
                 TxPtr tx); // 分配numPages个连续的页，返回第一个页的指针
  int update(std::function<int(TxPtr tx)> fn);
  int view(std::function<int(TxPtr tx)> fn);
  TxPtr beginRWTx(); // 数据库不支持update事务并发
  TxPtr beginTx();
  void closeTx(TxPtr tx);
  void removeTx(TxPtr tx);
  bool getMmapRLock(); // 阻塞，没有超时时间
  bool getMmapWLock(); // 阻塞
  bool unlockMmapLock();
  void resetRWTX();
  void writerLeave();
  bool isNoSync() { return NoSync_; }
  uint32_t freeListSerialSize() const { return freeList_->size(); }
  int grow(uint32_t sz);
  int getFd() const { return file_; }

private:
  bool StrictMode_;
  bool NoSync_;
  bool NoGrowSync_;
  int MmapFlags_;
  int MaxBatchSize_;
  uint64_t MaxBatchDelay_; // batch开始前最大是延时
  int AllocSize_;
  string path_;
  int file_; // db file fd
  FlockUtil lock_file_;
  void *dataref_; // read only mmap file 利用mmap, 把分页的管理交给了操作系统
  char *data_;
  int datasz_;
  uint32_t filesz_;
  meta *meta0_;
  meta *meta1_;
  uint32_t pageSize_;
  bool opened_;
  TxPtr rwtx_;
  vector<TxPtr> txs_;
  freeList *freeList_;
  // Stats stats_;  // for performance
  ObjPool<Bucket> bucketPool_; // 线程安全的对象池
  ObjPool<Cursor> cursorPool_; // 线程安全的对象池
  ObjPool<Node> nodePool_;     // 线程安全的对象池
  std::mutex batchMu_;
  batch *batch_;

  std::mutex rwLock_;
  std::mutex metaLock_;
  /*
  * 读锁和写锁之前互斥，如果已经有了写锁，下一个写锁必须等所有的锁都unlock之后才能持有写锁
  * 如果没有写锁，写锁也需要等所有的锁unlock之后才能持有写锁
  */
  pthread_rwlock_t mmapLock_;
  std::mutex statLock_;
  bool readOnly_;
};

#endif // TYPE_H_