#include "node.h"

Node::Node(Bucket *b, Node *p) : bucket_(b), isLeaf_(false), unbalanced_(false), spilled_(false), key_(), pageId_(0), parentNode_(p), children_(), inodeList_() {}

void Node::dereference() {
    //<del>
  // node lives in heap
  // nothing to be done here
  //</del>
  // 2 kinds of nodes lives in inodeslist
  // 1.value pointers to mmap address
  // 2.value pointers to heap/memory pool allocated object
  // when remapping is needed, the first kind needs to be saved to somewhere.
  // or it will pointing to undefined values after a new mmap
  // the second case will not need to be saved
  // may provide a method in memorypool to distinguish with pointer should be
  // saved for now, just copy every value to memory pool duplicate values exist.
  // they will be freed when memorypool clears itself

  // clone current node's key



  // if (!key.empty()) {
  //   key = key.clone(&bucket->getPool());
  // }

  // // clone current node's kv pairs
  // for (auto &item : inodeList) {
  //   item.key = item.key.clone(&bucket->getPool());
  //   item.value = item.value.clone(&bucket->getPool());
  // }

  // // do copy recursively
  // for (auto &child : children) {
  //   child->dereference();
  // }
}

Node *Node::root() {
  if (parentNode_ == nullptr) {
    return this;
  }
  return parentNode_->root();
}
