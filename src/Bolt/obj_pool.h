#ifndef OBJ_POOL_H_
#define OBJ_POOL_H_

#include <unistd.h>
#include <iostream>
#include <stdint.h>
#include <vector>
#include <utility>
#include <assert.h>

template <class T> class ObjPool {
  using value_type = T;

public:
  explicit ObjPool(int obj_cnt, size_t obj_size = sizeof(value_type),
                   size_t alignment = 8)
      : obj_cnt_(obj_cnt), bitmap_(obj_cnt, 0), obj_size_(obj_size) {
    account_malloc_cnt_ = (uint64_t *)malloc(obj_cnt_ * sizeof(uint64_t));
    account_free_cnt_ = (uint64_t *)malloc(obj_cnt_ * sizeof(uint64_t));
    assert(account_malloc_cnt_);
    assert(account_free_cnt_);
    memset(account_malloc_cnt_, 0, obj_cnt_ * sizeof(uint64_t));
    memset(account_free_cnt_, 0, obj_cnt_ * sizeof(uint64_t));

    real_size_ = ((obj_size_ * obj_cnt_ - 1) / alignment + 1) * alignment;
    int ret = posix_memalign((void **)&mem_ptr_, alignment, real_size_);
    if (ret != 0) {
      LOG(INFO) << "Failed to malloc ret: " << ret << ", size=" << obj_size_ << "*"
           << obj_cnt_;
      mem_ptr_ = nullptr;
      return;
    }
    free_list_ = (value_type **)malloc(sizeof(value_type *) * obj_cnt_);
    free_list_size_ = 0;
    for (int i = 0; i < obj_cnt_; ++i) {
      free_list_[free_list_size_++] =
          reinterpret_cast<value_type *>(mem_ptr_ + i * obj_size_);
    }
  }
  ~ObjPool() {
    if (account_malloc_cnt_) {
      free(account_malloc_cnt_);
    }
    if (account_free_cnt_) {
      free(account_free_cnt_);
    }

    free(free_list_);
    free(mem_ptr_);
  }

public:
  template <typename... _Args> value_type *Malloc(_Args &&... __args) {
    if (free_list_size_ == 0) {
      ++malloc_failed_cnt_;
      return nullptr;
    }
    auto ele = free_list_[--free_list_size_];
    new (ele) value_type(std::forward<_Args>(__args)...);
    assert(MallocCheckAndSet(ele));
    return ele;
  }
  void Free(value_type *p) {
    assert(FreeCheckAndSet(p));
    p->~value_type();
    free_list_[free_list_size_++] = p;
  }

  int free_list_size() { return free_list_size_; }

  // 即将废弃
  int chunk_cnt() { return obj_cnt_; }
  int obj_cnt() { return obj_cnt_; }

  uint64_t malloc_succeed_cnt() { return malloc_succeed_cnt_; }
  uint64_t malloc_failed_cnt() { return malloc_failed_cnt_; }
  uint64_t free_cnt() { return free_cnt_; }
  int32_t lowest_free_cnt() { return lowest_free_cnt_; }
  void *pool_addr() const { return mem_ptr_; }
  size_t pool_size() const { return real_size_; }

private:
  int GetIndex(void *p) {
    auto ptr_int = reinterpret_cast<uintptr_t>(p);
    auto ptr_start_int = reinterpret_cast<uintptr_t>(mem_ptr_);
    uintptr_t offset = ptr_int - ptr_start_int;
    if (offset >= (obj_size_ * obj_cnt_)) {
      return -1;
    }

    if (offset % obj_size_ != 0) {
      return -1;
    }
    return offset / obj_size_;
  }

  bool MallocCheckAndSet(void *p) {
    ++malloc_succeed_cnt_;
    if (free_list_size_ < lowest_free_cnt_) {
      lowest_free_cnt_ = free_list_size_;
    }
    auto idx = GetIndex(p);
    if (idx < 0) {
      return false;
    }
    if (bitmap_[idx]) {
      return false;
    }
    bitmap_[idx] = 1;
    account_malloc_cnt_[idx]++;
    return true;
  }

  bool FreeCheckAndSet(void *p) {
    ++free_cnt_;
    auto idx = GetIndex(p);
    if (idx < 0) {
      return false;
    }
    if (!bitmap_[idx]) {
      sleep(3);
      return false;
    }
    bitmap_[idx] = 0;
    account_free_cnt_[idx]++;
    return true;
  }

private:
  value_type **free_list_;
  int free_list_size_ = 0;

private:
  uint8_t *mem_ptr_;
  int obj_cnt_;
  size_t real_size_;
  uint64_t malloc_succeed_cnt_ = 0;
  uint64_t malloc_failed_cnt_ = 0;
  uint64_t free_cnt_ = 0;
  int32_t lowest_free_cnt_ = INT32_MAX;
  std::vector<uint8_t> bitmap_;
  uint64_t *account_malloc_cnt_ = nullptr;
  uint64_t *account_free_cnt_ = nullptr;
  size_t obj_size_;
};
#endif // OBJ_POOL_H_
