## 相关结构体的定义

PostgreSQL的表达式模块主要与下面结构体有关:

### 1. Expr

​	在PostgreSQL内部，`Expr`（Expression）结构体及其子类是表示SQL语句中各种表达式的核心数据结构。这些表达式可以是简单的常量、列引用，也可以是复杂的函数调用、运算符操作、子查询等。理解`Expr`及其子类对于深入了解PostgreSQL的查询解析、优化和执行过程至关重要。

​	`Expr`本身是一个抽象基类，它定义了所有表达式节点共有的基本属性。在PostgreSQL的C语言源码中，它通常作为一个`union`的一部分，或者通过`NodeTag`来区分不同的具体表达式类型。所有表示表达式的结构体都会包含一个`NodeTag`字段，用于标识其具体的类型。

简而言之，`Expr`的作用是：

1. **统一表示：** 提供一个统一的接口来处理各种不同类型的表达式，无论它们是简单的还是复杂的。
2. **树形结构：** 所有的表达式在内部都以树形结构（或称为表达式树、AST - 抽象语法树）表示。`Expr`及其子类构成了这棵树的节点。
3. **阶段性处理：** 在PostgreSQL的查询处理流程中，表达式树会在不同阶段（解析、重写、规划、执行）被构建、转换和优化。

#### `Expr` 的核心属性（通常在 `Node` 中定义）

由于`Expr`是更通用的`Node`结构体的一种特化，它通常会继承或包含`Node`的基本属性，例如：

- `NodeTag type;`：这是最重要的字段，它是一个枚举值（例如 `T_OpExpr`、`T_FuncExpr`等），用于标识当前节点的具体类型。PostgreSQL通过这个`NodeTag`来区分不同的表达式类型，并进行相应的处理。

#### 表达式树的构建和转换

SQL语句在PostgreSQL内部的处理流程大致如下：

1. **解析 (Parsing)：** SQL文本首先通过词法分析和语法分析，生成一个“原始解析树”（Raw Parse Tree）。在这个阶段，会使用像 `A_Expr` 这样的通用节点来表示操作符。
2. **分析/语义分析 (Analyze/Semantic Analysis)：** 原始解析树被转换为一个“查询树”（Query Tree）。在这个阶段，PostgreSQL会进行类型检查、函数/操作符重载解析、权限检查等。例如，`A_Expr` 可能会被转换为更具体的 `OpExpr` 或 `FuncExpr`。
3. **重写 (Rewriting)：** 查询树根据规则系统进行重写，例如视图展开、规则应用等。
4. **规划 (Planning)：** 规划器接收查询树，并生成一个“执行计划”（Plan Tree）。在规划阶段，表达式树可能会被进一步优化和转换，例如常量折叠、子表达式消除等。
5. **执行 (Execution)：** 执行器根据执行计划执行查询，遍历计划树并计算表达式的结果。

​	在整个过程中，`Expr`及其子类是核心的数据载体，它们以树形结构组织起来，代表了SQL查询中的各种计算逻辑。通过对这些结构体的递归遍历和操作，PostgreSQL能够理解、优化和执行复杂的SQL语句。

#### `Expr` 的主要子类（或具体实现结构体）

​	PostgreSQL中有很多表示不同类型表达式的结构体，它们都或多或少地遵循`Expr`的逻辑。以下是一些重要的子类（或者说，是具有`NodeTag`标识的，实际代表特定表达式类型的结构体）

#### 1.1 基本叶子节点

##### Var：

 变量引用，通常代表表中的某一列。字段包括：`varno`（范围表索引）、`varattno`（列号）、`vartype`（数据类型）、`varlevelsup`（外层引用深度）等。

##### Const

 常量节点，表示一个常量值，如数字、字符串、布尔值等。保存字面值或默认值信息；字段有 `consttype`（类型）、`constlen`、`constvalue`、`constisnull` 等。

##### Param

 参数引用，可用于表示外部参数、EXEC 参数或子查询结果；字段有 `paramkind`（`PARAM_EXTERN`、`PARAM_EXEC` 等）、`paramid`、`paramtype`

#### 1.2 函数与算子调用

##### FuncExpr

 普通函数调用，表示一个函数调用，包括内置函数、用户定义函数、聚合函数、窗口函数等，例如：now()`, `upper(text_col)等，字段包括：`funcid`（函数 OID）、`funcresulttype`、`funcretset`（是否返回集合）、以及 `List *args`（参数列表）等。

##### OpExpr

 算子调用，底层也是函数调用，额外包含 `opno`（运算符 OID）、`opresulttype`、`opretset` 以及 `List *args`。`DistinctExpr` 和 `NullIfExpr` 都是它的 `typedef` 别名。

##### ScalarArrayOpExpr

 标量与数组的比较（如 `x = ANY (array)`）；字段有 `useOr`（决定 `= ANY` 时使用 OR，`<> ALL` 时使用 AND）、`inputcollid`、`List *args`

##### AggRef

**作用：** 表示对聚合函数的引用，例如 `COUNT(*)`、`SUM(col)`、`AVG(col)`。

**示例：** `SELECT COUNT(*) FROM my_table`

**关键字段：**

- `aggfnoid`: 聚合函数的OID。
- `aggargtypes`: 聚合函数参数的类型。
- `args`: 聚合函数的参数列表。
- `aggorder`: `ORDER BY` 子句。
- `aggdistinct`: `DISTINCT` 标志。

##### WindowFunc (窗口函数引用)

**作用：** 表示对窗口函数的引用，例如 `ROW_NUMBER() OVER (...)`、`SUM(col) OVER (...)`。

**示例：** `SELECT ROW_NUMBER() OVER (ORDER BY id) FROM my_table`

**关键字段：**

- `winfnoid`: 窗口函数的OID。
- `args`: 窗口函数的参数列表。
- `winref`: 引用哪个窗口定义。

#### 1.3 逻辑与子查询

##### BoolExpr

 布尔逻辑节点，支持 `AND`、`OR`、`NOT` 三种类型，字段包括 `boolop`（枚举 `AND_EXPR/OR_EXPR/NOT_EXPR`）和 `List *args`。

##### SubLink

 子查询表达式，如 `EXISTS (subselect)`、`x IN (subselect)` 等。字段有 `subLinkType`（`EXISTS_SUBLINK`、`ANY_SUBLINK` 等）、`testexpr`、`operName`、`operOids`、`subselect`（`struct Query *`）等。

#### 1.4 条件与聚合特性

##### CaseExpr/ CaseWhen/ CaseTestExpr

 `CASE … WHEN … THEN …` 结构；`CaseExpr` 包含 `List *args`（多组 `CaseWhen`）、`defresult`（默认结果）、`casetype`（结果类型）等，`CaseWhen` 则含条件与结果表达式。

##### CoalesceExpr

 `COALESCE(a, b, …)`，字段有 `List *args`、`coalescetype`、`coalescecollid`。

##### MinMaxExpr

 `GREATEST`/`LEAST`，字段包括 `MinMaxOp` 枚举与 `List *args`。

##### SQLValueFunction

 系统函数如 `CURRENT_DATE`、`CURRENT_TIME` 等，字段为 `SQLValueFunctionOp`。

#### 1.5 类型转换与访问控制

##### CoerceExpr(类型强制转换)

- **作用：** 表示显式的类型强制转换，例如 `CAST(expr AS type)` 或 `expr::type`。
- **示例：** `'123'::integer`, `CAST('abc' AS varchar(10))`
- 关键字段：
    - `arg`: 被转换的表达式。
    - `resulttype`: 目标类型 OID。
    - `coerceformat`: 强制转换的格式（显式、隐式等）。

##### ArrayExpr (数组表达式)

- **作用：** 表示一个数组构造器。
- **示例：** `ARRAY[1, 2, 3]`, `ARRAY['a', 'b']`
- 关键字段：
    - `elements`: 数组元素的列表。
    - `elementtype`: 数组元素的类型 OID。

##### RowExpr(行构造器)

- **作用：** 表示一个行构造器，用于创建匿名复合类型值。
- **示例：** `(1, 'abc', TRUE)`
- 关键字段：
    - `args`: 行中各个字段的表达式列表。
    - `row_typeid`: 行的复合类型 OID。

##### RelabelType/CoerceViaIO/ArrayCoerceExpr/ConvertRowtypeExpr

 各类类型转换节点，用于显示/强制类型转换或数组类型间的转换。

##### CollateExpr/CoerceToDomain/CoerceToDomainValue/SetToDefault

 排序规则、域类型强制、域值检查以及默认值设置相关的表达式节点。

#### 1.6 XML/JSON 专用表达式

**`XmlExpr`** / **`XmlOptionType`**, **`JsonExpr`**, **`JsonFormat`**, **`JsonValueExpr`**, **`JsonConstructorExpr`**, **`JsonIsPredicate`**, **`JsonBehavior`** 等
 用于处理 SQL/XML 与 SQL/JSON 标准中的各种构造与谓词

#### 1.7 其他实用节点

**`FieldSelect`** / **`FieldStore`**：记录类型字段的访问与赋值。

**`CurrentOfExpr`** / **`NextValueExpr`**：游标 `WHERE CURRENT OF` 与序列 `nextval()` 调用。

**`MergeAction`**：`MERGE` 语句中的动作定义（`MATCHED`/`NOT MATCHED`）。

**`ReturningExpr`**：`RETURNING` 子句中的特殊表达式。

**`InferenceElem`**：唯一索引推断元素。



### 2. ExprState：

​	`Expr` 描述了“表达式是什么”，而 `ExprState` 描述了“如何计算这个表达式，以及计算过程中需要维护哪些状态”。

​	`ExprState` 结构体是执行器（Executor）模块的核心组件，用于管理和求值查询执行过程中的各种表达式（如 `WHERE` 子句条件、计算列等）。

#### `ExprState` 的核心属性

```c
typedef struct ExprState
{
    NodeTag         type;               /* 节点类型，恒为 T_ExprState */

    uint8           flags;              /* 位标志，详见 EEO_FLAG_*，
    																			EEO_FLAG_IS_QUAL：该表达式用于行过滤（ExecQual）。
																					EEO_FLAG_HAS_OLD / EEO_FLAG_HAS_NEW：RETURNING 中引用到 OLD/NEW 行
																				*/

    /* 标量表达式结果存放，在每次 ExecEvalExpr() 调用后由 evalfunc 更新 */
    bool            resnull;            /* 结果是否为 NULL */
    Datum           resvalue;           /* 结果值 */

    /* 如果表达式返回一个行型（tuple），则用此槽保存，
     对于返回整个元组的表达式（如投影复合类型、ROW(...) 构造），将结果写入此槽
     */
    TupleTableSlot *resultslot;

    /* 表达式的指令序列，可以通过顺序执行这些执行来执行表达式，
       每个 ExprEvalStep 描述一步操作（载入常量、调用函数、访问 Var、比较、算术运算等），
       数组长度：由 steps_len 记录，初始化时由 ExecInitExprRec() 填充 */
    struct ExprEvalStep *steps;

    /* 真正执行求值的函数指针（解释型 / JIT 型）, 表达式执行的入口，通过调用该函数进行表达式的执行 */
    ExprStateEvalFunc evalfunc;

    /* 原始表达式树，用于调试 */
    Expr           *expr;

    /* evalfunc 的私有数据（如 JIT 上下文） */
    void           *evalfunc_private;

    /* —— 以下字段仅在 ExecInitExpr 阶段用 —— */
    int             steps_len;         /* 当前指令数 */
    int             steps_alloc;       /* 指令数组分配长度 */

    /* 回溯到所属的 PlanState 节点 */
    PlanState      *parent;

    /* 用于编译 PARAM_EXTERN 节点时的参数列表 */
    ParamListInfo   ext_params;

    /* CASE/CoerceToDomain 等节点所需的临时存储 */
    Datum          *innermost_caseval;
    bool           *innermost_casenull;
    Datum          *innermost_domainval;
    bool           *innermost_domainnull;

    /* 软错误（如类型转换失败）保存上下文 */
    ErrorSaveContext *escontext;
} ExprState;
```

#### 初始化与执行流程

##### 初始化阶段（`ExecInitExpr()`）

- 分配 `ExprState`，并将 `expr`、`parent`、`ext_params` 等字段设置好。
- 调用 `ExecInitExprRec()` 遍历 `Expr` 树，为每个子节点生成对应的 `ExprEvalStep`(调用ExecInitExprRec())，填充 `steps` 数组。
- 根据表达式复杂度，选择合适的 `evalfunc`（解释型或 JIT 型，并做一些相对应的前置准备。	
    - 解释执行，这里由分为传统的解释执行(switch case)和computed goto两种，由宏EEO_USE_COMPUTED_GOTO控制，在编译时决定。
    - 编译执行，依靠LLVM将表达式编译为机器码执行。 2. 执行表达式，会依靠之前初始化时确定的执行方式:

​		解析执行，通过调用函数ExecInterpExprStillValid()

​		编译执行，通过调用函数ExecRunCompiledExpr()

- （可选）丢弃编译时临时字段 `steps_len/steps_alloc`，仅保留 `steps` 和 `evalfunc`。

##### 执行阶段（`ExecEvalExpr()`）

- 切换内存上下文到 `ExprContext->ecxt_per_tuple_memory`。
- 调用 `evalfunc(exprstate, econtext, &resnull)` 执行指令流，结果存入 `resvalue`/`resnull` 或 `resultslot`。
- 如果是投影表达式，则由上层逻辑将 `resvalue` 写入目标 `TupleTableSlot`。