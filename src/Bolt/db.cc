#include "db.h"
#include "bucket.h"
#include "page.h"
#include "util.h"
#include <file_util.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <cstring>
#include <utility>
#include <sys/mman.h>
#include <limits.h>

const uint64_t MAXMAPSIZE = 0x7FFFFFFF; // 2GB on x86
// const uint64_t MAXMAPSIZE = 0xFFFFFFFFFFFF; // 256TB on x86_64
// const uint64_t MAXALLOCSIZE = 0xFFFFFFF;  // used on x86
const uint64_t MAXALLOCSIZE = 0x7FFFFFFF; // on x86_64 used when creating
// array pointers x86/x86_64 will not break on unaligned load/store
constexpr int DEFAULTALLOCSIZE = 16 * 1024 * 1024;

constexpr uint64_t MAXMMAPSTEP = 1 << 30; // 1GB used when remapping the mmap
const int DEFAULTMAXBATCHSIZE = 1000;
constexpr int DEFAULTMAXBATCHDELAY = 10; // 单位ms
const uint32_t MAGIC = 0xED0CDAED;
const int VERSION = 1;
constexpr int DEFAULTPAGESIZE = 4096;
uint32_t MAXOBJPOOLSIZE = 10240;

bool meta::validate() {
  if (this->magic_ != MAGIC || this->version_ != VERSION) {
    return false;
  }
  return checksum_ != 0 ? checksum_ == sum64() : true;
}

DB::DB(const string &path)
    : StrictMode_(false), NoSync_(false), NoGrowSync_(false), path_(path),
      file_(0), lock_file_(path_ + "_lock"), dataref_(nullptr), data_(nullptr),
      freeList_(new freeList()), bucketPool_(MAXOBJPOOLSIZE),
      cursorPool_(MAXOBJPOOLSIZE), nodePool_(MAXOBJPOOLSIZE), batchMu_(),
      rwLock_(), metaLock_(), mmapLock_(), statLock_(), readOnly_(false) {
  assert(pthread_rwlock_init(&mmapLock_, NULL) == 0);
}

DB::~DB() {
  DbClose();
  pthread_rwlock_destroy(&mmapLock_);
}

int DB::Open(const Options &options) {
  if (file_ > 0) {
    LOG(INFO) << "file already opened!";
    return 0;
  }
  // 文件锁，不允许两个DB打开同一个文件
  if (!lock_file_.lock()) {
    LOG(INFO) << "acquire file lock failed!";
    return -1;
  }

  opened_ = true;
  NoGrowSync_ = options.NoGrowSync;
  MmapFlags_ = options.MmapFlags;
  MaxBatchSize_ = DEFAULTMAXBATCHSIZE;
  MaxBatchDelay_ = DEFAULTMAXBATCHDELAY;
  AllocSize_ = DEFAULTALLOCSIZE;

  int flag = O_CREAT; //  | O_DIRECT
  if (options.ReadOnly) {
    readOnly_ = true;
    flag |= O_RDONLY;
  } else {
    flag |= O_RDWR;
  }
  file_ = ::open(path_.c_str(), flag);
  if (-1 == file_) {
    LOG(ERROR) << "open file failed!";
    return -1;
  }
  filesz_ = GetFileSize(file_);
  if (filesz_ == 0) {
    // first time to init a DB
    if (Init() != 0) {
      LOG(ERROR) << "init DB failed!";
      DbClose();
      return -1;
    }
  }
  char buf[0x1000]{ 0 };
  auto ret = ::pread(file_, buf, sizeof(buf), 0);
  if (ret == -1) {
    LOG(ERROR) << "get meta page failed upon open db!";
    DbClose();
    return -1;
  }
  auto m = reinterpret_cast<meta *>(pageInBuffer(buf, sizeof(buf), 0)->ptr);
  if (m->validate()) {
    pageSize_ = DEFAULTPAGESIZE;
  } else {
    pageSize_ = m->pageSize_;
  }
  if (initMeta(options.InitialMMapSize) == -1) {
    LOG(ERROR) << "init Meta failed!";
    DbClose();
    return -1;
  }

  freeList_->read(getPagePtr(getMeta()->freeListPageNumber_));
  return 0;
}

meta *DB::getMeta() {
  auto m0 = meta0_;
  auto m1 = meta1_;
  if (m0->txid_ < m1->txid_) {
    m0 = meta1_;
    m1 = meta0_;
  }
  if (m0->validate()) {
    return m0;
  }
  if (m1->validate()) {
    return m1;
  }
  assert(false);
  return nullptr;
}

int DB::initMeta(uint32_t InitialMMapSize) {
  assert(getMmapWLock());
  if (filesz_ < pageSize_ * 2) {
    unlockMmapLock();
    return -1;
  }
  LOG(INFO) << "initMeta filesz_= " << filesz_;
  auto targetSize = std::max(filesz_, InitialMMapSize);
  if (getMmapSize(targetSize) != 0) {
    LOG(ERROR) << "get Mmap file size failed!";
    unlockMmapLock();
    return -1;
  }

  // Dereference all mmap references before unmapping.
  if (rwtx_) {
    rwtx_->rootBucket_->dereference();
  }
  // unmap previous files
  if (!DbMunmap()) {
    LOG(ERROR) << "un-map previous file failed!";
    unlockMmapLock();
    return -1;
  }
  // mmap file
  if (!mmapDbFile(targetSize)) {
    LOG(ERROR) << "mmap new file failed!";
    unlockMmapLock();
    return -1;
  }

  meta0_ = getPagePtr(0)->metaPtr();
  meta1_ = getPagePtr(1)->metaPtr();

  if (!meta0_->validate() && !meta1_->validate()) {
    LOG(ERROR) << "Validate meta failed!";
    unlockMmapLock();
    return -1;
  }
  unlockMmapLock();
  return 0;
}

Page *DB::getPagePtr(pgid pgid) {
  return reinterpret_cast<Page *>(&data_[pgid * pageSize_]);
}

bool DB::mmapDbFile(int targetSize) {
  void *ptr = ::mmap(nullptr, targetSize, PROT_READ, MAP_SHARED, file_, 0);
  if (ptr == MAP_FAILED) {
    return false;
  }
  ::madvise(ptr, targetSize, MADV_RANDOM);
  data_ = reinterpret_cast<decltype(data_)>(ptr);
  dataref_ = ptr;
  datasz_ = targetSize;
  return true;
}

bool DB::DbMunmap() {
  if (!dataref_) {
    return true;
  }
  LOG(INFO) << "current dataref_: " << dataref_;
  int ret = ::munmap(dataref_, datasz_);
  if (ret == -1) {
    LOG(ERROR) << "munmap failed!";
    return false;
  }
  data_ = nullptr;
  dataref_ = nullptr;
  datasz_ = 0;
  return true;
}

// mmapSize determines the appropriate size for the mmap given the current size
// of the database. The minimum size is 32KB and doubles until it reaches 1GB.
// Returns an error if the new mmap size is greater than the max allowed.
int DB::getMmapSize(uint32_t &targetSize) {
  // from 32k up to 1G
  LOG(INFO) << "mmap target size = " << targetSize;
  for (int i = 15; i <= 30; i++) {
    if (targetSize <= (1UL << i)) {
      targetSize = 1UL << i;
      return 0;
    }
  }
  if (targetSize > MAXMAPSIZE) {
    return -1;
  }
  assert(false);
}

void DB::DbClose() {
  std::lock_guard<std::mutex> guard1(rwLock_);
  std::lock_guard<std::mutex> guard2(metaLock_);
  getMmapRLock();

  if (!opened_) {
    return;
  }
  opened_ = false;
  freeList_->reset();
  if (file_) {
    close(file_);
    file_ = -1;
  }
  path_.clear();

  unlockMmapLock();
}

int DB::Init() {
  pageSize_ = getpagesize();
  // 2 meta pages, 1 freeList page, 1 leaf Node page
  char buf[pageSize_ * 4]{ 0 };

  // meta
  for (int i = 0; i < 2; ++i) {
    Page *p = pageInBuffer(buf, sizeof(buf), i);
    p->id = i;
    p->flag = pageFlags::metaPageFlag;
    meta *m = p->metaPtr();
    m->magic_ = MAGIC;
    m->version_ = VERSION;
    m->pageSize_ = pageSize_;
    m->freeListPageNumber_ = 2;
    m->root_ = bucketHeader{ 3, 0 };
    m->totalPageNumber_ = 4;
    m->txid_ = i;
    m->checksum_ = 0; // TODO(roland):checksum
  }

  // free list
  auto p = pageInBuffer(buf, sizeof(buf), 2);
  p->id = 2;
  p->count = 0;
  p->flag = pageFlags::freelistPageFlag;

  // leaf
  p = pageInBuffer(buf, sizeof(buf), 3);
  p->id = 3;
  p->flag = pageFlags::leafPageFlag;
  p->count = 0;

  size_t size = writeAt(buf, sizeof(buf), 0);
  if (size != sizeof(buf)) {
    LOG(ERROR) << "write db file failed!";
    return -1;
  }
  if (!fileSync()) {
    LOG(ERROR) << "sync db file failed!";
  }
  filesz_ = size;
  return 0;
}

Page *DB::pageInBuffer(char *ptr, size_t length, pgid pageId) {
  assert(length > pageId * pageSize_);
  return reinterpret_cast<Page *>(ptr + pageId * pageSize_);
}

Page *DB::allocate(uint32_t numPages, TxPtr tx) {
  uint32_t len = numPages * pageSize_;
  assert(numPages < 0x1000);
  LOG(INFO) << "allocating len: " << len;
  // 操作在内存，现在不持久化
  auto ptr = reinterpret_cast<Page *>(new char[len]); // TODO(roland): mempool
  ptr->overflow = numPages - 1;
  pgid pg = freeList_->allocate(numPages);
  if (pg != 0) {
    ptr->id = pg;
    return ptr;
  }
  // need to expand mmap file here
  LOG(INFO) << "not free Page, try to mmap new Page!";
  ptr->id = rwtx_->getMeta()->totalPageNumber_;
  int minLen = (ptr->id + numPages + 1) * pageSize_;
  if (minLen > datasz_) {
    struct stat stat1;
    {
      auto ret = fstat(file_, &stat1);
      if (ret == -1) {
        LOG(FATAL) << "syscall fstat failed!";
        delete ptr;
        return nullptr;
      }
    }
    if (initMeta(minLen)) {
      LOG(ERROR) << "reallocate file initMeta failed!";
      delete ptr;
      return nullptr;
    }
  }

  rwtx_->getMeta()->totalPageNumber_ += numPages;
  LOG(INFO) << "allocate more page successed!";
  return ptr;
}

int DB::update(std::function<int(TxPtr tx)> fn) {
  TxPtr tx = beginRWTx();
  if (tx == nullptr) {
    LOG(ERROR) << "construct update transaction failed!";
    return -1;
  }
  tx->managed_ = true;
  int ret = fn(tx);
  tx->managed_ = false;
  if (ret != 0) {
    LOG(ERROR) << "user intput returned false!";
    tx->rollback();
    return -1;
  }

  auto result = tx->commit();
  closeTx(tx);
  return result;
}

void DB::closeTx(TxPtr tx) {
  if (tx == nullptr) {
    return;
  }
  if (rwtx_ && rwtx_ == tx) {
    resetRWTX();
    writerLeave(); // 解锁
  } else {
    removeTx(tx);
  }
}

void DB::resetRWTX() { rwtx_ = nullptr; }

void DB::writerLeave() { rwLock_.unlock(); }

void DB::removeTx(TxPtr tx) {
  unlockMmapLock();
  std::lock_guard<std::mutex> guard(metaLock_);

  for (auto iter = txs_.begin(); iter != txs_.end(); iter++) {
    if (*iter == tx) {
      txs_.erase(iter);
      return;
    }
  }
  LOG(WARNING) << "tx not found!";
}

int DB::view(std::function<int(TxPtr tx)> fn) {
  auto tx = beginTx();
  if (tx == nullptr) {
    return -1;
  }

  tx->managed_ = true;

  int ret = fn(tx);

  tx->managed_ = false;

  if (ret != 0) {
    tx->rollback();
    closeTx(tx);
    return -1;
  }

  tx->rollback();
  closeTx(tx);
  return 0;
}

TxPtr DB::beginRWTx() {
  if (readOnly_) {
    return nullptr;
  }

  // 本函数内不会解锁，只有在commit/rollback后才会接锁。保证同一时间只有一个RW
  // transaction.
  rwLock_.lock();

  // MDL
  std::lock_guard<std::mutex> guard(metaLock_);

  if (!opened_) {
    rwLock_.unlock();
    return nullptr;
  }
  rwtx_.reset(new Tx());
  rwtx_->setWriteable(true);
  rwtx_->init(this);

  // Free any pages associated with closed read-only transactions.
  // 写事务开始时，其他事务肯定已经完成了
  auto minId = UINT64_MAX;
  for (auto &it : txs_) {
    minId = std::min(minId, it->metaData_->txid_);
  }

  //暂存在pending中的page小于最小txid的都释放到free中
  if (minId > 0) {
    freeList_->release(minId - 1); // 注意这个-1
  }
  return rwtx_;
}

TxPtr DB::beginTx() {
  std::lock_guard<std::mutex> guard(metaLock_);
  getMmapRLock();
  if (!opened_) {
    unlockMmapLock();
    return nullptr;
  }

  txs_.emplace_back(new Tx());
  auto &tx = txs_.back();
  tx->init(this);
  return tx;
}

bool DB::getMmapRLock() { return pthread_rwlock_rdlock(&mmapLock_) == 0; }

bool DB::getMmapWLock() { return pthread_rwlock_wrlock(&mmapLock_) == 0; }

bool DB::unlockMmapLock() { return pthread_rwlock_unlock(&mmapLock_) == 0; }
