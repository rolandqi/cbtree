#ifndef CURSOR_H_
#define CURSOR_H_

#include "type.h"
#include <stack>

class Page;
class Bucket;

struct ElementRef {
  ElementRef() : page_(nullptr), node_(nullptr) {}
  ElementRef(Page *page_p, NodePtr node_p) : page_(page_p), node_(node_p) {}
  Page *page_;
  NodePtr node_;
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
  // search a given key and seek its address
  void search(const Item &key, pgid pgid);
  // search leaf node (which is on the top of the stack) for a Key
  void searchLeaf(const Item &key);
  void searchBranchNode(const Item &key, NodePtr node);
  void searchBranchPage(const Item &key, Page *page);
  void *keyValue(Item &key, Item &value, uint32_t &flag);
  void clearElements();
  // weird function signature
  // return kv of the search Key if searchkey exists
  // or return the next Key
  void *do_seek(Item searchKey, Item &key, Item &value, uint32_t &flag);
  // Seek moves the cursor to a given key and returns it.
  // If the key does not exist then the next key is used. If no keys
  // follow, a nil key is returned.
  // The returned key and value are only valid for the life of the transaction.
  void *seek(const Item &searchKey, Item &key, Item &value, uint32_t &flag);

  // return the node the cursor is currently on
  NodePtr getNode();

  void do_next(Item &key, Item &value, uint32_t &flag);

  void do_first();
  void do_last();
  int remove();
  void prev(Item &key, Item &value);
  void next(Item &key, Item &value);
  void last(Item &key, Item &value);
  void first(Item &key, Item &value);
  uint32_t binarySearchLeaf(leafPageElement *arr, const Item &key, int count,
                            bool &found);
  uint32_t binarySearchBranch(branchPageElement *arr, const Item &key,
                              int count, bool &found);

private:
  Bucket *bucket_;
  std::stack<ElementRef> elements_;
};

#endif // CURSOR_H_
