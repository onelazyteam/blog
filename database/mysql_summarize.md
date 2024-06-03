## **接口调用层次总览**

```c++
mysql_execute_command(THD* thd)  
└─ dispatch to Sql_cmd_select::execute(THD* thd)  
   └─ Query_expression::prepare()/rewrite()          -- 解析 & 重写阶段  
      └─ OLD: JOIN::optimize()                       -- 逻辑→物理计划  
         或  
      └─ NEW: FindBestQueryPlan(Hypergraph)          -- 超图优化  
         └─ create_access_paths()/AccessPath 列表  
            └─ CreateIteratorFromAccessPath(thd, path, join, batch_mode)  
               ├─ TableScanIterator::Init()/Read()/Close()  
               ├─ IndexScanIterator::Init()/Read()/Close()  
               ├─ RefIterator (点查找)  
               ├─ BKAIterator (批量键访问连接)  
               ├─ HashJoinIterator (哈希连接)  
               ├─ NestedLoopJoinIterator (嵌套循环连接)  
               ├─ FilterIterator (谓词过滤)  
               ├─ SortIterator (排序)  
               ├─ GroupByIterator/AggregateIterator (聚合)  
               ├─ MaterializeIterator (物化)  
               └─ UnionIterator / MergeIterator ...  
                  └─ （所有算子内部可能再次调用）  
                     └─ handler API: ha_index_init(), ha_index_next(), ha_read_record() …
                     
                       
客户端请求
   │
   ▼
mysql_execute_command(THD*)
   │
   ▼
Sql_cmd_xxx::execute()
   │
   ▼
Query_expression::prepare() / rewrite()
   │
   ▼
JOIN::optimize() / FindBestQueryPlan()
   │
   ▼
CreateIteratorFromAccessPath()
   │
   ▼
RowIterator::Init()
   │
   ▼
RowIterator::Read() → handler::ha_xxx()
   │
   ▼
RowIterator::Close()
   │
   ▼
结果返回给客户端


各层职责与接口详解
1. 客户端入口
函数：mysql_execute_command(THD *thd)
位置：sql/sql_parse.cc
职责：解析客户端请求，根据 SQL 类型分发到对应的 Sql_cmd_xxx::execute() 方法。
2. SQL 命令执行
函数：Sql_cmd_select::execute(THD *thd)
位置：sql/sql_select.cc
职责：处理 SELECT 语句，调用 Query_expression::execute() 进行查询执行。
3. 查询准备与重写
函数：Query_expression::prepare(THD *thd)
位置：sql/sql_union.cc
职责：准备查询，包括解析、视图展开、权限检查等。
函数：Query_expression::rewrite(THD *thd)
位置：sql/sql_union.cc
职责：重写查询，进行子查询去相关化、常量折叠等优化。
4. 查询优化
函数：JOIN::optimize()
位置：sql/sql_select.cc
职责：传统优化器，生成查询执行计划。
函数：FindBestQueryPlan()
位置：sql/join_optimizer.cc
职责：新优化器，基于超图模型生成最优查询计划。
5. 迭代器构建
函数：CreateIteratorFromAccessPath()
位置：sql/iterator.cc
职责：根据优化器生成的 AccessPath 构建对应的迭代器树。
6. 迭代器执行
接口：RowIterator::Init()
职责：初始化迭代器，准备执行环境。
接口：RowIterator::Read()
职责：读取下一行数据，可能递归调用子迭代器的 Read()。
接口：RowIterator::Close()
职责：关闭迭代器，释放资源。
7. 存储引擎接口
接口：handler::ha_rnd_init(), handler::ha_rnd_next(), handler::ha_index_read(), handler::ha_index_next()
位置：sql/handler.cc
职责：提供对底层存储引擎（如 InnoDB）的访问接口，实现数据的读取。
```

- **入口**

- - 客户端发来 SQL，经解析后走到 mysql_execute_command()，在 sql/sql_base.cc 中根据命令类型分发；SELECT 命令进入 Sql_cmd_select::execute()。

- **准备 & 重写**

- - Query_expression::prepare() → Query_block::prepare() 完成表/字段解析、JOIN 条件、GROUP BY、WINDOW 等语义校验与初步转换。

- **计划生成**

- - **旧版**：JOIN::optimize() 生成左深树形式的 QEP_TAB 结构，调用 make_join_plan()、make_join_readinfo() 等。
    - **新版**：启用 optimizer_switch="hypergraph_optimizer=on" 后，FindBestQueryPlan() 将 SQL 转为 Hypergraph，输出一组 AccessPath。

- **迭代器工厂**

- - CreateIteratorFromAccessPath() 读取每个 AccessPath 的 type，通过模板 NewIterator<...> 生成对应的 RowIterator 子类实例（如 TableScanIterator、IndexScanIterator、HashJoinIterator 等）。

- **统一迭代器接口**

- - 所有迭代器实现相同的三大方法：

- ```C++
    iterator->Init();    // 打开 handler、分配缓存
    while ((row = iterator->Read()) != nullptr) { … }
    iterator->Close();   // 释放资源
    ```

- - 根迭代器的 Init() 会递归调用子节点，Read() 驱动整个树的深度优先行流，直到数据耗尽，最后在顶层调用 Close() 完成收尾。

- **存储引擎调用**

- - 在 TableScanIterator::Read() 内部，会调用 handler::ha_index_init(), ha_index_next(), ha_read_record() 等底层 API 获取 InnoDB 或其他存储引擎的数据页。

- **监控与分析**

- - EXPLAIN ANALYZE ... FORMAT=TREE 用新执行器執行每步算子并返回实际成本、循环次数，为性能调优提供精确度量。

- 

## 关键算子实现

- **TableScanIterator** (sql/sql_executor.cc)：封装 handler 打开、范围定位、逐行读取；支持 MRR (多范围读取) 优化。

- **IndexScanIterator** (sql/sql_executor.cc)：支持 EQ_REF、RANGE_SCAN、FULLTEXT 等多种索引访问方式。

- **HashJoinIterator** (sql/item_join.cc)：构建小表哈希表，批量 probe 大表。

- **NestedLoopJoinIterator** (sql/item_join.cc)：标准嵌套循环实现。

- **FilterIterator** (sql/iterators/composite_iterators.h)：在行流中按谓词过滤。

- **SortIterator** (sql/sql_sort.cc)：基于 std::sort 或外部归并的快速排序实现。

- **GroupByIterator** / **AggregateIterator** (sql/item_sum.cc)：SUM/COUNT/AVG/MIN/MAX 聚合逻辑，可插入向量化或 SIMD 批处理。

- **MaterializeIterator**：将子树结果物化到临时表，再返回结果。



## **核心组件与文件映射**

| 功能模块        | 主要源码文件                                   | 说明                                       |
| --------------- | ---------------------------------------------- | ------------------------------------------ |
| 命令入口        | sql/sql_base.cc                                | mysql_execute_command() 分发               |
| SELECT 执行     | sql/sql_select.cc                              | Sql_cmd_select::execute() 实现             |
| 解析 & 重写     | sql/parse_tree_nodes.cc                        | AST 定义、prepare()/rewrite()              |
| 旧优化器        | sql/sql_optimizer.cc                           | JOIN::optimize()                           |
| 超图优化器      | sql/join_optimizer_hypergraph.cc               | FindBestQueryPlan()                        |
| AccessPath 定义 | sql/join_optimizer/access_path.h               | AccessPath, CreateIteratorFromAccessPath() |
| 迭代器总入口    | sql/sql_executor.cc                            | ExecIteratorExecutor()                     |
| 主要算子实现    | sql/item_sum.ccsql/item_join.ccsql/sql_sort.cc | 聚合 / 连接 / 排序                         |
| 存储引擎桥接    | sql/handler.cc                                 | ha_index_*(), ha_read_record()             |
| 性能监控集成    | sql/pfs_engine_table.cc                        | Performance Schema 插件                    |

## **调用序列示例：简单 SELECT**

以简单 SELECT id, SUM(val) FROM t WHERE flag=1 GROUP BY id 为例：

1. 客户端 → mysql_execute_command()→Sql_cmd_select::execute() MySQL开发者官网
2. Query_expression::prepare()→JOIN::optimize()/FindBestQueryPlan() 输出三棵 AccessPath：

- - TableScan(t) + Filter(flag=1)
    - HashAggregation(id)

3. CreateIteratorFromAccessPath() 生成：

```c
AggregateIterator
└─ FilterIterator
   └─ TableScanIterator
```

4. 根算子 Init() 递归打开 handler
5. 客户端逐行调用根 Read()：

- - AggregateIterator 触发 FilterIterator::Read()
    - FilterIterator 调用 TableScanIterator::Read() → ha_index_next()
    - 满足 flag=1 行累加到哈希桶

6. 结束后 Close() 递归清理

