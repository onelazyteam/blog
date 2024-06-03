## 1. 引言

### 1.1 JIT编译概述

​	即时编译（Just-in-Time, JIT）是一种先进的程序执行优化技术，其核心在于在程序运行时（而非传统编译器的预先编译阶段）将某些形式的解释性程序评估转换为原生机器代码 。这种动态转换的根本目标是利用运行时可用的特定上下文信息，生成高度优化的机器码，从而显著提升执行效率。

​	JIT编译的本质在于通过动态生成针对当前执行上下文的专用代码，来有效降低传统解释器模型中因其通用性而产生的性能开销 。例如，对于诸如 `WHERE a.col = 3` 这样的SQL谓词，JIT可以生成一个专门处理此表达式的函数，该函数能够被CPU直接原生执行，而非依赖于一个必须处理任意SQL表达式的通用解释器，从而在CPU层面实现显著的加速 。

​	JIT的引入改变了性能瓶颈的性质。在传统的解释执行模式下，性能瓶颈往往集中在解释器反复评估表达式所带来的固定开销，这包括大量的间接跳转、难以预测的分支以及完成特定任务所需的指令数量庞大 。这些都是典型的CPU性能瓶颈。JIT通过将这部分固定开销转化为一次性的编译开销（包括机器码的生成和优化）加上后续的、更高效的原生执行开销。这种转变意味着，JIT的真正价值在于其原生执行所带来的性能提升必须能够显著抵消其一次性编译的成本 。因此，JIT并非适用于所有场景的普适性优化，其适用性严格取决于性能收益与编译成本之间的权衡。这种权衡也决定了数据库查询优化器在何时调用JIT的关键决策。如果盲目地对所有查询启用JIT，对于那些执行时间短、简单重复的查询，编译开销可能会超过执行时间节省，反而导致整体性能下降。

### 1.2. PostgreSQL引入JIT的背景与目标

​	PostgreSQL在版本11中正式引入了JIT编译功能，作为其查询执行引擎的一个重要新组件 。在此之前，PostgreSQL已提供了PL/pgSQL函数的提前编译（ahead-of-time compilation）以及在版本10中引入的表达式编译功能，但这些机制并未涉及生成原生机器码 。

​	引入JIT的主要驱动力在于解决当时大型查询中日益凸显的性能瓶颈：表达式评估（Expression Evaluation）和元组变形（Tuple Deforming） 。PostgreSQL核心开发者Andres Freund曾指出，在JIT实现之后，“表达式评估本身比以前快十倍以上” ，这有力地量化了JIT在这一关键领域所带来的巨大性能改进。这一显著的性能提升表明，在JIT引入之前，表达式评估是查询执行中的一个严重瓶颈。

​	JIT的核心目标是加速CPU密集型、长时间运行的复杂查询，特别是应用于在线分析处理（OLAP）和决策支持系统中的分析型查询 。在现代数据库环境中，随着固态硬盘（SSD）、列式存储和分布式数据库等技术的广泛应用，I/O操作已不再是数据库性能的主要瓶颈 。相反，大量的逻辑操作和虚拟函数调用成为制约OLAP查询效率的关键因素 。JIT正是为了应对这种性能瓶颈的转移而设计的，通过减少这些冗余的逻辑操作和虚拟函数调用来提高效率。

​	将JIT的重点放在表达式评估和元组变形上，这种战略性的聚焦表明JIT并非一个通用的性能万灵药，而是一个专门针对特定、已识别的CPU密集型组件的工具。

### 1.3. JIT编译的优势与适用场景

#### 优势

​	JIT编译为数据库查询优化带来了多项显著优势，尤其是在处理计算密集型任务时：	

- 更快的执行速度：

     通过在运行时动态地将部分查询代码编译为机器码，JIT显著降低了传统解释器带来的开销，从而实现了更快的查询执行速度 。

- 计算密集型任务效率提升：

     对于涉及大型数据集和复杂操作（如过滤、排序和聚合）的查询，JIT的优势尤为突出 。它通过生成高度优化的机器码，减少了重复计算和数据处理所需的时间。

- 分析型查询的关键优化：

     JIT在计算效率至关重要的场景中表现出色，例如数据仓库和报告中的分析型查询。它能够显著提高计算大型聚合或对数百万行数据执行复杂过滤条件的性能。 

- 资源利用率提高：

     JIT允许数据库动态优化操作，更有效地利用CPU和内存资源来加速查询执行。

- 函数内联优化：

     PostgreSQL的高度可扩展性允许用户定义新的数据类型、函数和操作符。尽管这提供了极大的灵活性，但也可能引入函数调用开销。JIT编译能够将小型函数的函数体直接内联到使用它们的表达式中，从而显著消除很大一部分函数调用开销，提高代码的局部性和执行效率。

#### 适用场景

​	JIT编译主要适用于长时间运行、CPU密集型的查询 。这些查询通常是分析型查询，涉及大量数据处理和复杂计算。

​	基准测试结果已显示出显著的性能提升。例如，在TPC-H Q1查询中，PostgreSQL 11在开启JIT后比PostgreSQL 10快约29.31% 。其他研究也报告了在TPC-H Q1上高达5倍（与原始解释器相比）甚至8倍（在某些专有数据库上）的加速 。这些数据表明，对于特定类型的分析工作负载，JIT能够带来实质性的性能收益。

#### 不适用场景

​	JIT编译并非万能药，对于执行次数少、短小的查询，其额外开销可能大于带来的执行时间节省，甚至导致整体性能下降 。这是因为JIT编译过程本身会产生固定的成本，包括中间表示（IR）的生成、优化和机器码的发射。例如，一个简单的 `SELECT 42;` 查询，LLVM JIT的耗时远高于无JIT的情况 。



## 2. JIT架构与LLVM集成

### 2.1. PostgreSQL JIT核心原理

​	PostgreSQL JIT的核心原理在于将查询计划中原本由通用解释器处理的CPU密集型操作，例如表达式评估和元组变形，转换为原生机器码，从而显著提高执行效率 。

​	这种转换通过消除通用解释器固有的开销来实现。解释器为了处理任意SQL表达式，通常需要执行大量的间接函数调用、处理难以预测的分支，并且完成特定任务所需的指令数量也相对较多 。这些因素都会导致CPU缓存效率低下和分支预测失误，从而降低性能。JIT通过生成特定于当前查询的机器码，能够将这些间接调用转化为直接分支，甚至在编译时消除某些分支（如果输入是常量），从而大幅减少每项任务所需的指令数量，提高CPU缓存的命中率和分支预测的准确性 。

### 2.2. LLVM作为JIT编译框架

​	PostgreSQL的JIT实现选择LLVM（Low-Level Virtual Machine）作为其主要的编译器框架 。

​	为了启用JIT支持，PostgreSQL在构建时必须使用`--with-llvm`配置选项进行编译 。这确保了PostgreSQL能够链接到LLVM库，并具备在运行时调用LLVM功能的能力。

​	当前 JIT 主要加速**表达式计算**（WHERE 条件、目标列表、聚合、投影等）和**元组解包**（从存储格式到内存字段的转换）。

### 2.3. LLVM IR生成与优化

#### IR生成

​	在PostgreSQL查询计划树的遍历过程中，JIT不再直接执行节点操作，而是生成对应的LLVM Intermediate Representation (IR) 。这一过程涉及将PostgreSQL的内部表达式结构（如`ExprState`）和元组变形逻辑转换为LLVM IR 。

​	为了使JIT编译的代码能够无缝地调用PostgreSQL的现有内部函数，PostgreSQL采取了预编译策略。PostgreSQL的后端函数（例如用于整数加法的`int8pl`等）的C语言源代码会通过Clang编译器预编译为LLVM Bitcode（`.bc`文件）。这些位码文件在JIT初始化时被加载并链接到LLVM模块中，从而允许JIT编译的代码直接调用这些预编译的PostgreSQL内部函数，而无需重新实现其逻辑 。这种方法避免了重新实现PostgreSQL庞大的函数库，极大地简化了JIT的集成复杂性。

#### 优化

​	LLVM提供了强大的代码优化能力。其中一些优化是轻量级的，可以在每次JIT使用时执行，而另一些则更适合长时间运行的查询，因为它们的开销较高 。

​	**函数内联（Function Inlining）：** 这是JIT编译的一项关键优化，它通过将小型函数的函数体直接嵌入到调用它们的表达式中，显著减少了PostgreSQL可扩展函数/操作符机制带来的函数调用开销 。传统解释器模型中，函数调用和间接跳转会引入CPU缓存失效和分支预测错误，从而降低性能 。通过内联，这些开销得以消除，提高了代码的局部性，并使CPU能够更高效地执行指令。

​	**常量折叠（Constant Folding）和稀疏条件常量传播（SCCP）：** 这些是LLVM提供的标准编译器优化技术 。常量折叠在编译时计算并替换常量表达式的值。SCCP是一种更高级的优化，它通过分析程序中的数据流，识别并传播常量值，从而简化代码并消除不必要的计算。

​	**控制流图重构（CFG Restructuring）：** 例如循环展开（Loop Unrolling），这些优化有助于简化和优化在编译时已知其行为的代码路径 。通过减少循环迭代中的分支和开销，可以进一步提高执行效率。

​	**消除间接跳转：** JIT可以将解释器中大量的间接跳转（如通过函数指针或虚函数调用）替换为直接跳转，或者在编译时消除它们（如果输入是常量） 。这对于元组变形等操作尤其有利，因为它可以显著减少CPU周期消耗。



## 3. 主要接口与源码结构

### 3.1. JIT模块的顶层结构(`src/backend/jit`)

​	PostgreSQL的JIT模块代码主要位于`src/backend/jit/`目录下 。这个目录的设计旨在提供一个LLVM无关的抽象层，使得理论上可以替换JIT的后端实现。

- **`jit.c`：** 这是JIT模块的顶层文件，充当LLVM JIT提供者的通用包装器 。它定义了JIT功能的通用接口，并负责加载具体的JIT提供者（如LLVM）。`jit.c`中的关键函数包括：

    - `provider_init()`：此静态函数负责初始化JIT提供者。它检查`jit_enabled` GUC参数是否为真，然后尝试加载由`jit_provider` GUC参数指定的共享库（默认为`llvmjit`）。如果加载成功，它将初始化`JitProviderCallbacks`结构体中的回调函数指针。

        ```c
        struct JitProviderCallbacks
        {
        	JitProviderResetAfterErrorCB reset_after_error;
        	JitProviderReleaseContextCB release_context;
        	JitProviderCompileExprCB compile_expr;
        };
        ```

    - `pg_jit_available()`：一个SQL可调用的函数，用于检查JIT功能是否可用 。

    - `jit_reset_after_error()`：在发生错误后重置JIT上下文的函数 。

    - `jit_release_context(JitContext *context)`：用于释放JIT上下文及其相关资源的函数 。JIT编译的函数生命周期通过`JitContext`管理，通常在查询初始化时创建，并在查询执行结束或事务结束时释放 。

    - `jit_compile_expr(struct ExprState *state)`：这是核心的表达式JIT编译入口点 。它检查是否满足JIT编译的条件（如`jit_enabled`和`PGJIT_EXPR`标志），然后将编译任务委托给已加载的JIT提供者的`compile_expr`回调函数。

- **`jit.h`：** 包含了JIT模块的公共数据结构和宏定义，例如`JitContext`、`JitInstrumentation`以及用于控制JIT行为的标志（`PGJIT_PERFORM`, `PGJIT_EXPR`, `PGJIT_INLINE`, `PGJIT_OPT3`）。

### 3.2. LLVM特定实现 (`src/backend/jit/llvm`)

​	`src/backend/jit/llvm/`目录包含了PostgreSQL JIT与LLVM框架集成的具体实现 。这个子目录中的文件负责将PostgreSQL的内部结构转换为LLVM IR，并利用LLVM的编译和优化能力。

- **`llvmjit.c`：** 这是LLVM JIT提供者的主文件，实现了`JitProviderCallbacks`中定义的回调函数 。它负责LLVM上下文的初始化`LLVMJitContext`、模块的创建、优化和机器码的发射。关键函数包括:
    - `_PG_jit_provider_init()`：LLVM JIT提供者的入口函数，由`jit.c`中的`provider_init()`调用。它设置`JitProviderCallbacks`结构体中的回调函数指针（如 `cb->compile_expr = llvm_compile_expr` 等），指向LLVM特定的实现，例如`llvm_compile_expr`、`llvm_release_context`和`llvm_reset_after_error`。
    - `llvm_session_initialize()`：初始化 LLVM 全局上下文，注册类型映射（如 `TypeCacheEntry` → LLVM 类型）。
    - `llvm_create_context()`：分配并初始化一个 `LLVMJitContext`，设置 JIT 标志、资源所有者等，对应地，`llvm_release_context(JitContext *ctx)` 释放所有 OrcJIT 资源并销毁 LLVM 模块、上下文等内部对象。
    - `llvm_session_initialize()`：初始化LLVM会话，包括创建LLVM上下文和设置目标机器 。
    - `llvm_compile_module(LLVMJitContext *context)`：负责将LLVM模块编译为可执行代码 。
    - `llvm_optimize_module(LLVMJitContext *context, LLVMModuleRef module)`：对生成的LLVM IR进行优化 。
    - `llvm_recreate_llvm_context()`：为了防止内存累积问题（特别是在内联操作后），会定期重新创建LLVM上下文 。
    - `LLVMJitContext` 中使用 LLVM Orc JIT（`LLVMOrcLLJITRef`）来完成将 IR 编译为机器码的工作。核心模块在 `llvm_compile_module`、`llvm_optimize_module` 等内部函数中把构建好的 LLVM 模块提交给 OrcJIT，得到一个 `LLVMJitHandle`（封装了对象文件句柄），并将其添加到 `context->handles` 列表中。在表达式最终准备就绪后，通过 `JitProvider` 回调将函数指针注入 PostgreSQL 执行树。接口 `llvm_get_function(LLVMJitContext*, const char *funcname)` 可从上下文中查找并返回已编译的函数指针，用于在多次执行中复用编译结果。

- **`llvmjit_expr.c`：** 负责将PostgreSQL的`ExprState`结构体（表示表达式树）转换为LLVM IR 。这涉及到遍历表达式树，为每个操作符、函数和常量生成相应的LLVM指令。

    主要函数 `bool llvm_compile_expr(ExprState *state)` 是表达式 JIT 的入口（注册为 `JitProviderCallbacks.compile_expr`）,从 `state->expr` 的表达式树生成对应的 LLVM 代码，并最终将生成的本机函数指针写入 `state->evalfunc`，以替代默认的解释器。其内部使用了辅助函数生成对 PgExecutor 数据结构（如 `ExprContext`、`TupleTableSlot` 等）的 LLVM 类型引用，并构建基本块执行表达式中的操作。编译成功返回 true，否则保持默认行为。该模块还支持将某些内置函数或操作内联进最终代码，以减少调用开销。

    - `BuildV1Call`：为每个表达式节点生成对应的 LLVM 调用。
    - `build_EvalXFuncInt`：递归处理表达式树，生成条件判断、逻辑与或等操作的 IR。

- **`llvmjit_deform.c`：** 负责实现元组变形的JIT编译 。它生成将磁盘上的元组（`on-disk tuple`）转换为内存中表示的优化代码，这对于列数较多或需要频繁访问元组内容的查询非常重要 。

    核心函数 `LLVMValueRef slot_compile_deform(LLVMJitContext *context, TupleDesc desc, const TupleTableSlotOps *ops, int natts)` 根据 `TupleDesc`（表描述）和要解包的列数生成一个 LLVM 函数，用于将 `HeapTuple`（或其他物理/最小元组）转换为`TupleTableSlot`中的值和空值数组。在执行时，如果 `es_jit_flags` 包含 `PGJIT_DEFORM` 标志，则扫描节点会调用此函数生成定制的解包逻辑，从而替换通用的 C 函数，提高数据提取性能。

- **`llvmjit_inline.cpp`：** 包含了LLVM内联优化的相关逻辑 。它利用LLVM的内联能力，将PostgreSQL内部的小型函数（如操作符实现）的IR直接嵌入到调用它们的表达式IR中，从而消除函数调用开销 。

    该模块维护一个 LLVM 模块缓存，通过读取预先编译好的位码文件（存放在 `$pkglibdir/bitcode/` 目录下）将 C 函数的定义加载为 LLVM IR。见 `load_module` 函数示例，它根据函数名从 `pkglib_path/bitcode/*.bc` 读取位码，解析为 LLVM 模块，以便在生成表达式代码时直接将函数体内联。使用内联后，可显著减少函数调用开销，并发挥 LLVM 优化器作用。

- **`llvmjit_types.c`：** 这个文件包含了PostgreSQL内部类型与LLVM类型系统之间的映射 。它通过引用PostgreSQL所需的各种类型，在编译时将其转换为位码，并在LLVM初始化时加载，以确保类型定义在JIT编译过程中同步。这解决了手动在C代码中重新创建类型定义可能带来的错误和工作量问题 。

    生成包含 PostgreSQL 数据结构（如 `HeapTupleHeaderData`、`TupleTableSlot` 等）的 LLVM 位码文件，供内联模块使用。该部分通常作为独立工具编译产生 `.bc` 文件，并由内联模块动态加载。通过预定义这些类型，JIT 代码可正确访问 PostgreSQL 内部结构。

- **`llvmjit_error.cpp`：** 包含了LLVM相关的错误处理逻辑 。

- **`SectionMemoryManager.cpp`：** 负责管理JIT编译代码的内存分配，确保生成的机器码能够被正确地加载和执行 。

​	这些模块协同工作：查询执行时由核心模块管理编译过程，表达式模块和解包模块负责具体代码生成，内联模块提供额外的函数定义，类型模块确保数据结构一致性。所有生成的对象通过 OrcJIT 添加到 `LLVMJitContext->handles` 列表中，待后续调用 `LLVMOrcLLJIT` 执行**即时编译**并获得函数指针。每次执行完毕或出现错误后，会通过 `llvm_release_context` 和 `llvm_reset_after_error` 清理相关资源。

### 3.3. 关键数据结构 ( `LLVMJitContext`,`ExprState`)

- `LLVMJitContext`:

    核心数据结构，定义在 `include/jit/llvmjit.h`。它扩展了通用 `JitContext`，主要字段包括资源所有者 `ResourceOwner resowner`、当前模块编号 `size_t module_generation`、LLVM 环境句柄 `LLVMContextRef llvm_context` 和待生成代码的模块 `LLVMModuleRef module`，以及编译标志 `bool compiled`、名称计数器 `int counter`、和已生成代码对象句柄列表 `List *handles`。`JitContext base` 包含通用字段（如 JIT 选项 flags），整个结构用于在一次查询执行中累积生成 LLVM IR 并管理其生命周期。

- `ExprState`：

    - `ExprState`是PostgreSQL执行器中用于表示表达式树及其运行时状态的数据结构 。

    - 在JIT编译流程中，`ExprState`是JIT编译器的输入 。JIT编译器会遍历`ExprState`树，将其中的操作符、函数调用、常量和变量引用等转换为LLVM IR 。

    - `jit_compile_expr(struct ExprState *state)`函数接收一个`ExprState`指针作为参数，并根据其内部的逻辑结构生成对应的JIT编译代码 。

    - JIT的目标是将`ExprState`中通用、解释性的评估逻辑替换为高度优化的原生机器码，从而加速表达式的求值过程 。

        

## 4. 流程调用

### 4.1. JIT编译的触发条件与配置

​	PostgreSQL JIT编译的触发和行为由一系列GUC（Grand Unified Configuration）参数控制 。这些参数允许数据库管理员根据工作负载特性进行细粒度调整。

- **`jit`：** 这个参数用于全局启用或禁用JIT编译。在PostgreSQL 11中，JIT默认是禁用的，但在后续版本中通常默认启用 。如果此参数设置为`off`，即使其他成本条件满足，JIT也不会被执行 。
- **`jit_above_cost`：** 这是一个成本阈值参数。只有当查询优化器估算的查询总成本超过此值时，JIT编译才会被考虑执行 。默认值通常设置得较高（例如100000），以避免对短查询引入不必要的编译开销 。
- **`jit_inline_above_cost`：** 当查询的估算成本超过此阈值时，JIT编译器会尝试将查询中使用的短函数和操作符进行内联 。内联有助于消除函数调用开销，进一步优化性能。
- **`jit_optimize_above_cost`：** 如果查询的估算成本超过此阈值，JIT编译器将应用更昂贵的优化策略来改进生成的代码 。这些优化可能需要更多的编译时间，但能为长时间运行的复杂查询带来更大的性能收益。
- **`jit_provider`：** 此参数指定了用于JIT编译的共享库。默认值为`llvmjit`，表示使用LLVM作为JIT提供者 。虽然理论上可以指定其他JIT提供者，但目前内联支持仅在LLVM提供者下可用 。

​	这些成本相关的参数在初始时被有意设置为较高的值，以防止在启用JIT时出现意外的负面性能影响 。将JIT成本参数设置为`0`将强制所有查询都进行JIT编译，这反而可能导致所有查询变慢，因为即使是简单查询也需要承担编译开销 。

### 4.2. JIT在查询处理流程中的集成

​	PostgreSQL的查询处理通常分为解析（Parsing）、规划（Planning）和执行（Execution）三个主要阶段 。JIT编译主要集成在规划和执行阶段。

#### 4.2.1. 规划器集成 (Planner Integration)

​	JIT编译的决策是在查询的规划阶段做出的 。在规划器生成查询计划时，它会估算查询的总成本 。

​	**成本估算与JIT决策：** 规划器会比较查询的估算成本与`jit_above_cost`参数的值 。如果估算成本高于此阈值，规划器将决定对该查询启用JIT编译。

​	**内联与优化决策：** 如果查询的估算成本进一步超过`jit_inline_above_cost`和`jit_optimize_above_cost`的阈值，规划器还会指示JIT编译器执行相应的内联和更昂贵的优化 。

​	**预处理语句的影响：** 对于预处理语句（Prepared Statements），JIT编译的决策是在第一次执行时做出的，并且如果使用了通用计划（Generic Plan），那么在准备时生效的配置参数将控制这些决策，而不是在后续执行时生效的参数 。这意味着，即使在后续会话中更改了JIT相关的GUC参数，预处理语句的行为也不会改变，除非重新准备。	

​	需要注意的是，JIT的成本估算目前没有一个明确的“理想值”确定方法，它们最好被视为特定安装中所有查询混合的平均值 。因此，基于少量实验来更改这些参数具有较高的风险 。

```c
result->jitFlags = PGJIT_NONE;
if (jit_enabled && top_plan->total_cost > jit_above_cost) {
    result->jitFlags |= PGJIT_PERFORM;
    if (jit_optimize_above_cost >= 0 && top_plan->total_cost > jit_optimize_above_cost)
        result->jitFlags |= PGJIT_OPT3;
    if (jit_inline_above_cost >= 0 && top_plan->total_cost > jit_inline_above_cost)
        result->jitFlags |= PGJIT_INLINE;
    if (jit_expressions)
        result->jitFlags |= PGJIT_EXPR;
    if (jit_tuple_deforming)
        result->jitFlags |= PGJIT_DEFORM;
}
```

#### 4.2.2. 执行器集成 (Executor Integration)

​	JIT编译主要在查询的执行阶段发挥作用，特别是针对那些被规划器标记为需要JIT优化的操作 。

​	**查询执行启动**：在 `ExecutorStart` 中，将 `plannedstmt->jitFlags` 复制到执行状态 `EState.es_jit_flags`。之后，`InitPlan` 构建计划树并调用各节点的 `ExecInit*` 函数。在节点初始化过程中，例如在初始化表达式（`ExecInitExpr`）或扫描节点时，会检查 `es_jit_flags` 中的标志。如果对应的 JIT 功能被启用，就触发编译流程。

​	**编译表达式**：以表达式为例，当节点需要评估一个表达式（如WHERE条件或目标列表），执行器调用 `ExecReadyExpr(ExprState *state)`。此时，若 JIT 表示支持且尚未编译，`ExecReadyExpr` 会调用我们注册的 `llvm_compile_expr(state)`。编译过程将生成一个定制的 LLVM 函数，最终取到本机代码地址并设置 `state->evalfunc = <编译后函数指针>`。后续在执行时，`ExecEvalExpr(state, econtext)` 将直接调用这一函数，绕过解释器逻辑。

​	**编译元组解包**：在表扫描或索引扫描过程中，每次从磁盘读出一个 `HeapTuple` 并填充到 `TupleTableSlot` 时，通常需要将行中各列的值提取出来。若 `PGJIT_DEFORM` 标志已设置，执行器会调用内部逻辑使用 `slot_compile_deform` 为该表当前列数生成一个解包函数，并将其编译为机器码；后续处理每行时直接调用这个专用函数进行解包。这样避免了通用版元组解包函数中的循环和检查开销。

​	**LLVM IR生成：** JIT模块（通过其LLVM提供者）接收`ExprState`，并遍历其内部结构，将其转换为LLVM Intermediate Representation (IR) 。这个过程涉及将PostgreSQL的内部操作映射到LLVM的指令集。同时，JIT编译的代码可以调用预编译的PostgreSQL后端函数，这些函数以LLVM Bitcode形式存在，并被链接到JIT模块中 。  

​	**LLVM优化与机器码发射：** 生成的LLVM IR会经过LLVM的优化阶段，根据规划器传递的JIT标志（如是否内联、是否执行昂贵优化）进行处理 。优化后的IR随后被LLVM的执行引擎（如ORC JIT）编译成特定CPU架构的原生机器码，并加载到内存中 。

​	**函数指针替换与执行：** JIT编译完成后，PostgreSQL执行器会将原先指向解释器函数的函数指针替换为指向新生成的原生机器码函数 。此后，当需要评估该表达式或执行元组变形时，将直接调用JIT编译后的原生函数，从而避免了通用解释器的开销 。

​	**运行与清理**：一旦本机代码生成完成并注入执行环境，查询就以与平常相同的方式进行执行，只是执行速度更快。查询结束后或遇到错误时，`llvm_release_context` 和 `llvm_reset_after_error` 会被调用清理所有 JIT 相关资源（如 LLVM 模块、Orc 对象等），确保后续查询正常运行。

​	`EXPLAIN ANALYZE`命令的输出会详细显示JIT编译的各个阶段所花费的时间，包括生成（Generation）、内联（Inlining）、优化（Optimization）和发射（Emission）等 。这对于诊断和调优JIT性能非常有用。

