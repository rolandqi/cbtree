# BoltDB 源码分析

- [BoltDB 源码分析](#boltdb-源码分析)
  - [node](#node)
  - [Element](#element)
  - [bucket](#bucket)
  - [内存分配](#内存分配)
  - [inline bucket](#inline-bucket)
  - [Cursor](#cursor)
  - [rebalance](#rebalance)
  - [事务](#事务)
    - [原子性（Atomicity）](#原子性atomicity)
    - [一致性（Consistency）](#一致性consistency)
    - [隔离型（Isolation）](#隔离型isolation)
    - [持久性（Durability）](#持久性durability)

---

## node

node为一个page在内存中的体现，也是数据插入的基本单元，每个node下存在innode，真正的存储kv

每个node都有children和innode（如果innode没有落盘，则不会分配pgid），并且有指针反指回parentNode

非leaf层的node节点至少要有2个inode元素

理论上，node中的元素，有一部分是mmap上来的指针地址，有一部分是新插入的元素。所以如果需要数据库需要resize重新mmap的时候，就需要将之前mmap的指针全部拷贝到内存中。因此数据库resize是个很重的操作。

## Element

分为`branchPageElement`和`leafPageElement`其中：

- `branchPageElement`指定了key的值和下一层的pgid，从而可以继续向下查找
- `leafPageElement`通过`flag`指明当前leaf的内容
  - `bucketLeafFlag`表明当前的leaf中存储的是其他的bucket，也就是**bucket-root**，一个blotDB存在唯一一个rootbucket
  - `flag==0`说明每个leaf里面是按顺序存放的kv对，通过成员变量`pos`标明位置

- `leafPageElement`

```txt
|page|leafPageElement|leafPageElement|leafPageElement|...|leaf key|leaf value|leaf key|leaf value|...|
```

- `branchPageElement`

```txt
|page|branchPageElement|branchPageElement|branchPageElement|...|branch key|branch key|...|
```

上层branchnode存放下层node的第一个key


## bucket

bucket是一些列的键值对的集合。一个bucket相当于一个命名空间，每个bucket中表示了一个完整的b+树，另外bucket可以嵌套。对数据的增删改查都基于bucket。

Bucket类比于mysql中的table，在boltdb中，**meta页面中有一个成员bucket，其存储了整个数据库根bucket的信息**，**而一个数据库中存储的其他table的信息，则作为子bucket存储到Bucket中**。其关系如下：

```c++
type DB struct {
  // ...
	meta0    *meta
	meta1    *meta  
}
type meta struct {
  // ...
	root     bucket	// 根bucket的信息，通过这个可以找到根bucket的page，根bucket中存放所有的其他root bucket
  // |bucket|bucket|bucket|bucket|...|
  // 每个子bucket中再保存各种映射信息
}
type Bucket struct {
	*bucket
  // ...
  buckets  map[string]*Bucket // 存储子bucket的对应关系
}
type bucket struct {
	// 根节点的page id
	root pgid // page id of the bucket's root-level page
	// 单调递增的序列号
	sequence uint64 // monotonically incrementing, used by NextSequence()
}
```

子bucket保存在`leafPageElement`中，通过其中的元素flag来标识其是否是一个bucket

```c++
struct leafPageElement {
  uint32_t flag = 0; // is this element a bucket? yes:1 (bucketLeafFlag) no:0 (存储B+树叶子页面的内容)
  uint32_t pos = 0;
  uint32_t ksize = 0;
  uint32_t vsize = 0;
  Item read(uint32_t p, uint32_t s) const {
    const auto *ptr = reinterpret_cast<const char *>(this);
    //    return std::string(&ptr[p], &ptr[p + s]);
    return { &ptr[p], s };
  }
  Item key() const {
    return read(pos, ksize);
  } // 从当前位置算偏移，得到key和value值
  Item value() const { return read(pos + ksize, vsize); }
} __attribute__((packed));
```

Bucket会有一个当前关联的事务`Tx`

## 内存分配

在一个bucket创建的时候，会创建与之对应的node。然后会开辟一片内存，存放存放bucketHeader和node的数据结构，具体代码在`Bucket::write()`函数中，内存分布如下：

```txt
内存中元素分布：
|bucketHeader||page header | leaf/branch element .... | kv pair ...  |

分布示意图：
|<--bucket-->|<--                  node...                        -->|
```

## inline bucket

如果子Bucket中的数据量很少，就会造成磁盘空间的浪费。为了针对这类型Bucket进行优化，boltdb提供了inline page这个特殊的页面，将小的子Bucket数据存放在这里。

这类型的子Bucket需要满足以下两个条件：

- 该子Bucket再没有嵌套的子Bucket了。
- 整个子Bucket的大小不能超过page size/4。

## Cursor

由于数据在inodes是按顺序存放的，因此我们通过cursor进行二分，他会从rootbucket向下查找，并将路上的element放入stack中。最终，stack顶部的元素就是叶子节点，可以可以进行CRUD操作。

首先通过`meta->root_`找到root—bucket，然后cursor就会从这个地方为起点进行search

## rebalance

由于CRUD操作是在内存中进行的，因此下刷磁盘的时候需要调整B+Tree结构。此时会涉及两个操作：

1. **rebalance**:删除操作会对node打上unbalanced标记，因为删除数据可能会引起page填充率不够，此时会对这些节点检查并进行合并。如果水位超过25%就不需要rebalance
   1. 情况一、当前的parent node只有一个节点，将下层的节点提升
   2. 情况二、当前node已经不存在任何inode了，需要移除
   3. 情况三、本层的两个node合并，选择相邻的两个节点，将右边节点的内容移入左边
2. **spill**：添加操作会使得page填充率过高，需要对节点进行分裂。如果超过水位（默认50%）就需要进行spill
   1. 实际spill的时候，首先在bucket层面进行spill自底向下spill，在这个过程中spill node

## 事务

Bolt中的事务类Tx代表具体的事务。

每次事务开始的时候（创建Tx类）的时候，创建rootBucket_，通过meta进行初始化

写事务需要将事务id加一。DB类中的锁保证同一时间只有一个写事务。读写事务通过读写锁进行并发控制。

### 原子性（Atomicity）

使用COW操作，每次update都要重写root到实际node的所有页，包括meta，meta page写入成功事务才算更新成功。

### 一致性（Consistency）

### 隔离型（Isolation）

### 持久性（Durability）
