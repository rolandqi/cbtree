# cbtree

implement several b-tree structures in c++.

## boltDB

1. using mmap file to manange page.
2. tx->commit->allocate(tx->DB->freepage)

### implementation

[x] 1. page(meta/freelist/data) management
[x] 2. gtest/glog capability
[ ] 3. bucket/node(B+Tree) management
[ ] 4. unit test
[ ] 5. performance test