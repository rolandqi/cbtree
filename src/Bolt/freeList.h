#ifndef FREE_LIST_H_
#define FREE_LIST_H_

#include "type.h"
#include <unordered_map>
using namespace std;

/*
* pending:正在做某种事务的
* cache_:目测是update和read都写到内存里，如果回滚直接把页加到ids_里就行了
*/

// boltdb中使用了MVCC多版本控制，写事务修改的数据都会新分配page存放，以前的page中的数据并不会被删除，而是放入pending中，待事务版本升高，旧数据持有的page便可以释放用来重新进行分配给新的写事务存放数据。

struct Page;

// 读页面内容到内存：对应操作在freelist.read中，页面数据部分保存的是当前闲置页面ID数组，将其读入ids成员中。
// 写页面内容到磁盘：对应操作在freelist.write中，读取ids数组和pending中的页面id，拼接、排序之后在一起写入磁盘。
struct freeList {
  // allocate的时候从ids_取出连续的page
  vector<pgid> ids_; // all free and available free page ids_.

  // mapping of soon-to-be free page ids_ by tx.
  // 保存事务操作对应的页面ID，键为事务ID，值为页面ID数组。这部分的页面ID，在事务操作完成之后即被释放。
  // free的时候将page放入pending_中
  unordered_map<txid, std::vector<pgid> > pending_;

  // fast lookup of all free and pending_ page ids_.
  // 标记一个页面ID可用，即这个成员中的所有键都是页面ID，而这些页面ID当前都是闲置可分配使用的。
  unordered_map<pgid, bool> cache_;
  freeList();
  void reset();
  void read(Page *page);  // 将freelist中存储的所有页读出
  void write(Page *page); // 将一个page写进free
  // list的index，所有free和pending的ids都持久化下来
  void copyall(std::vector<pgid> *dest);
  void reindex(); // 重新计算 cache_ index
  uint32_t count() const;
  uint32_t freeCount() const;
  uint32_t pendingCount() const;
  uint32_t size() const;
  void mergePageIds(vector<pgid> *dest, const vector<pgid> &src);
  pgid allocate(
      uint32_t
          numPages); // 分配连续的numPages个页，如果失败外部的调用方负责mmap额外的页
  void mergeids_(vector<pgid> *dest, const vector<pgid> &src);
  // 以下事务相关：
  void release(txid txid);
  void rollback(txid txid);
  void reload(Page *pg);
  void free(txid txid, Page *p);
};

#endif