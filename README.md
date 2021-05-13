[![Build Status](https://travis-ci.com/rolandqi/cbtree.svg?branch=main)](https://travis-ci.com/rolandqi/cbtree)

# cbtree

implement several b-tree structures in c++.

## boltDB

1. using mmap file to manange page.
2. tx->commit->allocate(tx->DB->freepage)

### implementation

- [x] page(meta/freelist/data) management
- [x] gtest/glog capability
- [ ] bucket/node(B+Tree) management
- [ ] unit test
- [ ] performance testß