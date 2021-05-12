#include "tx.h"
#include "db.h"

Tx::Tx() : writeable_(false), managed_(false), db_(nullptr), metaData_(nullptr), rootBucket_(this), writeFlag_(false) {

}

void Tx::init(DB *db) {
  db_ = db;
  // 创建事务就是把db中的元数据赋值一份到tx中，这些元数据写的过程中会变化，为了保护之前的数据一致性不被破坏，这里需要拷贝一份新数据，事务提交之后使用tx中的数据在把db中数据覆盖一遍。 
  metaData_ = db_->getMeta()->clone(); //对db中的mate做个快照
  rootBucket_.setBucketHeader(metaData_->root_);
  if (writeable_) {
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
  if (writeable_) {
    db_->getFreeList()->rollback(metaData_->txid_);
    db_->getFreeList()->reload(db_->getPagePtr(metaData_->freeListPageNumber_));
  } // 只有写事务需要rollback
  return 0;
}

int Tx::commit() {
  // if (managed_) {
  //   return -1;
  // }
  // if (db_ == nullptr || !isWritable()) {
  //   return -1;
  // }

  // // recursively merge nodes of which number of elements is below threshold
  // rootBucket_.rebalance();
  // if (rootBucket_.spill()) {
  //   rollback();
  //   return -1;
  // }

  // metaData->rootBucketHeader.rootPageId = rootBucket_.getRootPage();
  // auto pgid = metaData->totalPageNumber;

  // free(metaData->txnId, db->pagePointer(metaData->freeListPageNumber));
  // auto page = allocate((db->freeListSerialSize() / boltDB_CPP::DB::getPageSize()) + 1);
  // if (page == nullptr) {
  //   rollback();
  //   return -1;
  // }
  // if (db->getFreeLIst().write(page)) {
  //   rollback();
  //   return -1;
  // }

  // metaData->freeListPageNumber = page->pageId;

  // if (metaData->totalPageNumber > pgid) {
  //   if (db->grow((metaData->totalPageNumber + 1) * db->getPageSize())) {
  //     rollback();
  //     return -1;
  //   }
  // }

  // if (write() != 0) {
  //   rollback();
  //   return -1;
  // }

  // if (writeMeta() != 0) {
  //   rollback();
  //   return -1;
  // }

  // for (auto &item : commitHandlers) {
  //   item();
  // }

  return 0;
}