#include "node.h"
#include "bucket.h"
#include "page.h"
#include "tx.h"
#include <algorithm>
#include <memory>

// template<class Iter, class T>
// Iter BinaryFind(Iter begin, Iter end, T val, bool *found)
// {
//     // Finds the lower bound in at most log(last - first) + 1 comparisons
//     Iter i = std::lower_bound(begin, end, val);

//     if (i != end && val == *i) {
//       found = true;
//       return i; // found
//     }
//     found = false;
//     return i; // not found
// }

// template<typename T>
// int BinarySearch(vector<T> array,T var)
// {
//     //array should be sorted in ascending order in this case
//     int start=0;
//     int end=array.size()-1;
//     while(start<=end){
//         int mid=(start+end)/2;
//         if(array[mid]==var){
//             return mid;
//         }
//         else if(var<array[mid]){
//             end=mid-1;
//         }
//         else{
//             start=mid+1;
//         }
//     }
//     return 0;
// }

uint32_t Node::binarySearch(const InodeList &arr, const Item &key,
                            bool &found) {
  found = false;
  uint32_t begin = 0;
  uint32_t end = arr.size();
  uint32_t mid = (begin + end) / 2;
  while (begin < end) {
    mid = (begin + end) / 2;
    if (arr[mid].key > key) {
      end = mid;
    } else if (arr[mid].key == key) {
      found = true;
      return mid;
    } else {
      begin = mid;
    }
  }
  // 如果没找到，返回ceiling
  // TODO(roland):unit test
  return end;
};

Node::Node(Bucket *b, NodePtr parentNode)
    : bucket_(b), isLeaf_(false), unbalanced_(false), spilled_(false), key_(),
      pageId_(0), parentNode_(parentNode), children_(), inodeList_() {}

// dereference causes the node to copy all its inode key/value references to
// heap memory.
// This is required when the mmap is reallocated so inodes are not pointing to
// stale data.
// 原版实现，如果mmap页变动，需要将所有mmap中的数据拷贝出来
// 但是目前我的实现由于只要放到内存里就是拷贝（使用string实现）
// 所以在dereference阶段不需要考虑太多事情
void Node::dereference() {
  // clone current node's key
  //...

  // clone current node's kv pairs
  // ..
  // do copy recursively
  // for (auto &child : children_) {
  //   child->dereference();
  // }
}

NodePtr Node::root() {
  if (parentNode_ == nullptr) {
    return shared_from_this();
  }
  return parentNode_->root();
}

uint32_t Node::minKeys() {
  if (isLeaf_) {
    return 1;
  }
  return 2;
}

uint32_t Node::size() const {
  int size = PAGEHEADERSIZE;
  for (uint32_t i = 0; i < inodeList_.size(); i++) {
    auto it = inodeList_[i];
    size += pageElementSize() + it.key.length_ + it.value.length_;
  }
  return size;
}

uint32_t Node::pageElementSize() const {
  if (isLeaf_) {
    return sizeof(leafPageElement);
  }
  return sizeof(branchPageElement);
}

NodePtr Node::childAt(int index) {
  if (isLeaf_) {
    LOG(ERROR) << "leaf node do not have children!";
    return nullptr;
  }
  return bucket_->getNode(
      inodeList_[index].pageId,
      shared_from_this()); // 从当前node所对应的bucket里面获取node。传入child
                           // node所对应的pgid
}

// 上层业务要保证调用put之后有序表（目前实现是vector）还是有序的。
bool Node::put(const Item &oldKey, const Item &newKey, const Item &value,
               pgid pageId, uint32_t flag) {
  if (pageId >= bucket_->getTotalPageNumber()) {
    LOG(ERROR) << "node put request a non-exist page.";
    return false;
  }
  if (oldKey.length_ <= 0 || newKey.length_ <= 0) {
    LOG(ERROR) << "key length_ error!";
    return false;
  }
  bool found = 0;
  auto index = binarySearch(inodeList_, oldKey, found);
  auto target_inode = inodeList_.begin();
  if (!found) {
    target_inode = inodeList_.insert(target_inode + index,
                                     Inode()); // TODO(roland):how's the
                                               // performance when changed to
                                               // std::map()?
  } else {
    target_inode += index;
  }
  target_inode->flag = flag;
  target_inode->key = newKey;
  target_inode->value = value;
  target_inode->pageId = pageId;
  return true;
}

bool Node::del(const Item &key) {
  bool found = false;
  auto it = binarySearch(inodeList_, key, found);

  if (!found) {
    return true;
  }
  inodeList_.erase(inodeList_.begin() + it);

  // need re-balance
  unbalanced_ = true;
  return true;
}

void Node::read(Page *page) {
  if (!page) {
    LOG(FATAL) << "node read receive nullptr.";
  }
  this->pageId_ = page->id;
  this->isLeaf_ = (page->flag == pageFlags::leafPageFlag);
  this->inodeList_.resize(page->count);

  for (uint16_t i = 0; i < page->count; i++) {
    auto &it = this->inodeList_[i];
    if (this->isLeaf_) {
      auto element = page->getLeafPageElement(i);
      it.flag = element->flag;
      it.key = element->key();
      it.value = element->value();
    } else {
      auto element = page->getBranchPageElement(i);
      it.pageId = element->pageId;
      it.key = element->key();
    }
    // assert(item.key.length_ != 0);
  }

  // Save first key so we can find the node in the parent when we spill.
  if (!inodeList_.empty()) {
    key_ = inodeList_.front().key;
    assert(!key_.empty());
  } else {
    key_.reset();
  }
}

// 将node中的元素序列化到page内存中
void Node::write(Page *page) {
  if (isLeaf_) {
    page->flag |= pageFlags::leafPageFlag;
  } else {
    page->flag |= pageFlags::branchPageFlag;
  }

  // why it exceed 0xffff ?
  if (inodeList_.size() > 0xffff) {
    LOG(FATAL) << "inode overflow " << inodeList_.size();
  }

  page->count = inodeList_.size();
  if (page->count == 0) {
    // TODO(roland): no items to write?
    return;
  }

  //|page header | leaf/branch element .... | kv pair ...  |
  //|<-page start| &page->ptr               |<-contentPtr  |<-page end
  auto contentPtr = &(reinterpret_cast<char *>(
                         &page->ptr)[inodeList_.size() * pageElementSize()]);
  for (uint32_t i = 0; i < inodeList_.size(); i++) {
    if (isLeaf_) {
      auto item = page->getLeafPageElement(i);
      item->pos = contentPtr - (char *)item;
      item->flag = inodeList_[i].flag;
      item->ksize = inodeList_[i].key.length_;
      item->vsize = inodeList_[i].value.length_;
    } else {
      auto item = page->getBranchPageElement(i);
      item->pos = contentPtr - (char *)&item;
      item->ksize = inodeList_[i].key.length_;
      item->pageId = inodeList_[i].pageId;
    }

    memcpy(contentPtr, inodeList_[i].key.c_str(), inodeList_[i].key.length_);
    contentPtr += inodeList_[i].key.length_;
    memcpy(contentPtr, inodeList_[i].value.c_str(),
           inodeList_[i].value.length_);
    contentPtr += inodeList_[i].value.length_;
  }
}

bool Node::spill() {
  if (spilled_) {
    return 0;
  }
  auto tx = bucket_->getTx();

  // by pointer value for now
  std::sort(children_.begin(), children_.end());
  // spill will modify children_'s size, no range loop here
  for (uint32_t i = 0; i < children_.size(); i++) {
    // spill recursively
    if (children_[i]->spill()) {
      return false;
    }
  }

  children_.clear();
  auto nodes = split(4096);
  // auto nodes = split(bucket_->getTx()->db_->getPageSize());

  for (auto &node : nodes) {
    assert(node);
    if (node->getPageId() > 0) {
      // 当前页之前落盘过，就从相关联的事务中free掉(最终是调用freeList中的free())
      // Add node's page to the freelist if it's not new.
      tx->free(tx->getTxId(), tx->getPage(node->getPageId()));
    }

    auto page = tx->allocate((size() / 4096) + 1);
    // auto page = tx->allocate((size() / bucket_->tx_->db_->getPageSize()) +
    // 1);
    if (page == nullptr) {
      return false;
    }

    if (page->id >= tx->getTotalPageNumber()) {
      assert(false);
    }
    node->pageId_ = page->id;
    // 分裂之后写内存
    node->write(page);
    node->spilled_ = true;

    // Insert into parent inodes.
    if (node->parentNode_) {
      auto k = node->key_;
      if (k.length_ == 0) {
        k = inodeList_.front().key;
      }
      Item emptyValue;
      // 修改之前存放在parentNode_的本页的key索引
      // 如果有，就修改它，如果没有就插入
      node->parentNode_->put(k, node->inodeList_.front().key, emptyValue,
                             node->pageId_, 0);
      node->key_ = node->inodeList_.front().key;
      assert(k.length_ > 0);
    }
    // tx->stats_.spillCount++;
  }

  // If the root node split and created a new root then we need to spill that
  // as well. We'll clear out the children to make sure it doesn't try to
  // respill.
  if (parentNode_ && parentNode_->pageId_ == 0) {
    children_.clear();
    return parentNode_->spill();
  }
  return 0;
}

NodeList Node::split(uint32_t pageSize) {
  NodeList result;
  auto cur = shared_from_this();
  while (true) {
    NodePtr a;
    NodePtr b;
    std::tie(a, b) = cur->splitTwo(pageSize);
    result.push_back(a);
    if (b == nullptr) {
      // 当前node一直分裂到无法分裂，保证所有的结果都放到result里面了，再break
      break;
    }
    cur = b;
  }
  return result;
}

std::pair<NodePtr, NodePtr> Node::splitTwo(uint32_t pageSize) {
  NodePtr a;
  NodePtr b;
  if (inodeList_.size() <= MINKEYSPERPAGE * 2 || sizeLessThan(pageSize)) {
    // 如果达不到分裂标准，就返回
    a = shared_from_this();
    b.reset();
    return { a, b };
  }

  // calculate threshold
  double fill = bucket_->getFillPercent();
  if (fill < MINFILLPERCENT) {
    fill = MINFILLPERCENT;
  }
  if (fill > MAXFILLPERCENT) {
    fill = MAXFILLPERCENT;
  }

  uint32_t threshold = pageSize * fill;

  // determinate split position
  // index must been > 0
  uint32_t index = splitIndex(threshold);

  if (parentNode_ == nullptr) {
    // 如果没有parentNode_，就创建它
    parentNode_ = make_shared<Node>(bucket_, nullptr);
    parentNode_->children_.push_back(shared_from_this());
  }

  auto newNode = make_shared<Node>(bucket_, parentNode_);
  newNode->isLeaf_ = isLeaf_;
  parentNode_->children_.push_back(newNode);

  for (uint32_t i = index; i < inodeList_.size(); i++) {
    newNode->inodeList_.push_back(inodeList_[i]);
  }
  inodeList_.erase(inodeList_.begin() + index, inodeList_.end());

  // 分裂的结果是自己多出来一个平行的节点，挂在parent下。
  a = shared_from_this();
  b = newNode;
  return { a, b };
}

bool Node::sizeLessThan(uint32_t s) const {
  uint32_t sz = PAGEHEADERSIZE;
  for (uint32_t i = 0; i < inodeList_.size(); i++) {
    sz += pageElementSize() + inodeList_[i].key.length_ +
          inodeList_[i].value.length_;
    if (sz >= s) {
      return false;
    }
  }
  return true;
}

void Node::free() {
  if (pageId_) {
    auto tx = bucket_->getTx();
    tx->free(tx->getTxId(), tx->getPage(pageId_));
    pageId_ = 0;
  }
}

uint32_t Node::splitIndex(uint32_t threshold) {
  uint32_t index = 0;
  uint32_t sz = PAGEHEADERSIZE;
  for (uint32_t i = 0; i < inodeList_.size() - MINKEYSPERPAGE; i++) {
    index = i;
    auto &ref = inodeList_[i];
    auto elementSize = pageElementSize() + ref.key.length_ + ref.value.length_;
    // If we have at least the minimum number of keys and adding another
    // node would put us over the threshold then exit and return.
    if (i >= MINKEYSPERPAGE && sz + elementSize > threshold) {
      break;
    }
    sz += elementSize;
  }
  return index;
}

void Node::rebalance() {
  if (!unbalanced_) {
    return;
  }
  unbalanced_ = false;
  // bucket_->getTx()->stats_.rebalanceCount++;

  uint32_t threshold = 4096 / 4;
  // auto threshold = bucket_->tx_->db_->getPageSize() / 4;
  // Ignore if node is above threshold (25%) and has enough keys.
  if (size() > threshold && inodeList_.size() > minKeys()) {
    return;
  }
  // 情况一、当前的parent node只有一个节点，将下层的节点提升
  if (parentNode_ == nullptr) {
    // 如果parentNode_太小了，需要和children合并
    // If root node is a branch and only has one node then collapse it.
    if (!isLeaf_ && inodeList_.size() == 1) {
      auto child = bucket_->getNode(inodeList_[0].pageId, shared_from_this());
      isLeaf_ = child->isLeaf_;
      inodeList_ = child->inodeList_;
      children_ = child->children_;

      // Reparent all child nodes being moved.
      for (auto &item : inodeList_) {
        NodePtr n = bucket_->getCachedNode(item.pageId);

        if (n) {
          n->parentNode_ = shared_from_this();
        } else {
          assert(false);
        }
      }
      // 自己成为之前的children了，之前的children可以free()了。
      child->parentNode_ = nullptr;
      bucket_->eraseCachedNode(child->pageId_);
      child->free();
      // child这个shared_ptr应该不被任何东西持有了，应该被释放了。
    }
    return;
  }

  // 情况二、当前node已经不存在任何inode了，需要移除
  // If node has no keys then just remove it.
  if (numChildren() == 0) {
    parentNode_->del(key_); // 上层node存放当前node的以一个key
    parentNode_->removeChild(shared_from_this());
    bucket_->eraseCachedNode(pageId_);
    free();
    parentNode_->rebalance();
    return;
  }

  assert(parentNode_->numChildren() > 1);

  // 情况三、本层的两个node合并，选择相邻的两个节点，将右边节点的内容移入左边
  // Destination node is right sibling if idx == 0, otherwise left sibling.
  if ((parentNode_->childIndex(shared_from_this())) == 0) {
    // 只有自己是这parentNode下层的第一个节点的时候，才使用右兄弟
    auto target = nextSibling();
    // 将右兄弟的inode移入本node，然后递归调整parentNode

    // this should move inodes of target into current node
    // and re set up between node's parent&child link

    // set sibling node's children_'s parent to current node
    // 将右兄弟的孩子节点的parentNode指向本node
    for (auto &item : target->inodeList_) {
      auto childNode = bucket_->getCachedNode(item.pageId);
      if (childNode) {
        childNode->parentNode_->removeChild(childNode);
        childNode->parentNode_ = shared_from_this();
        childNode->parentNode_->children_.push_back(childNode);
      }
    }

    // copy sibling node's children_ to current node
    // 拷贝右兄弟的inodeList到本node
    std::copy(target->inodeList_.begin(), target->inodeList_.end(),
              std::back_inserter(inodeList_));
    // remove sibling node
    // 删除右兄弟
    parentNode_->del(target->key_);
    parentNode_->removeChild(target);
    bucket_->eraseCachedNode(target->pageId_);
    target->free();
  } else {
    // 与左兄弟合并
    // 自己删除，自己的节点移入左兄弟
    auto target = prevSibling();

    for (auto &item : inodeList_) {
      auto childNode = target->bucket_->getCachedNode(item.pageId);
      if (childNode) {
        childNode->parentNode_->removeChild(childNode);
        childNode->parentNode_ = target;
        childNode->parentNode_->children_.push_back(childNode);
      }
    }

    std::copy(inodeList_.begin(), inodeList_.end(),
              std::back_inserter(target->inodeList_));
    parentNode_->del(this->key_);
    parentNode_->removeChild(shared_from_this());
    bucket_->eraseCachedNode(this->pageId_);
    this->free();
  }

  // as parent node has one element removed, re-balance it
  parentNode_->rebalance();
}

// 从inodeList_里面去搜索，因为这里面存放了所有的下一层node节点的指针
// 因为inodeList_是有序的，所以用二分查找
uint32_t Node::childIndex(NodePtr child) {
  bool found = false;
  auto index = binarySearch(inodeList_, child->key_, found);
  assert(found); // 进这个函数一定能找到
  return index;
}

void Node::removeChild(NodePtr target) {
  auto it = children_.begin();
  while (it != children_.end()) {
    if (*it == target) {
      children_.erase(it);
      return;
    }
    it++;
  }
  LOG(ERROR) << "no child been removed!";
}

// 首先从parentNode_中找到自己的index，然后从bucket里面获取下一个node，也就是index+1的指针，如果之前没使用过nextSibling，由bucket负责申请（申请node不需要落盘，pgid=0）
NodePtr Node::nextSibling() {
  if (parentNode_ == nullptr) {
    return nullptr; // root node只有一个node
  }
  auto idx = parentNode_->childIndex(shared_from_this());
  if (idx == 0) {
    return nullptr; // TODO(roland):何时会有这种状况？
  }
  return parentNode_->childAt(idx + 1);
}

// BoltDB实现的B+Tree，找prev 和next页的时间复杂度是相同的。
NodePtr Node::prevSibling() {
  if (parentNode_ == nullptr) {
    return nullptr;
  }
  auto idx = parentNode_->childIndex(shared_from_this());
  if (idx == 0) {
    return nullptr;
  }
  return parentNode_->childAt(idx - 1);
}

bool Node::isinlineable(uint32_t maxInlineBucketSize) const {
  uint32_t s = PAGEHEADERSIZE;
  for (auto item : inodeList_) {
    s += sizeof(struct leafPageElement) + item.key.length_ + item.value.length_;

    if ((item.flag & bucketLeafFlag)) {
      return false;
    }
    if (s > maxInlineBucketSize) {
      return false;
    }
  }
  return true;
}

uint32_t Node::search(const Item &key, bool &found) {
  return binarySearch(inodeList_, key, found);
}

void Node::do_remove(const Item &key) {
  bool found = false;
  uint32_t index = binarySearch(inodeList_, key, found);
  if (!found) {
    return;
  }

  auto b = inodeList_.begin();
  std::advance(b, index);
  inodeList_.erase(b);

  unbalanced_ = true; // need re-balance
}

std::vector<pgid> Node::branchPageIds() {
  std::vector<pgid> result;
  for (auto &item : inodeList_) {
    result.push_back(item.pageId);
  }
  return result;
}