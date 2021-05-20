#include "meta.h"
#include "page.h"
#include <cstring>

meta *meta::clone() {
  meta *ptr = new meta(*this);
  return ptr;
}

void meta::write(Page *page) {
  if (root_.root >= totalPageNumber_) {
    assert(false);
  }
  if (freeListPageNumber_ >= totalPageNumber_) {
    assert(false);
  }

  page->id = txid_ % 2; // 两个pageID分开存放
  page->flag |= pageFlags::metaPageFlag;

  checksum_ = 0;

  memmove(page->metaPtr(), this, sizeof(meta));
}