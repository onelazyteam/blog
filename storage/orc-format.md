## 背景

​	ORC（Optimized Row Columnar）是一种面向大数据场景的列式存储格式，主要用在 Hadoop 生态与分析型数据库中，优势是压缩比高、扫描效率好并支持复杂类型与统计信息。

## ORC 文件内部结构

每个 ORC 文件由如下部分组成（顺序上）：

**1. Magic header**（文件头）

**2. Stripes（重复）**：每个 Stripe 含：

- Index streams（可选）
- Data streams（按列拆分）
- Stripe footer（列位置偏移、统计、streams 描述）

**3. File footer**：全局 schema、stripes 列表、统计摘要、metadata key-value

**4. Magic footer**（文件尾）

### Stripe 内部

- Stripe 大小策略（推荐默认 64MB - 256MB，可配置）
- 每列维护多种 stream：数据、length、present（null 位图）、dictionary（若用）
- Stripe footer 应包含每个 stream 的起止偏移、压缩后大小、列级统计信息（min/max/count/nulls），便于跳过/裁剪

### 压缩与编码

- 支持多种压缩算法（none、zlib/snappy/zstd/lz4），可在文件级或 stripe 级选择

- 编码层次：`EncodingStrategy -> Encoder`，默认支持：

    - Dictionary Encoding（适合低基数字符串）

    - Run-Length Encoding（RLE，适合重复值）

    - Delta Encoding（数值序列）

    - Direct / Plain

- 设计：编码器可插拔，支持自适应策略（采样数据决定是否启用 dictionary）

### 示例场景

表：`orders(order_id:int, ts:date, customer:string, amount:decimal, status:string)`

- Stripe 大小 ≈ 128MB，每个 Stripe ~100 万行。
- RowGroup 大小 = 10k 行，所以每个 Stripe 有 100 个 RowGroup。
- 文件有 2 个 Stripe。

```c
+=================================================================================+
| ORC FILE: orders.orc                                                            |
+=================================================================================+
| Header ("ORC")                                                                  |
+---------------------------------------------------------------------------------+
| Stripe 1 (rows ≈ 1,000,000; RowGroups = 100)                                    |
|   +---------------------------------------------------------------------------+ |
|   | Index Data (per column, per RowGroup stats)                               | |
|   |   order_id: RG0[min=1,max=10000], RG1[min=10001,max=20000], ...           | |
|   |   ts:       RG0[min=2025-08-01,max=2025-08-01], ...                       | |
|   |   status:   RG0[min="CANCELLED",max="PAID"], Bloom={"PAID"}               | |
|   +---------------------------------------------------------------------------+ |
|   | Row Data (encoded columnar data streams)                                  | |
|   |   order_id → RLEv2 encoded [1..1000000]                                   | |
|   |   ts       → Delta encoded int64 timestamps                               | |
|   |   customer → Dictionary {0:"Alice",1:"Bob",...}, Encoded [0,1,2,...]      | |
|   |   amount   → Decimal scaled int encoding                                  | |
|   |   status   → Low-cardinality dict + Bloom filters                         | |
|   +---------------------------------------------------------------------------+ |
|   | Stripe Footer: streams map, col stats, checksum, row count=1,000,000      | |
|   +---------------------------------------------------------------------------+ |
+---------------------------------------------------------------------------------+
| Stripe 2 (rows ≈ 1,000,000; RowGroups = 100)                                    |
|   +---------------------------------------------------------------------------+ |
|   | Index Data                                                                | |
|   |   order_id: RG100[min=1,000,001,max=1,010,000], ...                       | |
|   |   ts:       RG100[min=2025-11-09,max=2025-11-09], ...                     | |
|   |   status:   RG100[min="PAID",max="REFUNDED"], Bloom={"PAID"}              | |
|   +---------------------------------------------------------------------------+ |
|   | Row Data (同 Stripe1，编码方式一致)                                       | |
|   +---------------------------------------------------------------------------+ |
|   | Stripe Footer: row count=1,000,000                                        | |
|   +---------------------------------------------------------------------------+ |
+---------------------------------------------------------------------------------+
| ... (更多 Stripe)                                                               |
+---------------------------------------------------------------------------------+
| File Footer                                                                     |
|   - Schema: {order_id:int, ts:date, customer:string, amount:decimal, status:str}|
|   - Stripe directory: [offset, length, rowCount] per stripe                     |
|   - Global column stats (min/max/nulls/distinctCount per col)                   |
|   - User metadata (kv)                                                          |
+---------------------------------------------------------------------------------+
| PostScript                                                                      |
|   - footerLength, compression=zstd, writerVersion=ORC-1.6                       |
|   - magic="ORC"                                                                 |
+---------------------------------------------------------------------------------+
| Magic ("ORC")                                                                   |
+=================================================================================+

```

```sql
SELECT SUM(amount)
FROM orders
WHERE ts BETWEEN '2025-08-01' AND '2025-08-07'
  AND status = 'PAID';
```

#### 执行过程：

1. **File Footer** → 找到所有 Stripe 位置。
2. **Stripe 级裁剪**：只读取 Stripe1（因为 ts 范围覆盖 8 月份，Stripe2 的 ts 在 11 月）。
3. **RowGroup 级裁剪**：在 Stripe1 中，RowGroup 0–6 的 ts 范围匹配，Bloom 显示包含 "PAID"，只读取这些 RowGroup。
4. **列裁剪**：只读 `amount`, `ts`, `status` 三列。
5. **解码 → 过滤 → 聚合**。



交易表 `transactions(user_id:int, ts:date, status:string, amount:decimal)`

```sql
SELECT user_id, amount
FROM transactions
WHERE status = 'FAILED' AND ts BETWEEN '2023-01-01' AND '2023-01-02';
```

```
ORC File
+-----------------------------------------------------------------------------------+
| Stripe 1 (rows 0 - 999,999)                                                       |
|  +-------------------------+   +-------------------------+   +------------------+ |
|  | RowGroup 1 (0-9,999)    |   | RowGroup 2 (10k-19,999) | … | RowGroup 100 (..) | |
|  | min(ts)=2023-01-01      |   | min(ts)=2023-01-03      |   | min(ts)=2023-01-08 | |
|  | max(ts)=2023-01-01      |   | max(ts)=2023-01-04      |   | max(ts)=2023-01-09 | |
|  | Bloom(status)={OK,FAIL} |   | Bloom(status)={OK}      |   | Bloom(status)={OK} | |
|  +-------------------------+   +-------------------------+   +------------------+ |
|         ^ MATCH ✅                ^ SKIP ❌                     ^ SKIP ❌            |
+-----------------------------------------------------------------------------------+

+-----------------------------------------------------------------------------------+
| Stripe 2 (rows 1,000,000 - 1,999,999)                                             |
|  +-------------------------+   +-------------------------+   +------------------+ |
|  | RowGroup 1 (.. )        |   | RowGroup 2 (.. )        |   | RowGroup N        | |
|  | min(ts)=2023-01-02      |   | min(ts)=2023-01-05      |   | ...                | |
|  | max(ts)=2023-01-02      |   | max(ts)=2023-01-06      |   | ...                | |
|  | Bloom(status)={FAILED}  |   | Bloom(status)={OK}      |   | ...                | |
|  +-------------------------+   +-------------------------+   +------------------+ |
|         ^ MATCH ✅                ^ SKIP ❌                      ...                |
+-----------------------------------------------------------------------------------+

+-----------------------------------------------------------------------------------+
| Stripe 3 (rows 2,000,000 - …)                                                     |
|   All RowGroups ts >= 2023-01-10 → SKIP ❌ (index prune)                          |
+-----------------------------------------------------------------------------------+

Footer / PostScript
- 记录每个 Stripe 的统计信息
- 查询优化器利用这些统计做跳过
```

##### 过程说明

1. **Stripe 1**
    - RowGroup1 (ts=2023-01-01, status={OK,FAIL}) → **命中**
    - RowGroup2 (ts=2023-01-03, status={OK}) → **跳过**
    - 后续 RowGroup → 全部跳过
2. **Stripe 2**
    - RowGroup1 (ts=2023-01-02, status={FAILED}) → **命中**
    - RowGroup2 (ts=2023-01-05, status={OK}) → **跳过**
3. **Stripe 3**
    - ts ≥ 2023-01-10 → 整个 Stripe 都被 **跳过**



#### 写入流程:

场景：交易表 `transactions(user_id:int, ts:date, status:string, amount:decimal)`

假设：

- Stripe ≈ 128MB
- RowGroup = 10,000 行
- 文件分 2 个 Stripe

```
Incoming Rows (Stream / Batch)
+------------------------+
| Row 1: 1001, 2023-01-01, OK, 50.0 |
| Row 2: 1002, 2023-01-01, FAILED, 25.0 |
| ...                                |
| Row 10,000                        |
+------------------------+
        |
        v
+------------------------+
| Column Buffers (in-memory)   <- 每列一个缓冲区
| user_id   | ts     | status | amount
| ----------|--------|--------|------
| 1001      |2023-01-01|OK    |50.0
| 1002      |2023-01-01|FAILED|25.0
| ...      | ...     | ...   | ...
+------------------------+
        |
        v
RowGroup 完成（10,000 行） → 生成 RowGroup Index
+------------------------+
| RowGroup Metadata       |
| order_id: min/max       |
| ts: min/max             |
| status: Bloom / min/max |
| amount: min/max         |
+------------------------+
        |
        v
继续填充 Column Buffers
直到 Stripe 满（如 1,000,000 行） → 写入 Stripe
+-------------------------------------------------------------+
| Stripe 1                                                   |
|  Index Streams (每列 RowGroup 级统计)                     |
|  Data Streams (列式编码：RLE / Delta / Dictionary / Direct)|
|  Stripe Footer (streams map, offsets, col stats)          |
+-------------------------------------------------------------+
        |
        v
清空 Column Buffers
开始写 Stripe 2
...
        |
        v
文件写完 → 写 File Footer + PostScript + Magic
+-------------------------------------------------------------+
| File Footer: schema, stripes list, global col stats, meta  |
| PostScript: compression, footer length, writer version    |
| Magic: "ORC"                                              |
+-------------------------------------------------------------+
```

##### 流程说明

1. **行数据 → 列缓冲**
    - 数据写入内存缓冲，每列单独存放。
2. **RowGroup 完成 → 写索引**
    - 每 RowGroup 生成 min/max、nullCount、Bloom Filter 等索引。
3. **Stripe 完成 → 写磁盘**
    - 包含 Index Streams + Data Streams + Stripe Footer。
    - 支持列裁剪、压缩和跳过。
4. **文件完成 → 写 File Footer + PostScript + Magic**
    - 全局统计、Schema、所有 Stripe 信息。



### 读写代码实例

[ORC读写代码](https://github.com/onelazyteam/blog/tree/master/storage/code/orc)