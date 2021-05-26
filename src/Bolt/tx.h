#ifndef TX_H_
#define TX_H_

#include "type.h"
#include "meta.h"
#include "bucket.h"

struct meta;

struct TxStat {
  uint64_t pageCount = 0;
  uint64_t pageAlloc = 0; // in bytes

  uint64_t cursorCount = 0;

  uint64_t nodeCount = 0;
  uint64_t nodeDereferenceCount = 0;

  uint64_t rebalanceCount = 0;
  uint64_t rebalanceTime = 0;

  uint64_t splitCount = 0;
  uint64_t spillCount = 0;
  uint64_t spillTime = 0;

  uint64_t writeCount = 0;
  uint64_t writeTime = 0;
};

// 代表某个只读read-only或者读写read-write事务
class Tx : public std::enable_shared_from_this<Tx> {
public:
  Tx();
  ~Tx() {}
  Tx(const Tx &) = delete;
  Tx &operator=(const Tx &) = delete;
  bool isWritable() const { return writable_; }
  void setWriteable(bool writeable) { writable_ = writeable; }
  void increaseCurserCount() { stats_.cursorCount++; }
  void increaseNodeCount() { stats_.nodeCount++; }
  Page *allocate(uint32_t count);
  Page *getPage(pgid pageId);
  // 只有写事务在提交事务时，其在事务期间受到操作的数据才需要重新分配新的page来存储
  // 而他们原本所在的page会被pending，在事务完成期间并不会被释放到ids里
  int commit();
  void init(DB *db);
  int rollback();
  Bucket *getBucket(const Item &name);
  Bucket *createBucket(const Item &name);
  Bucket *createBucketIfNotExists(const Item &name);
  int deleteBucket(const Item &name);
  void free(txid tid, Page *Page);
  txid getTxId() { return metaData_->txid_; }
  uint32_t getTotalPageNumber() { return metaData_->totalPageNumber_; }
  meta *getMeta() { return metaData_; }
  void for_each_page(pgid pageId, int depth, std::function<void(Page *, int)>);
  void addCommitHandle(std::function<void()> fn) {
    commitHandlers_.push_back(fn);
  }
  int writeMeta();
  int write();
  // int isFreelistCheckOK();
  // bool isBucketsRemainConsistent(Bucket &bucket, std::map<pgid, Page *>
  // &reachable,
  //                                std::map<pgid, bool> &freed);
private:
  bool writable_;
  bool managed_;
  DB *db_;
  meta *metaData_;
  shared_ptr<Bucket> rootBucket_;                   // meta表中的根bucket
  std::unordered_map<pgid, Page *> dirtyPageTable_; // 只有写事务需要
  std::vector<std::function<void()> > commitHandlers_;
  TxStat stats_;
  // MemoryPool pool_;
  friend class DB;
  friend class Bucket;
  friend class Cursor;
};

#endif // TX_H_