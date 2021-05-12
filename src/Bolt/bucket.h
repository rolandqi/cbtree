#ifndef BUCKET_H_
#define BUCKET_H_

#include "node.h"
#include <unordered_map>

class Tx;

const uint32_t MAXKEYSIZE = 32768;
const uint32_t MAXVALUESIZE = (1L << 31) - 2;
const double MINFILLPERCENT = 0.1;
const double MAXFILLPERCENT = 1.0;
const double DEFAULTFILLPERCENT = 0.5;

struct bucket {
  pgid root;         // 某个Bucket的根数据页表
  uint64_t sequence; // 自增
  void reset() {
    root = 0;
    sequence = 0;
  }
};

class Bucket {
public:
  explicit Bucket(Tx *tx_ptr);
  void setBucketHeader(bucket bucket) { bucketHeader_ = bucket; }
  void setTx(Tx *tx) { this->tx_ = tx; }
  Tx *tx() const { return tx_; }
  pgid getRootPageId() const { return bucketHeader_.root; }
  Node *getCachedNode(pgid pgid);
  void eraseCachedNode(pgid pgid) { nodes_.erase(pgid); }
  void dereference();
private:
  bucket bucketHeader_;
  Tx *tx_;                                // 关联的事务
  unordered_map<Item, Bucket *> buckets_; // 当前bucket的子bucket
  page *page_;                            // 当前页的page信息
  Node *rootNode_;                        // B+树根节点
  unordered_map<pgid, Node *> nodes_;     // 已经缓存的node
  float FillPercent_;                     // 分裂水位、阈值
};

#endif // BUCKET_H_
