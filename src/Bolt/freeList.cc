#include "freeList.h"
#include "page.h"
#include <vector>
#include <algorithm>
#include <iostream>

template <typename T> void print(const T &a) {
  std::stringstream ss;
  typename T::const_iterator iter = a.begin();
  for (; iter != a.end(); iter++) {
    ss << *iter << ", ";
  }
  LOG(INFO) << ss.str();
}

freeList::freeList() : ids_(), pending_(), cache_() {}

// |page haeder|pgid|pgid|...|
void freeList::read(Page *page) {
  size_t index = 0;
  size_t count = page->count;
  // If the page.count is at the max uint16 value (64k) then it's considered
  // an overflow and the size of the freelist is stored as the first element.
  if (count == 0xffff) {
    index = 1;
    count = *reinterpret_cast<pgid *>(page->ptr);
  }

  if (count == 0) {
    ids_.clear();
  } else {
    ids_.clear();
    pgid *ptr = reinterpret_cast<pgid *>(&page->ptr) + index;
    for (size_t i = 0; i < count; i++) {
      ids_.push_back(*ptr);
      ptr++;
    }
    std::sort(ids_.begin(), ids_.end());
  }
  reindex();
}

// 溢出之后需要重新分配page
void freeList::write(Page *page) {
  page->flag |= pageFlags::freelistPageFlag;
  auto cnt = count();
  if (cnt == 0) {
    // 不需要下刷磁盘
    page->count = 0;
  } else if (cnt < 0xffff) {
    page->count = cnt;
    vector<pgid> dest;
    // 写mmap，会报错吗？
    copyall(&dest);
    // 将所有free/pending都持久化进freeList page
    // 如果下刷完成之后crash，所有的pending都会转化为free
    auto ptr = reinterpret_cast<pgid *>(&page->ptr);
    for (auto &item : dest) {
      *ptr = item;
      ptr++;
    }
  } else {
    // 使用额外的页
    page->count = 0xffff;
    auto ptr = reinterpret_cast<pgid *>(&page->ptr);
    *ptr = cnt;
    ++ptr;
    std::vector<pgid> dest;
    copyall(&dest);
    // TODO(roland):溢出管理
    for (pgid it : dest) {
      *ptr = it;
      ptr++;
    }
  }
}

void freeList::reindex() {
  cache_.clear();
  for (auto &e : ids_) {
    cache_[e] = true;
  }
  for (auto &item : pending_) {
    for (auto pgid : item.second) {
      cache_[pgid] = true;
    }
  }
}

void freeList::copyall(std::vector<pgid> *dest) {
  vector<pgid> tmp;
  for (auto &it : pending_) {
    for (auto pg : it.second) {
      tmp.push_back(pg);
    }
  }
  std::sort(tmp.begin(), tmp.end());
  mergeids_(dest, tmp);
}

// merge src and ids_ into dest;
void freeList::mergeids_(vector<pgid> *dest, const vector<pgid> &src) {
  auto it1 = src.begin();
  auto it2 = ids_.begin();
  while (it1 != src.end() && it2 != ids_.end()) {
    if (*it1 < *it2) {
      dest->push_back(*it1++);
    } else {
      dest->push_back(*it2++);
    }
  }
  if (it1 != src.end()) {
    dest->insert(dest->end(), it1, src.end());
  }
  if (it2 != ids_.end()) {
    dest->insert(dest->end(), it2, ids_.end());
  }
  LOG(INFO) << "pending: ";
  print(src);
  LOG(INFO) << "ids_: ";
  print(ids_);
  LOG(INFO) << "dest: ";
  print(*dest);
}

pgid freeList::allocate(uint32_t numPages) {
  if (ids_.empty()) {
    return 0;
  }
  pgid init = 0;
  pgid prev = 0;

  for (uint64_t i = 0; i < ids_.size(); i++) {
    pgid id = ids_[i];

    if (id < 2) {
      LOG(FATAL) << "Id = " << id;
      assert(false);
    }

    if (prev == 0 || id - prev != 1) {
      // 数组开头或者不连续的话就更新init
      init = id;
    }

    if (id - init + 1 == numPages) {
      LOG(INFO) << "before erase: ";
      print(ids_);
      ids_.erase(ids_.begin() + i - numPages + 1, ids_.begin() + i + 1);
      LOG(INFO) << "after erase: ";
      print(ids_);

      for (size_t j = 0; j < numPages; j++) {
        cache_.erase(init + j);
      }
      return init;
    }

    prev = id;
  }
  return 0;
}

uint32_t freeList::size() const {
  auto ret = count();
  if (ret >= 0xffff) {
    ret++;
  }

  return PAGEHEADERSIZE + ret * sizeof(pgid);
}

uint32_t freeList::count() const { return freeCount() + pendingCount(); }

uint32_t freeList::freeCount() const { return ids_.size(); }

uint32_t freeList::pendingCount() const {
  uint32_t result = 0;
  for (auto &item : pending_) {
    // TODO(roland):no overlapping?
    result += item.second.size();
  }
  return result;
}

// release 从零到given txid区间内的所有transaction
void freeList::release(txid txid) {
  vector<pgid> tmp;
  auto it = pending_.begin();
  while (it != pending_.end()) {
    if (it->first <= txid) {
      tmp.insert(tmp.end(), it->second.begin(), it->second.end());
      it = pending_.erase(it);
    } else {
      ++it;
    }
  }
  vector<pgid> tmp1;
  std::sort(tmp.begin(), tmp.end());
  mergeids_(&tmp1, tmp);
  ids_ = tmp1;
}

void freeList::rollback(txid txid) {
  for (auto it : pending_[txid]) {
    cache_.erase(it);
  }
  pending_.erase(txid);
}

// 将free的page放到pending中
void freeList::free(txid txid, Page *p) {
  if (p->id <= 1) {
    LOG(INFO) << "ERROR! cannot free page " << p->id;
  }
  auto &ids = pending_[txid];
  for (auto id = p->id; id <= p->id + p->overflow; ++id) {
    // 当前页已经被free了。
    if (cache_[id]) {
      LOG(INFO) << "ERROR! page " << p->id << " already been freed!";
      assert(false);
    }
    ids.push_back(id);
  }
  // pending_[txid] = ids;
}

void freeList::reset() {
  pending_.clear();
  ids_.clear();
  cache_.clear();
}

void freeList::reload(Page *pg) {
  read(pg);

  // Build a cache of only pending pages.
  unordered_map<pgid, bool> curPending;
  for (auto item : pending_) {
    for (auto inner : item.second) {
      curPending[inner] = true;
    }
  }

  // Check each page in the freelist and build a new available freelist
  // with any pages not in the pending lists.
  std::vector<pgid> newIds;
  for (auto item : ids_) {
    if (curPending.find(item) == curPending.end()) {
      newIds.push_back(item);
    }
  }

  ids_ = newIds;
  reindex();
}