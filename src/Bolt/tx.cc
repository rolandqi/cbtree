#include "tx.h"
#include "db.h"

static int file_data_sync(int fd) {
  int ret = fdatasync(fd);
  if (ret != 0) {
    LOG(INFO) << "fdatasync failed for fd: " << fd;
  }
  return ret;
}

Tx::Tx()
    : writable_(false), managed_(false), db_(nullptr), metaData_(nullptr),
      rootBucket_() {
  // rootBucket_.reset(new Bucket(shared_from_this()));
}

void Tx::init(DB *db) {
  rootBucket_.reset(new Bucket(shared_from_this()));
  db_ = db;
  // 创建事务就是把db中的元数据赋值一份到tx中，这些元数据写的过程中会变化，为了保护之前的数据一致性不被破坏，这里需要拷贝一份新数据，事务提交之后使用tx中的数据在把db中数据覆盖一遍。
  metaData_ = db_->getMeta()->clone(); //对db中的mate做个快照
  rootBucket_->setBucketHeader(metaData_->root_);
  if (writable_) {
    metaData_->txid_ += 1;
  }
}

int Tx::rollback() {
  if (managed_) {
    return -1;
  }
  if (db_ == nullptr) {
    return -1;
  }
  if (writable_) {
    db_->getFreeList()->rollback(metaData_->txid_);
    db_->getFreeList()->reload(db_->getPagePtr(metaData_->freeListPageNumber_));
  } // 只有写事务需要rollback
  return 0;
}

int Tx::commit() {
  if (managed_) {
    LOG(ERROR) << "transaction not managed!";
    return -1;
  }
  if (db_ == nullptr || !isWritable()) {
    LOG(ERROR) << "invalid parameter.";
    return -1;
  }

  // Rebalance nodes which have had deletions.
  rootBucket_->rebalance();
  if (!rootBucket_->spill()) {
    LOG(ERROR) << "root bucket spill failed!";
    rollback();
    return -1;
  }

  metaData_->root_.root = rootBucket_->getRootPage();
  auto pageId = metaData_->totalPageNumber_;

  free(metaData_->txid_, db_->getPagePtr(metaData_->freeListPageNumber_));
  auto page = allocate((db_->freeListSerialSize() / 4096) + 1);
  if (page == nullptr) {
    LOG(ERROR) << "can not allcate contigious pages durning commit.";
    rollback();
    return -1;
  }
  // if (db_->getFreeList()->write(page)) {
  //   rollback();
  //   return -1;
  // }
  // 默认write不会出错
  db_->getFreeList()->write(page);

  metaData_->freeListPageNumber_ = page->id;

  if (metaData_->totalPageNumber_ > pageId) {
    if (db_->grow((metaData_->totalPageNumber_ + 1) * db_->getPageSize())) {
      LOG(ERROR) << "grow page failed!";
      rollback();
      return -1;
    }
  }

  if (write() != 0) {
    LOG(ERROR) << "Write commit data failed!";
    rollback();
    return -1;
  }

  if (writeMeta() != 0) {
    LOG(ERROR) << "write committed meta failed!";
    rollback();
    return -1;
  }

  // for now we don't have any commit handles.
  for (auto &item : commitHandlers_) {
    item();
  }

  return 0;
}

Bucket *Tx::getBucket(const Item &name) {
  return rootBucket_->getBucketByName(name);
}

Bucket *Tx::createBucket(const Item &name) {
  return rootBucket_->createBucket(name);
}

Bucket *Tx::createBucketIfNotExists(const Item &name) {
  return rootBucket_->createBucketIfNotExists(name);
}

int Tx::deleteBucket(const Item &name) {
  rootBucket_->deleteBucket(name);
  return 0;
}

void Tx::free(txid tid, Page *page) { db_->getFreeList()->free(tid, page); }

// page returns a reference to the page with a given id.
// If page has been written to then a temporary buffered page is returned.
Page *Tx::getPage(pgid pageId) {
  if (pageId >= metaData_->totalPageNumber_) {
    return nullptr;
  }
  if (!dirtyPageTable_.empty()) {
    // Check the dirty pages first.
    auto iter = dirtyPageTable_.find(pageId);
    if (iter != dirtyPageTable_.end()) {
      return iter->second;
    }
  }
  // Otherwise return directly from the mmap.
  return db_->getPagePtr(pageId);
}

Page *Tx::allocate(uint32_t count) {
  auto ret = db_->allocate(count, shared_from_this());
  // free里面连续的页不够
  if (ret == nullptr) {
    return ret;
  }
  dirtyPageTable_[ret->id] = ret;

  stats_.pageCount++;
  stats_.pageAlloc += count * db_->getPageSize();

  return ret;
}

void Tx::for_each_page(pgid pageId, int depth,
                       std::function<void(Page *, int)> fn) {
  auto p = getPage(pageId);
  fn(p, depth);

  if (p->flag & static_cast<uint32_t>(pageFlags::branchPageFlag)) {
    for (int i = 0; i < p->count; i++) {
      auto element = p->getBranchPageElement(i);
      for_each_page(element->pageId, depth + 1, fn);
    }
  }
}

int Tx::writeMeta() {
  std::vector<char> tmp(4096);
  Page *page = reinterpret_cast<Page *>(tmp.data());
  metaData_->write(page);

  if (db_->writeAt(tmp.data(), tmp.size(), page->id * 4096) != (int)tmp.size()) {
    LOG(ERROR) << "writeAt meta failed!";
    return -1;
  }

  if (!db_->isNoSync()) {
    if (file_data_sync(db_->getFd())) {
      LOG(ERROR) << "meta sync failed!";
      return -1;
    }
  }
  return 0;
}

int Tx::write() {
  std::vector<Page *> pages;
  for (auto item : dirtyPageTable_) {
    pages.push_back(item.second);
  }
  dirtyPageTable_.clear();
  std::sort(pages.begin(), pages.end());

  for (auto p : pages) {
    auto length = (p->overflow + 1) * db_->getPageSize();
    auto offset = p->id * db_->getPageSize();
    if (db_->writeAt(reinterpret_cast<char *>(p), length, offset) != length) {
      return -1;
    }
  }

  if (!db_->isNoSync()) {
    if (file_data_sync(db_->getFd())) {
      return -1;
    }
  }

  return 0;
}

int DB::grow(uint32_t sz) {
  if (sz <= filesz_) {
    return 0;
  }

  if (datasz_ <= AllocSize_) {
    sz = datasz_;
  } else {
    sz += AllocSize_;
  }

  if (!NoGrowSync_ && !readOnly_) {
    // increase file's size
    if (ftruncate(file_, sz)) {
      return -1;
    }
    // make sure that file size is written into metadata
    if (fsync(file_)) {
      return -1;
    }
  }

  filesz_ = sz;
  return 0;
}