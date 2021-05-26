#ifndef NODE_H_
#define NODE_H_

#include "type.h"
#include <unordered_map>
#include <algorithm>
#include <string>
#include <vector>
#include <memory>

struct bucketHeader;
class Bucket;

typedef std::vector<NodePtr> NodeList;

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
};

typedef std::vector<Inode> InodeList;

class Page;
class Bucket;
// this is a in-memory deserialized page
// 一个页抽象出来的数据结构
class Node : public std::enable_shared_from_this<Node> {
  friend class ElementRef;

public:
  explicit Node(Bucket *b, NodePtr parentNode);
  Node(const Node &) = delete;
  Node &operator=(const Node &) = delete;
  /**
   * setter
   */
  void markLeaf() { isLeaf_ = true; }
  void setBucket(Bucket *b) { bucket_ = b; }
  void setParentNode(NodePtr p) { parentNode_ = p; }
  void addChild(NodePtr c) { children_.push_back(c); }

  /**
   * getter
   */
  pgid getPageId() const { return pageId_; }
  Inode getInode(size_t idx) { return inodeList_[idx]; }
  bool isLeafNode() const { return isLeaf_; }
  std::vector<pgid> branchPageIds();

  uint32_t search(const Item &key, bool &found);
  bool isinlineable(uint32_t maxInlineBucketSize) const;

  //   void do_remove(const Item &key);
  // return size of deserialized Node
  uint32_t size() const;
  uint32_t pageElementSize() const;
  NodePtr root();     // 返回最顶层的node
  uint32_t minKeys(); // returns the minimum number of inodes this node
                      // should have.
  bool sizeLessThan(uint32_t s) const;
  uint32_t childIndex(NodePtr child);
  //   size_t numChildren() const;
  NodePtr nextSibling();
  NodePtr prevSibling();
  bool put(const Item &oldKey, const Item &newKey, const Item &value,
           pgid pageId, uint32_t flag);
  bool del(const Item &key);
  void read(Page *page);
  void write(Page *page);
  NodePtr childAt(int index); // 返回一个children node
  void free();
  void removeChild(NodePtr target);
  void dereference();
  bool spill();
  NodeList split(uint32_t pageSize);
  std::pair<NodePtr, NodePtr> splitTwo(uint32_t pageSize);
  uint32_t splitIndex(uint32_t threshold); // return the size of the first page.
  void rebalance();
  uint32_t numChildren() { return inodeList_.size(); }
  uint32_t binarySearch(const InodeList &target, const Item &key, bool &found);
  void do_remove(const Item &key);

private:
  Bucket *bucket_;
  bool isLeaf_;
  bool unbalanced_;
  bool spilled_;
  Item key_;
  pgid pageId_;
  NodePtr parentNode_;
  NodeList children_; // 新增的子node才会放到children_里，后面去spill
  InodeList
  inodeList_; // 数据节点，里面存放的数据是调用方提供的，存放一份数据的拷贝（TODO(roland):免拷优化）
};

#endif // NODE_H_
