[![Build Status](https://travis-ci.com/rolandqi/cbtree.svg?branch=main)](https://travis-ci.com/rolandqi/cbtree)

# cbtree

implement several b-tree structures in c++.

## boltDB

[BoltDB源码分析](doc/bolt_analysis_cn.md)

1. using mmap file to manange page.
2. tx->commit->allocate(tx->DB->freepage)

### implementation

- [x] page(meta/freelist/data) management
- [x] gtest/glog capability
- [x] node management
- [x] transaction
- [x] bucket(B+Tree) management
- [x] unit test
- [x] [performance test](doc/bolt_performance_report.md)

### TODO

- [ ] better memory management (avoid some copy)
- [ ] palm
- [ ] transaction manager
