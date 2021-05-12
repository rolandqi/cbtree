#ifndef NODE_H_
#define NODE_H_

#include "type.h"
#include <unordered_map>
#include <algorithm>
#include <string>
#include <vector>

class Node;
struct bucket;
class Bucket;

typedef std::vector<Node *> NodeList;

// this is a pointer to element. The element can be in a page or not added to a
// page yet. 1.points to an element in a page 2.points to an element not yet in
// a page this can be pointing to kv pair. in this case, pageId is meaningless.
// if the inode is comprised in a branch Node, then pageId is the page starts
// with key value equals to 'key' member and the value is meaningless. may use
// an union to wrap up pageId and value
struct Inode {
  uint32_t flag = 0; //==bucketleaf if this is an inline bucket. otherwise == 0
  pgid pageId = 0;
  Item key;
  Item value;
  Item Key() const { return key; }
  Item Value() const { return value; }
};

typedef std::vector<Inode> InodeList;

class page;
class Bucket;
// this is a in-memory deserialized page
// 一个页抽象出来的数据结构
class Node {
  friend class ElementRef;

  Bucket *bucket_;
  bool isLeaf_;
  bool unbalanced_;
  bool spilled_;
  Item key_;
  pgid pageId_;
  Node *parentNode_;
  NodeList children_;   // 子节点的指针
  InodeList inodeList_; // 数据节点

public:
  explicit Node(Bucket *b, Node *p);
  //   /**
  //    * setter
  //    */
  //   void markLeaf() { isLeaf = true; }
  //   void setBucket(Bucket *b) { bucket = b; }
  //   void setParent(Node *p) { parentNode = p; }
  //   void addChild(Node *c) { children.push_back(c); }

  //   /**
  //    * getter
  //    */
  //   pgid getPageId() const { return pageId; }
  //   Inode getInode(size_t idx) { return inodeList[idx]; }
  //   bool isLeafNode() const { return isLeaf; }
  //   std::vector<pgid> branchPageIds();

  //   size_t search(const Item &key, bool &found);
  //   bool isinlineable(size_t maxInlineBucketSize) const;

  //   void read(page *page);
  //   Node *childAt(uint64_t index);
  //   void do_remove(const Item &key);
  //   // return size of deserialized Node
  //   size_t size() const;
  //   size_t pageElementSize() const;
  //   Node *root();
  //   size_t minKeys() const;
  //   bool sizeLessThan(size_t s) const;
  //   size_t childIndex(Node *child) const;
  //   size_t numChildren() const;
  //   Node *nextSibling();
  //   Node *prevSibling();
  //   void put(const Item &oldKey, const Item &newKey, const Item &value,
  //            pgid pageId, uint32_t flag);
  //   void del(const Item &key);
  //   void write(page *page);
  //   std::vector<Node *> split(size_t pageSize);
  //   void splitTwo(size_t pageSize, Node *&a, Node *&b);
  //   size_t splitIndex(size_t threshold);  // sz is return value. it's the
  // size of the first page.
  //   void free();
  //   void removeChild(Node *target);
  void dereference();
  Node *root();
  //   int spill();
  //   void rebalance();
};

#endif // NODE_H_
