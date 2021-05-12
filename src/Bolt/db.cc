#include "db.h"
#include "bucket.h"
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
    : path_(path), lock_file_(path_ + "_lock"), freeList_(new freeList()), bucketPool_(MAXOBJPOOLSIZE),
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
    cout << "file already opened!" << endl;
    return 0;
  }
  // 文件锁，不允许两个DB打开同一个文件
  if (!lock_file_.lock()) {
    cout << "acquire file lock failed!" << endl;
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
    cout << "open file failed!" << endl;
    return -1;
  }
  filesz_ = GetAvailableSpace(file_);
  if (filesz_ == 0) {
    // first time to init a DB
    if (!Init()) {
      cout << "init DB failed!" << endl;
      DbClose();
      return -1;
    }
  }
  char buf[0x1000]{ 0 };
  auto ret = ::pread(file_, buf, sizeof(buf), 0);
  if (ret == -1) {
    cout << "get meta page failed upon open db!" << endl;
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
  cout << "filesz_= " << filesz_ << endl;
  auto targetSize = std::max(filesz_, InitialMMapSize);
  if (getMmapSize(targetSize) != 0) {
    unlockMmapLock();
    return -1;
  }

  // Dereference all mmap references before unmapping.
  if (rwtx_) {
    rwtx_->rootBucket_.dereference();
  }
  // unmap previous files
  if (!DbMunmap()) {
    unlockMmapLock();
    return -1;
  }
  // mmap file
  if (mmapDbFile(targetSize)) {
    unlockMmapLock();
    return -1;
  }

  meta0_ = getPagePtr(0)->metaPtr();
  meta1_ = getPagePtr(1)->metaPtr();

  if (!meta0_->validate() && !meta1_->validate()) {
    unlockMmapLock();
    return -1;
  }
  unlockMmapLock();
  return 0;
}

page *DB::getPagePtr(pgid pgid) {
  return reinterpret_cast<page *>(&data_[pgid * pageSize_]);
}

bool DB::mmapDbFile(int targetSize) {
  void *ptr = ::mmap(nullptr, targetSize, PROT_READ, MAP_SHARED, file_, 0);
  if (ptr == MAP_FAILED) {
    return false;
  }
  ::madvise(ptr, targetSize, MADV_RANDOM);
  data_ = reinterpret_cast<decltype(data_) >(ptr);
  dataref_ = ptr;
  datasz_ = targetSize;
  return true;
}

bool DB::DbMunmap() {
  if (!dataref_) {
    return true;
  }
  int ret = ::munmap(dataref_, datasz_);
  if (ret == -1) {
    cout << "munmap failed!" << endl;
    return false;
  }
  data_ = nullptr;
  dataref_ = 0;
  datasz_ = 0;
  return true;
}

// mmapSize determines the appropriate size for the mmap given the current size
// of the database. The minimum size is 32KB and doubles until it reaches 1GB.
// Returns an error if the new mmap size is greater than the max allowed.
int DB::getMmapSize(uint32_t &targetSize) {
  // from 32k up to 1G
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
    page *p = pageInBuffer(buf, sizeof(buf), i);
    p->id = i;
    p->flag = pageFlags::metaPageFlag;
    meta *m = p->metaPtr();
    m->magic_ = MAGIC;
    m->version_ = VERSION;
    m->pageSize_ = pageSize_;
    m->freeListPageNumber_ = 2;
    m->root_ = bucket{ 3, 0 };
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
    cout << "init db failed!" << endl;
    return -1;
  }
  if (!fileSync()) {
    cout << "sync db file failed!" << endl;
  }
  filesz_ = size;
  return 0;
}

page *DB::pageInBuffer(char *ptr, size_t length, pgid pageId) {
  assert(length > pageId * pageSize_);
  return reinterpret_cast<page *>(ptr + pageId * pageSize_);
}

page *DB::allocate(uint32_t numPages, Tx *tx) {
  uint32_t len = numPages * pageSize_;
  assert(numPages < 0x1000);
  cout << "allocating len: " << len  << endl;
  // 操作在内存，现在不持久化
  auto ptr = reinterpret_cast<page*>(new char[len]); // TODO(roland): mempool
  ptr->overflow = numPages - 1;
  pgid pg = freeList_->allocate(numPages);
  if (pg != 0) {
    ptr->id = pg;
    return ptr;
  }
  cout << "not free page, try to mmap new page!" << endl;
  // ptr->id = rwtx_->metaData->totalPageNumber;
  // TODO
  return nullptr;
}

int DB::update(std::function<int(Tx *tx)> fn) {
  Tx *tx = beginRWTx();
  if (tx == nullptr) {
    return -1;
  }
  tx->managed_ = true;
  int ret = fn(tx);
  tx->managed_ = false;
  if (ret != 0) {
    tx->rollback();
    return -1;
  }

  auto result = tx->commit();
  // TODO
  // closeTx(tx);
  return result;
}

int DB::view(std::function<int(Tx *tx)> fn) {
  return 0;
}

Tx *DB::beginRWTx() {
  if (readOnly_) {
    return nullptr;
  }

  // 本函数内不会解锁，只有在commit/rollback后才会接锁。保证同一时间只有一个RW transaction.
  rwLock_.lock();

  // MDL
  std::lock_guard<std::mutex> guard(metaLock_);

  if (!opened_) {
    rwLock_.unlock();
    return nullptr;
  }
  rwtx_ = new Tx();
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
    freeList_->release(minId - 1);  // 注意这个-1
  }
  return rwtx_;
}

Tx *DB::beginTx() {
  return nullptr;
}

bool DB::getMmapRLock() {
  return pthread_rwlock_rdlock(&mmapLock_) == 0;
}

bool DB::getMmapWLock() {
  return pthread_rwlock_wrlock(&mmapLock_) == 0;

}

bool DB::unlockMmapLock() {
  return pthread_rwlock_unlock(&mmapLock_) == 0;
}
