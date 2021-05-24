# bolt performance report

- [bolt performance report](#bolt-performance-report)
  - [单线程](#单线程)
  - [8byte-key 8byte-value update](#8byte-key-8byte-value-update)
    - [只有一个transaction](#只有一个transaction)
    - [每个transaction update一条记录](#每个transaction-update一条记录)

---

使用 pcie gen3 nvme SSD 进行测试。

```txt
3b:00.0 Non-Volatile memory controller: Huawei Technologies Co., Ltd. Device 3714 (rev 20)
3c:00.0 Non-Volatile memory controller: Huawei Technologies Co., Ltd. Device 3714 (rev 20)
```

## 单线程

## 8byte-key 8byte-value update

### 只有一个transaction

- random

| num of iteration（random） | 10     | 100    | 1000   | 10000 |
|--------------------------|--------|--------|--------|-------|
| QPS                      | 769230 | 694444 | 188005 | 24474 |

- sequential

| num of iteration（seq） | 10     | 100    | 1000    | 10000  |
|-----------------------|--------|--------|---------|--------|
| QPS                   | 833333 | 892857 | 1107419 | 968992 |


### 每个transaction update一条记录

- random

| num of iteration（random） | 10   | 100  | 1000 | 10000 |
|--------------------------|------|------|------|-------|
| TPS                      | 8064 | 2020 | 3688 | 3713  |

- sequential

| num of iteration（query） | 10   | 100  | 1000 | 10000 |
|-------------------------|------|------|------|-------|
| TPS                     | 8340 | 1815 | 3538 | 3882  |
