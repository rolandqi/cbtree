#include "bucket.h"

Bucket::Bucket(Tx *tx_ptr) : tx_(tx_ptr) {}

Node *Bucket::getCachedNode(pgid pgid) {
  auto it = nodes_.find(pgid);
  if (it != nodes_.end()) {
    return it->second;
  }
  return nullptr;
}

void Bucket::dereference() {
  if (rootNode_) {
    rootNode_->root()->dereference();
  }

  for (auto it : buckets_) {
    it.second->dereference();
  }
}