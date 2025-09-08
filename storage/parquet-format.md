## Parquet 文件总体结构

Parquet 是一种自描述的列式存储格式，包含元数据，布局上由多个 **Row Group** 组成，每个 Row Group 包含多个 **Column Chunk**，每个 Column Chunk 由多个 **Page** 组成，最后是 **Footer** 包含元数据，整体结构如下:

```
+-----------------+
| File Header     |   <-- magic bytes "PAR1"
+-----------------+
| Row Group 1     |   <-- 包含多列数据，每列一列式存储
|   Column Chunk  |
|   Column Chunk  |
+-----------------+
| Row Group 2     |
|   Column Chunk  |
+-----------------+
| ...             |
+-----------------+
| File Footer     |   <-- 元数据，描述 schema、行组、列块信息
+-----------------+
| Footer Magic    |   <-- magic bytes "PAR1"
+-----------------+
```

**Row Group**：逻辑上包含若干行数据，是列式存储的单位。

**Column Chunk**：每列数据在磁盘上的存储块。

**Page**：列块中更小的存储单元，分为 **Data Page**、**Dictionary Page** 等。



## 元数据（File Footer）

Footer 使用 **Thrift** 定义，主要包含：

1. **Schema**：字段名、类型、是否可空、嵌套结构（map/list/struct）。
2. **Row Groups 信息**：
    - 每个 row group 的行数
    - 每列对应 column chunk 的偏移、大小
    - 压缩算法（Snappy、Gzip、Brotli 等）
3. **统计信息**：
    - 每列最小值、最大值
    - 空值数
    - 便于查询和过滤（predicate pushdown）
4. **版本信息**和**其他自定义 key-value metadata**。



## 数据布局细节

### Row Group

- Row group 是列式存储的最小可读单元，通常包含几十万行。
- 设计上：
    - 每列单独存储在 column chunk
    - 每列可以使用不同的编码和压缩策略
- 优势：
    - 可跳过不需要的列
    - 压缩效果好（同列数据相似度高）

### Column Chunk

- 每列存储为 **一块连续数据**。
- 可以使用：
    - **Plain Encoding**：原始存储
    - **Dictionary Encoding**：小值重复列用字典压缩
    - **Run-Length Encoding (RLE)**：重复值列
    - **Bit-Packing**：布尔或枚举列

### Page

- 列块内的数据被划分为页面：
    - **Dictionary Page**：存储字典
    - **Data Page**：存储实际数据
- Page 级别可压缩，减少内存占用。



## 举例：数据布局

employe(id int, name text,salary float)

```
+---------------------------------------------+
|                 File Header                 |
|                "PAR1" magic bytes           |
+---------------------------------------------+
|                 Row Group 1                 |
|  num_rows: 100000                            |
|  statistics:                                  |
|    id: min=1, max=100000, nulls=0           |
|    name: min="Alice", max="Charlie", nulls=15|
|    salary: min=1000, max=20000, nulls=2     |
|                                             |
|  Column Chunk: id                            |
|    offset: 4KB                               |
|    total_size: 16KB                          |
|    compression: Snappy                        |
|    Page 1 (Data Page)                        |
|      values: 1,2,...                         |
|      statistics: min=1, max=50000, nulls=0   |
|    Page 2 (Data Page)                        |
|      values: 50001,...100000                 |
|      statistics: min=50001, max=100000,nulls=0|
|                                             |
|  Column Chunk: name                          |
|    offset: 20KB                              |
|    total_size: 12KB                          |
|    compression: Gzip                          |
|    Dictionary Page                            |
|      0: "Alice"                               |
|      1: "Bob"                                 |
|      2: "Charlie"                             |
|    Data Page 1                                |
|      indices: 0,1,2                            |
|      statistics: min="Alice", max="Charlie", nulls=10 |
|    Data Page 2                                |
|      indices: ...                             |
|      statistics: min="Bob", max="Charlie", nulls=5  |
|                                             |
|  Column Chunk: salary                        |
|    offset: 32KB                              |
|    total_size: 16KB                          |
|    compression: Snappy                        |
|    Page 1                                     |
|      values: 1000,1500,...                   |
|      statistics: min=1000, max=9000, nulls=0 |
|    Page 2                                     |
|      values: ...                             |
|      statistics: min=9500, max=20000, nulls=2 |
+---------------------------------------------+
|                 Row Group 2                 |
|  num_rows: 80000                             |
|  statistics:                                  |
|    id: min=100001, max=180000, nulls=0      |
|    name: min="Dave", max="Eve", nulls=2     |
|    salary: min=1100, max=12000, nulls=1     |
|                                             |
|  Column Chunk: id                            |
|    offset: 48KB                              |
|    total_size: 12KB                          |
|    compression: Snappy                        |
|    Page 1                                     |
|      values: 100001,...140000                |
|      statistics: min=100001,max=120000,nulls=0 |
|                                             |
|  Column Chunk: name                          |
|    offset: 60KB                              |
|    total_size: 8KB                            |
|    compression: Gzip                          |
|    Dictionary Page                            |
|      0: "Dave"                                |
|      1: "Eve"                                 |
|    Data Page 1                                |
|      indices: 0,1,...                         |
|      statistics: min="Dave", max="Eve", nulls=2 |
|                                             |
|  Column Chunk: salary                        |
|    offset: 68KB                              |
|    total_size: 10KB                           |
|    compression: Snappy                        |
|    Page 1                                     |
|      values: 1100,...                         |
|      statistics: min=1100,max=12000,nulls=1 |
+---------------------------------------------+
|                 File Footer                 |
|  Metadata:                                  |
|    - Schema                                 |
|    - RowGroup info:                         |
|        RowGroup 1: offset=4KB, num_rows=100000, statistics: (see above) |
|          ColumnChunk id: offset=4KB,size=16KB,compression=Snappy |
|          ColumnChunk name: offset=20KB,size=12KB,compression=Gzip|
|          ColumnChunk salary: offset=32KB,size=16KB,compression=Snappy|
|        RowGroup 2: offset=48KB, num_rows=80000, statistics: (see above) |
|          ColumnChunk id: offset=48KB,size=12KB,compression=Snappy|
|          ColumnChunk name: offset=60KB,size=8KB,compression=Gzip|
|          ColumnChunk salary: offset=68KB,size=10KB,compression=Snappy|
|    - Statistics for predicate pushdown       |
|    - Compression info                         |
+---------------------------------------------+
|             Footer Magic "PAR1"             |
+---------------------------------------------+
```

