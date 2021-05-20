#ifndef TYPE_H_
#define TYPE_H_

#include <stdint.h>
#include <string.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include <assert.h>
#include <glog/logging.h>
#include <cstddef>
#include <memory>

using namespace std;

class DB;
struct meta;
class Tx;
class Node;
typedef std::shared_ptr<Tx> TxPtr;
typedef std::shared_ptr<Node> NodePtr;

enum pageFlags { //替代 #define
  branchPageFlag = 0x01,
  leafPageFlag = 0x02,
  metaPageFlag = 0x04,
  freelistPageFlag = 0x10 };

typedef uint64_t pgid;
typedef uint64_t txid;

const int valueLeafFlag = 0x00;  // 该leaf中存储的的是Value结构
const int bucketLeafFlag = 0x01; // 该leaf中存储的的是其他bucket结构

// |META_PAGE|META_PAGE|FREELIST_PAGE|DATA_PAGE...|

// string wrapper

struct Item {
  Item() : data_(), length_(0) {}
  Item(string str) {
    data_ = str;
    length_ = str.size();
  }
  Item(const string &str) {
    data_ = str;
    length_ = str.size();
  }
  Item(int size) {
    data_ = std::string(size, '\0');
    length_ = size;
  }
  Item(const Item &item) {
    this->data_ = item.data_;
    this->length_ = item.length_;
  }
  Item &operator=(const Item &item) {
    this->data_ = item.data_;
    this->length_ = item.length_;
    return *this;
  }
  Item(const char *buf, uint32_t len) {
    data_ = std::string(buf, len);
    length_ = len;
  }
  char *c_str() { return const_cast<char *>(data_.data()); }
  bool operator==(const Item &item) { return this->data_ == item.data_; }
  bool operator!=(const Item &item) { return this->data_ != item.data_; }
  bool operator<(const Item &item) { return this->data_ < item.data_; }
  bool operator>(const Item &item) { return this->data_ > item.data_; }
  bool operator==(const Item &item) const { return this->data_ == item.data_; }
  bool operator!=(const Item &item) const { return this->data_ != item.data_; }
  bool operator<(const Item &item) const { return this->data_ < item.data_; }
  bool operator>(const Item &item) const { return this->data_ > item.data_; }
  void reset() {
    data_.clear();
    length_ = 0;
  }
  bool empty() { return length_ == 0; }
  std::string data_;
  uint32_t length_ = 0;
};

namespace std {
template <> struct hash<Item> {
  std::size_t operator()(Item const &item) const noexcept {
    return std::hash<string> {}
    (item.data_);
  }
};
}

struct leafPageElement {
  uint32_t flag = 0; // is this element a bucket? yes:1 (bucketLeafFlag) no:0
  uint32_t pos = 0;
  uint32_t ksize = 0;
  uint32_t vsize = 0;
  Item read(uint32_t p, uint32_t s) const {
    const auto *ptr = reinterpret_cast<const char *>(this);
    //    return std::string(&ptr[p], &ptr[p + s]);
    return { &ptr[p], s };
  }
  Item key() const {
    return read(pos, ksize);
  } // 从当前位置算偏移，得到key和value值
  Item value() const { return read(pos + ksize, vsize); }
} __attribute__((packed));

struct branchPageElement {
  uint32_t pos = 0;
  uint32_t ksize = 0;
  pgid pageId = 0;

  Item key() const {
    auto ptr = reinterpret_cast<const char *>(this);
    return { &ptr[pos], ksize };
  }
} __attribute__((packed));

struct batch {
  DB *db;
};

#endif // TYPE_H_