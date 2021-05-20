#ifndef BUCKET_H_
#define BUCKET_H_

#include "type.h"
#include "node.h"
#include "cursor.h"
#include <unordered_map>
#include <functional>

const uint32_t MAXKEYSIZE = 32768;
const uint32_t MAXVALUESIZE = (1L << 31) - 2;
const double MINFILLPERCENT = 0.1;
const double MAXFILLPERCENT = 1.0;
const double DEFAULTFILLPERCENT = 0.5;

struct bucketHeader {
  pgid root;         // 某个Bucket的根数据页表
  uint64_t sequence; // 自增
  void reset() {
    root = 0;
    sequence = 0;
  }
};

class Bucket {
public:
  explicit Bucket(TxPtr tx_ptr);
  void setBucketHeader(bucketHeader bucket) { bucketHeader_ = bucket; }
  pgid getRootPage() const { return bucketHeader_.root; }
  void setTx(TxPtr tx) { this->tx_ = tx; }
  TxPtr getTx() const { return tx_; }
  NodePtr getCachedNode(pgid pgid);
  void eraseCachedNode(pgid pgid) { nodes_.erase(pgid); }
  void dereference();
  Bucket *openBucket(Item value);
  Bucket *newBucket(TxPtr tx);
  Bucket *createBucket(const Item &key);
  Bucket *createBucketIfNotExists(const Item &key);
  int deleteBucket(const Item &key);
  NodePtr getNode(pgid pgid, NodePtr parentNode);
  uint32_t getTotalPageNumber() const;
  double getFillPercent() const { return fillPercent_; }
  void rebalance();
  bool isInlineable();
  uint32_t maxInlineBucketSize();
  bool spill();
  Bucket *getBucketByName(const Item &searchKey);
  Cursor *createCursor();
  void free();
  int for_each(std::function<int(const Item &, const Item &)> fn);
  void for_each_page(std::function<void(Page *, int)> fn);
  void for_each_page_node(std::function<void(Page *, NodePtr, int)> fn);
  void for_each_page_node_impl(pgid pgid, int depth,
                               std::function<void(Page *, NodePtr, int)> fn);
  Item write();
  uint32_t getTotalPageNumber();
  void getPageNode(pgid pageId, NodePtr &node, Page *&page);
  bool isWritable() const;

private:
  bucketHeader bucketHeader_;
  TxPtr tx_;                              // 关联的事务
  unordered_map<Item, Bucket *> buckets_; // 当前bucket的子bucket
  Page *page_; // 当前bucket的page信息,只有inline bucket这个值才有意义
  NodePtr rootNode_;                   // B+树根节点
  unordered_map<pgid, NodePtr> nodes_; // 已经缓存的node
  double fillPercent_;                 // 分裂水位、阈值
  NodePtr parentNode_ = nullptr;
};

const uint32_t BUCKETHEADERSIZE = sizeof(bucketHeader);

#endif // BUCKET_H_
