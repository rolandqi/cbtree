#ifndef CURSOR_H_
#define CURSOR_H_

#include "type.h"
#include <deque>

class page;
class Node;
class Bucket;

struct ElementRef {
  ElementRef() : page_(nullptr), node_(nullptr) {}
  ElementRef(page *page_p, Node *node_p) : page_(page_p), node_(node_p) {}
  page *page_;
  Node *node_;
  uint64_t index_ = 0; // DO NOT change this default ctor build up a ref to the
  // first element in a page is this a leaf page/Node
  bool isLeaf() const;
  // return the number of inodes or page elements
  size_t count() const;
};

// 通过Cursor遍历整个bucket中的所有kv pair

class Cursor {
public:
  explicit Cursor(Bucket *bucket) : bucket_(bucket), elements_() {}
  Bucket *getBucket() { return bucket_; }
  void search(const Item &key, pgid pgid);

private:
  Bucket *bucket_;
  std::deque<ElementRef> elements_;
};

#endif // CURSOR_H_
