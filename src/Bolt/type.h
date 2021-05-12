#ifndef TYPE_H_
#define TYPE_H_

#include <stdint.h>
#include <string.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include <assert.h>

using namespace std;

class DB;
class meta;

enum pageFlags { //替代 #define
  branchPageFlag = 0x01,
  leafPageFlag = 0x02,
  metaPageFlag = 0x04,
  freelistPageFlag = 0x10 };

typedef uint64_t pgid;
typedef uint64_t txid;

const int bucketLeafFlag = 0x01; // 该bucket中存储的的是其他bucket结构

// |META_PAGE|META_PAGE|FREELIST_PAGE|DATA_PAGE...|

struct Item {
  const char *pointer = nullptr;
  size_t length = 0;
  Item() = default;
  Item(const char *p, size_t sz) : pointer(p), length(sz) {}
  bool operator==(const Item &other) const;
  bool operator!=(const Item &other) const;
  bool operator<(const Item &other) const;

  void reset();
  bool empty() const;
  //   Item clone(MemoryPool *pool);
  static Item make_item(const char *p) {
    if (p == nullptr || *p == 0) {
      return {};
    }
    return { p, strlen(p) };
  }
};

namespace std {
template <> struct hash<Item> {
  std::size_t operator()(Item const &item) const noexcept {
    return hash<size_t>()(reinterpret_cast<size_t>(item.pointer)) ^
           hash<size_t>()(item.length);
  }
};
}

struct batch {
  DB *db;
};

struct page { // 每个page的头
  pgid id;
  uint16_t flag;
  uint16_t
  count; // 页面中存储的数据数量，仅在页面类型是freelist以及leaf的时候起作用。
  uint32_t
  overflow; // 当前页面如果还不够存放数据，就会有后续页面，这个字段表示后续页面的数量。
  char ptr[0]; // 指向页表头数据结尾，也就是页面数据的起始位置。
  meta *metaPtr() { return reinterpret_cast<meta *>(ptr); }
} __attribute__((packed));

const size_t PAGEHEADERSIZE = offsetof(page, ptr);
const size_t MINKEYSPERPAGE = 2;
// const size_t BRANCHPAGEELEMENTSIZE = sizeof(BranchPageElement);
// const size_t LEAFPAGEELEMENTSIZE = sizeof(LeafPageElement);

#endif // TYPE_H_