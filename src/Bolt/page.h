#ifndef PAGE_H_
#define PAGE_H_

#include "type.h"

struct Page { // 每个page的头
  pgid id;
  uint16_t flag;
  // 页面中存储的数据数量，仅在页面类型是freelist以及leaf的时候起作用。
  uint16_t count;
  // 当前页面如果还不够存放数据，就会有后续页面，这个字段表示后续页面的数量。
  uint32_t overflow;
  // 指向页表头数据结尾，也就是页面数据的起始位置。
  char ptr[0];
  meta *metaPtr() { return reinterpret_cast<meta *>(ptr); }
  leafPageElement *getLeafPageElement(uint64_t index) {
    auto list = reinterpret_cast<leafPageElement *>(&ptr);
    return static_cast<leafPageElement *>(&list[index]);
  }
  void *getLeafPageKeyElementPtr(uint64_t index) {
    auto list = reinterpret_cast<leafPageElement *>(&ptr);
    return static_cast<void *>(&list[index]);
  }

  branchPageElement *getBranchPageElement(uint64_t index) {
    auto list = reinterpret_cast<branchPageElement *>(&ptr);
    return static_cast<branchPageElement *>(&list[index]);
  }
} __attribute__((packed));

const uint32_t PAGEHEADERSIZE = offsetof(Page, ptr);
const uint32_t MINKEYSPERPAGE = 2; // minKeysPerPage
// const size_t BRANCHPAGEELEMENTSIZE = sizeof(branchPageElement);
// const size_t LEAFPAGEELEMENTSIZE = sizeof(leafPageElement);

#endif // PAGE_H_