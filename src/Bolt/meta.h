#ifndef META_H_
#define META_H_

#include "type.h"
#include "bucket.h"

struct meta {
  uint32_t magic_;    // “0xED0CDAED”
  uint32_t version_;   // 程序版本号
  uint32_t pageSize_; // 页面大小，默认4k
  uint32_t flags_;    // 保留
  // meta下面的root存储的是整个数据库的root，数据库中其他的table是整个bucket的子bucket
  bucket root_; // 根bucket
  pgid freeListPageNumber_;
  pgid totalPageNumber_; // 总的pgid，即最大页面+1
  txid txid_; // 上一次写数据库的事务ID，每次写数据库时+1，只读不改变
  uint64_t checksum_;
  meta() {};
  bool validate();
  meta *clone();
  uint64_t sum64() { return 0; }
} __attribute__((packed));

#endif