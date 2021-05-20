#include "bucket.h"
#include "tx.h"
#include "meta.h"
#include "db.h"
#include <cstring>
#include "page.h"

Bucket::Bucket(TxPtr tx_ptr)
    : bucketHeader_(), tx_(tx_ptr), buckets_(), page_(nullptr),
      rootNode_(nullptr), nodes_(), fillPercent_(DEFAULTFILLPERCENT) {}

NodePtr Bucket::getCachedNode(pgid pgid) {
  auto it = nodes_.find(pgid);
  if (it != nodes_.end()) {
    return it->second;
  }
  return nullptr;
}

void Bucket::free() {
  if (bucketHeader_.root == 0) {
    return;
  }

  for_each_page_node([this](Page *p, NodePtr n, int) {
    if (p) {
      // 在磁盘里
      tx_->free(tx_->metaData_->txid_, p);
    } else {
      // 在内存里
      assert(n);
      n->free();
    }
  });

  bucketHeader_.root = 0;
}

void Bucket::dereference() {
  if (rootNode_) {
    rootNode_->root()->dereference();
  }

  for (auto it : buckets_) {
    it.second->dereference();
  }
}

// Item中存放的是bucketHeader
Bucket *Bucket::openBucket(Item value) {
  auto child = newBucket(tx_);

  // TODO(roland):memory management
  // if (child->tx_->isWritable()) {
  //   std::memcpy((char *) &child->bucketHeader_, value.c_str(),
  //               sizeof(bucketHeader));
  // } else {
  //   child->bucketHeader =
  //       *(reinterpret_cast<bucketHeader *>(const_cast<char
  // *>(value.pointer)));
  // }

  memmove((char *)&child->bucketHeader_, value.c_str(), sizeof(bucketHeader));
  // bucketHeader *tmp = reinterpret_cast<bucketHeader*>(value.c_str());
  // is this a inline bucket?
  if (child->bucketHeader_.root == 0) {
    child->page_ = reinterpret_cast<Page *>(&(value.c_str())[BUCKETHEADERSIZE]);
  }
  return child;
}

Bucket *Bucket::newBucket(TxPtr tx) {
  auto bucket = new Bucket(tx);
  return bucket;
}

Bucket *Bucket::getBucketByName(const Item &searchKey) {
  auto iter = buckets_.find(searchKey);
  if (iter != buckets_.end()) {
    return iter->second;
  }

  auto cursor = createCursor();
  Item key;
  Item value;
  uint32_t flag = 0;
  cursor->seek(searchKey, key, value, flag);
  if (searchKey.data_ != key.data_ || !(flag & bucketLeafFlag)) {
    LOG(ERROR) << "getBucketByName failed! no bucket exist.";
    return nullptr;
  }

  auto result = openBucket(value);
  buckets_[searchKey] = result;
  return result;
}

Cursor *Bucket::createCursor() {
  tx_->increaseCurserCount();
  auto ret = new Cursor(this);
  LOG(INFO) << "create new cursor: " << ret;
  return ret;
}

// creates a new bucket at the given key and returns the new bucket.
// The bucket instance is only valid for the lifetime of the transaction.
Bucket *Bucket::createBucket(const Item &key) {
  if (tx_->db_ == nullptr || !tx_->writable_ || key.length_ == 0) {
    LOG(ERROR) << "invalid param for createBucket";
    return nullptr;
  }
  auto c = Cursor(this);
  Item k;
  Item v;
  uint32_t flag;
  c.seek(key, k, v, flag);

  if (k == key) {
    if (flag & static_cast<uint16_t>(bucketLeafFlag)) {
      LOG(ERROR) << "key already exists";
      return getBucketByName(key);
    }
    return nullptr;
  }

  // create an empty inline bucket
  Bucket bucket(tx_);
  bucket.rootNode_ = make_shared<Node>(&bucket, nullptr); // root node
  bucket.rootNode_->markLeaf();

  auto putValue = bucket.write();

  // key就是bucket的名字，value就是（bucketheader + node）
  c.getNode()->put(key, key, putValue, 0, bucketLeafFlag);

  // Since subbuckets are not allowed on inline buckets, we need to
  // dereference the inline page, if it exists. This will cause the bucket
  // to be treated as a regular, non-inline bucket for the rest of the tx.
  page_ = nullptr;
  // 通过getBucketByName函数将刚创建的bucket放到buckets_里面去
  return getBucketByName(key);
}

Item Bucket::write() {
  int length = BUCKETHEADERSIZE + rootNode_->size();
  Item item(length);

  // write bucketHeader in the front
  *(reinterpret_cast<bucketHeader *>(item.c_str())) = bucketHeader_;

  // serialize node after bucketHeader
  auto pageInBuffer = (Page *)&(item.c_str())[BUCKETHEADERSIZE];
  rootNode_->write(pageInBuffer);

  return item;
}

Bucket *Bucket::createBucketIfNotExists(const Item &key) {
  auto child = createBucket(key);
  return child;
}

int Bucket::deleteBucket(const Item &key) {
  if (tx_->db_ == nullptr || !tx_->isWritable()) {
    LOG(ERROR) << "invalid parameter!";
    return -1;
  }
  auto c = createCursor();
  Item k;
  Item v;
  uint32_t flag;
  c->seek(key, k, v, flag);
  if (k.data_ != key.data_ || (flag & bucketLeafFlag)) {
    LOG(ERROR) << "delete Bucket failed!";
    return -1;
  }

  auto child = getBucketByName(key);
  auto ret = for_each([&child](const Item &k, const Item &v) {
    if (v.length_ == 0) {
      auto ret = child->deleteBucket(k);
      if (ret != 0) {
        return ret;
      }
    }
    return 0;
  });
  if (ret != 0) {
    return ret;
  }

  // remove cache
  buckets_.erase(key);

  child->nodes_.clear();
  child->rootNode_ = nullptr;
  child->free();

  c->getNode()->del(key);

  return 0;
}

NodePtr Bucket::getNode(pgid pgid, NodePtr parentNode) {
  // return if the node already cached.
  auto it = nodes_.find(pgid);
  if (it != nodes_.end()) {
    return it->second;
  }

  // otherwise make a new node.
  auto node = make_shared<Node>(this, parentNode_);
  if (parentNode_ == nullptr) {
    rootNode_ = node;
  } else {
    parentNode_->addChild(node);
  }

  // Use the inline page if this is an inline bucket.
  if (page_ == nullptr) {
    page_ = tx_->getPage(pgid);
  }
  node->read(page_);
  nodes_[pgid] = node;
  // tx_->stats_.nodeCount++;
  return node;
}

uint32_t Bucket::getTotalPageNumber() {
  return tx_->metaData_->totalPageNumber_;
}

void Bucket::rebalance() {
  for (auto &item : nodes_) {
    item.second->rebalance();
  }

  for (auto &item : buckets_) {
    item.second->rebalance();
  }
}

// inlineable returns true if a bucket is small enough to be written inline
// and if it contains no subbuckets. Otherwise returns false.
bool Bucket::isInlineable() {
  auto r = rootNode_;

  // Bucket must only contain a single leaf node.
  if (r == nullptr || !r->isLeafNode()) {
    return false;
  }

  return r->isinlineable(maxInlineBucketSize());
}

// Returns the maximum total size of a bucket to make it a candidate for
// inlining.
uint32_t Bucket::maxInlineBucketSize() {
  // return tx_->db_->getPageSize() / 4;
  return 1024;
}

// writes all the nodes for this bucket to dirty pages.
bool Bucket::spill() {
  // Spill all child buckets first.
  for (auto item : buckets_) {

    // If the child bucket is small enough and it has no child buckets then
    // write it inline into the parent bucket's page. Otherwise spill it
    // like a normal bucket and make the parent value a pointer to the page.
    // 将children bucket放到当前bucket的page中，形成inline bucket
    auto name = item.first;
    auto child = item.second;

    Item newValue;

    if (child->isInlineable()) {
      // 如果可以inline，就将bucket和node数据都写到newValue中，然后put进去
      child->free();
      newValue = child->write();
    } else {
      if (child->spill()) {
        return -1;
      }

      newValue = Item(reinterpret_cast<char *>(&child->bucketHeader_),
                      sizeof(struct bucketHeader));
    }

    if (child->rootNode_ == nullptr) {
      continue;
    }

    auto c = createCursor();
    Item k;
    Item v;
    uint32_t flag = 0;

    c->seek(name, k, v, flag);

    if (k != name) {
      assert(false);
    }

    if (!(flag & bucketLeafFlag)) {
      assert(false);
    }

    c->getNode()->put(name, name, newValue, 0, bucketLeafFlag);
  }

  if (rootNode_ == nullptr) {
    return 0;
  }

  auto ret = rootNode_->spill();
  if (ret) {
    return ret;
  }

  rootNode_ = rootNode_->root();

  if (rootNode_->getPageId() >= tx_->metaData_->totalPageNumber_) {
    assert(false);
  }

  bucketHeader_.root = rootNode_->getPageId();
  return 0;
}

int Bucket::for_each(std::function<int(const Item &, const Item &)> fn) {
  if (tx_->db_ == nullptr) {
    return -1;
  }
  auto c = createCursor();
  Item k;
  Item v;
  c->first(k, v);
  while (k.length_ != 0) {
    auto ret = fn(k, v);
    if (ret != 0) {
      return ret;
    }
    c->next(k, v);
  }
  return 0;
}

void Bucket::for_each_page(std::function<void(Page *, int)> fn) {
  // inline bucket，只有一个page
  if (page_) {
    fn(page_, 0);
    return;
  }

  tx_->for_each_page(getRootPage(), 0, fn);
}

void Bucket::for_each_page_node(std::function<void(Page *, NodePtr, int)> fn) {
  if (page_) {
    fn(page_, nullptr, 0);
    return;
  }
  for_each_page_node_impl(getRootPage(), 0, fn);
}

void
Bucket::for_each_page_node_impl(pgid pgid, int depth,
                                std::function<void(Page *, NodePtr, int)> fn) {
  NodePtr node;
  Page *page;
  getPageNode(pgid, node, page);

  fn(page, node, depth);
  // Recursively loop over children.
  if (page) {
    if (page->flag & pageFlags::branchPageFlag) {
      for (size_t i = 0; i < page->count; i++) {
        auto element = page->getBranchPageElement(i);
        for_each_page_node_impl(element->pageId, depth + 1, fn);
      }
    }
  } else {
    if (!node->isLeafNode()) {
      for (auto pid : node->branchPageIds()) {
        for_each_page_node_impl(pid, depth + 1, fn);
      }
    }
  }
}

void Bucket::getPageNode(pgid pageId, NodePtr &node, Page *&page) {
  node = nullptr;
  page = nullptr;

  // inline
  if (bucketHeader_.root == 0) {
    if (rootNode_) {
      node = rootNode_;
      return;
    }
    page = page_;
    return;
  }
  if (!nodes_.empty()) {
    // 找到缓存的node之后可以直接返回
    auto iter = nodes_.find(pageId);
    if (iter != nodes_.end()) {
      node = iter->second;
      return;
    }
  }
  page = tx_->getPage(pageId);
  return;
}

bool Bucket::isWritable() const { return tx_->isWritable(); }