#include "cursor.h"
#include "node.h"
#include "page.h"
#include "bucket.h"
#include <functional>
#include <algorithm>
#include "tx.h"

bool ElementRef::isLeaf() const {
  if (node_ != nullptr) {
    return node_->isLeaf_;
  }
  assert(page_);
  return (page_->flag & pageFlags::leafPageFlag) != 0;
}

size_t ElementRef::count() const {
  if (node_ != nullptr) {
    return node_->inodeList_.size();
  }
  assert(page_);
  return page_->count;
}

void Cursor::search(const Item &key, pgid pgid) {
  // 都是从0开始搜索的
  // LOG(INFO) << "cursor searching pageid: " << pgid;
  NodePtr node = nullptr;
  Page *page = nullptr;
  bucket_->getPageNode(pgid, node, page);
  if (page && ((page->flag &
                (pageFlags::branchPageFlag | pageFlags::leafPageFlag)) == 0)) {
    // 既不是branch，又不是leaf
    LOG(FATAL) << " paga flag: " << page->flag << " unidentifiable!";
  }
  ElementRef ref{ page, node };
  elements_.push(ref);
  if (ref.isLeaf()) {
    searchLeaf(key);
    return;
  }

  if (node) {
    searchBranchNode(key, node);
    return;
  }
  searchBranchPage(key, page);
}

void Cursor::searchLeaf(const Item &key) {
  assert(!elements_.empty());
  ElementRef &ref = elements_.top();

  bool found = false;
  if (ref.node_) {
    // search through inodeList_ for a matching Key
    // inodelist should be sorted in ascending order
    ref.index_ = static_cast<uint32_t>(ref.node_->search(key, found));
    return;
  }

  // 在page中，但是没构造成node结构
  auto ptr = ref.page_->getLeafPageElement(0);
  ref.index_ = static_cast<uint32_t>(
      binarySearchLeaf(ptr, key, ref.page_->count, found));
  LOG(INFO) << "cursor search leaf: " << (found ? "found!" : "not found.");
}

void Cursor::searchBranchNode(const Item &key, NodePtr node) {
  bool found = false;
  auto index = node->search(key, found);
  if (!found && index > 0) {
    index--;
  }
  assert(!elements_.empty());
  elements_.top().index_ = index;
  // 从branch往下搜索
  search(key, node->getInode(index).pageId);
}

void Cursor::searchBranchPage(const Item &key, Page *page) {
  auto branchElements = page->getBranchPageElement(0);
  bool found = false;
  auto index = binarySearchBranch(branchElements, key, page->count, found);
  if (!found && index > 0) {
    index--;
  }
  assert(!elements_.empty());
  elements_.top().index_ = index;
  search(key, branchElements[index].pageId);
}

void *Cursor::do_seek(Item searchKey, Item &key, Item &value, uint32_t &flag) {
  clearElements();
  search(searchKey, bucket_->getRootPage());

  auto &ref = elements_.top();
  if (ref.index_ >= ref.count()) {
    key.reset();
    value.reset();
    flag = 0;
    return nullptr;
  }
  return keyValue(key, value, flag);
}

void *Cursor::seek(const Item &searchKey, Item &key, Item &value,
                   uint32_t &flag) {
  key.reset();
  value.reset();
  flag = 0;
  return do_seek(searchKey, key, value, flag);
}

//  returns the leaf node that the cursor is currently positioned on.
NodePtr Cursor::getNode() {
  if (!elements_.empty() && elements_.top().node_ && elements_.top().isLeaf()) {
    return elements_.top().node_;
  }

  std::vector<ElementRef> v;
  while (!elements_.empty()) {
    v.push_back(elements_.top());
    elements_.pop();
  }
  reverse(v.begin(), v.end());
  for (int i = v.size() - 1; i >= 0; i--) {
    elements_.push(v[i]);
  }

  NodePtr node = v[0].node_;
  if (node == nullptr) {
    node = bucket_->getNode(v[0].page_->id, nullptr);
  }

  // the last one should be a leaf node
  // transverse every branch node
  for (size_t i = 0; i + 1 < v.size(); i++) {
    assert(!node->isLeafNode());
    node = node->childAt(v[i].index_);
  }

  if (!node->isLeafNode()) {
    LOG(ERROR) << "got invalid branch node!"; // expect leaf
  }
  assert(node->isLeafNode());
  return node;
}

// 从seek stack的最上层读出k-v
void *Cursor::keyValue(Item &key, Item &value, uint32_t &flag) {
  if (elements_.empty()) {
    key.reset();
    value.reset();
    flag = 0;
    return nullptr;
  }

  auto ref = elements_.top();
  if (ref.count() == 0 || ref.index_ >= ref.count()) {
    LOG(ERROR) << "get Key/value from empty bucket_ / index out of range";
    return nullptr;
  }

  // are those values sitting a node?
  if (ref.node_) {
    auto inode = ref.node_->getInode(ref.index_);
    key = inode.key;
    value = inode.value;
    flag = inode.flag;
    return nullptr;
  }

  // let's get them from page
  auto ret = ref.page_->getLeafPageElement(ref.index_);
  key = ret->key();
  value = ret->value();
  flag = ret->flag;
  return ref.page_->getLeafPageKeyElementPtr(ref.index_);
}

uint32_t Cursor::binarySearchLeaf(leafPageElement *arr, const Item &key,
                                  int count, bool &found) {
  found = false;
  uint32_t begin = 0;
  uint32_t end = count;
  uint32_t mid = (begin + end) / 2;
  while (begin < end) {
    mid = (begin + end) / 2;
    if (arr[mid].key() > key) {
      end = mid;
    } else if (arr[mid].key() == key) {
      found = true;
      return mid;
    } else {
      begin = mid + 1;
    }
  }
  // 如果没找到，返回ceiling
  // TODO(roland):unit test
  return end;
}

uint32_t Cursor::binarySearchBranch(branchPageElement *arr, const Item &key,
                                    int count, bool &found) {
  found = false;
  uint32_t begin = 0;
  uint32_t end = count;
  uint32_t mid = (begin + end) / 2;
  while (begin < end) {
    mid = (begin + end) / 2;
    if (arr[mid].key().data_ > key.data_) {
      end = mid;
    } else if (arr[mid].key().data_ == key.data_) {
      found = true;
      return mid;
    } else {
      begin = mid + 1;
    }
  }
  // 如果没找到，返回ceiling
  // TODO(roland):unit test
  return end;
}

void Cursor::prev(Item &key, Item &value) {
  key.reset();
  value.reset();
  while (!elements_.empty()) {
    auto &ref = elements_.top();
    if (ref.index_ > 0) {
      ref.index_--;
      break;
    }
    elements_.pop();
  }

  if (elements_.empty()) {
    return;
  }

  do_last();
  uint32_t flag = 0;
  keyValue(key, value, flag);
}

void Cursor::next(Item &key, Item &value) {
  key.reset();
  value.reset();
  uint32_t flag = 0;
  do_next(key, value, flag);
}

void Cursor::last(Item &key, Item &value) {
  key.reset();
  value.reset();
  clearElements();
  Page *page = nullptr;
  NodePtr node = nullptr;
  bucket_->getPageNode(bucket_->getRootPage(), node, page);
  ElementRef element{ page, node };
  element.index_ = element.count() - 1;
  elements_.push(element);
  do_last();
  uint32_t flag = 0;
  keyValue(key, value, flag);
}

void Cursor::first(Item &key, Item &value) {
  key.reset();
  value.reset();
  clearElements();
  Page *page = nullptr;
  NodePtr node = nullptr;
  bucket_->getPageNode(bucket_->getRootPage(), node, page);
  ElementRef element{ page, node };

  elements_.push(element);
  do_first();

  uint32_t flag = 0;
  if (elements_.top().count() == 0) {
    do_next(key, value, flag);
  }

  keyValue(key, value, flag);
}

void Cursor::do_next(Item &key, Item &value, uint32_t &flag) {
  while (true) {
    while (!elements_.empty()) {
      auto &ref = elements_.top();
      // not the last element
      if (ref.index_ + 1 < ref.count()) {
        ref.index_++;
        break;
      }
      elements_.pop();
    }

    if (elements_.empty()) {
      key.reset();
      value.reset();
      flag = 0;
      return;
    }

    do_first();
    // not sure what this intends to do
    if (elements_.top().count() == 0) {
      continue;
    }

    keyValue(key, value, flag);
    return;
  }
}

void Cursor::do_first() {
  while (true) {
    assert(!elements_.empty());
    if (elements_.top().isLeaf()) {
      break;
    }

    auto &ref = elements_.top();
    pgid pageId = 0;
    if (ref.node_ != nullptr) {
      pageId = ref.node_->getInode(ref.index_).pageId;
    } else {
      pageId = ref.page_->getBranchPageElement(ref.index_)->pageId;
    }

    Page *page = nullptr;
    NodePtr node = nullptr;
    bucket_->getPageNode(pageId, node, page);
    ElementRef element(page, node);
    elements_.push(element);
  }
}

void Cursor::do_last() {
  while (true) {
    assert(!elements_.empty());
    auto &ref = elements_.top();
    if (ref.isLeaf()) {
      break;
    }

    pgid pageId = 0;
    if (ref.node_ != nullptr) {
      pageId = ref.node_->getInode(ref.index_).pageId;
    } else {
      pageId = ref.page_->getBranchPageElement(ref.index_)->pageId;
    }

    Page *page = nullptr;
    NodePtr node = nullptr;
    bucket_->getPageNode(pageId, node, page);
    ElementRef element(page, node);
    element.index_ = element.count() - 1;
    elements_.push(element);
  }
}

int Cursor::remove() {
  if (bucket_->getTx()->db_ == nullptr) {
    LOG(ERROR) << "db closed";
    return -1;
  }

  if (!bucket_->isWritable()) {
    LOG(ERROR) << "tx not writable";
    return -1;
  }

  Item key;
  Item value;
  uint32_t flag;
  keyValue(key, value, flag);

  if (flag & bucketLeafFlag) {
    LOG(ERROR) << "current value is a bucket_| try removing a branch bucket_ "
                  "other than kv in leaf node";
    return -1;
  }

  getNode()->del(key);
  return 0;
}

void Cursor::clearElements() {
  while (!elements_.empty()) {
    elements_.pop();
  }
}
