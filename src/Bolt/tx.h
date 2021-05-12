#ifndef TX_H_
#define TX_H_

#include "type.h"
#include "meta.h"

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
class Tx {
public:
  Tx();
  ~Tx();
  Tx(const Tx&) = delete;
  bool isWritable() const { return writeable_; }
  void setWriteable(bool writeable) { writeable_ = writeable; }
  void increaseCurserCount() { stats_.cursorCount++; }
  void increaseNodeCount() { stats_.nodeCount++; }
  page *allocate(size_t count);
  page *getPage(pgid pageId);
  // 只有写事务在提交事务时，其在事务期间受到操作的数据才需要重新分配新的page来存储
  // 而他们原本所在的page会被pending，在事务完成期间并不会被释放到ids里
  int commit();
  void init(DB *db);
  int rollback();
  
private:
  bool writeable_;
  bool managed_;
  DB *db_;
  meta *metaData_;
  Bucket rootBucket_;
  std::unordered_map<pgid, page *> dirtyPageTable_;  // 只有写事务需要
  std::vector<std::function<void()>> commitHandlers_;
  bool writeFlag_;
  TxStat stats_;
  // MemoryPool pool_;
  friend class DB;
  friend class Bucket;
  friend class Cursor;
};

#endif // TX_H_