# PostgreSQL 17 JIT 体系结构与设计

PostgreSQL 内置了 LLVM 后端的 JIT 编译支持（需使用 `--with-llvm` 编译开启），可以在运行时将部分解释执行的操作编译为本机代码。当前 JIT 主要加速**表达式计算**（WHERE 条件、目标列表、聚合、投影等）和**元组解包**（从存储格式到内存字段的转换）

整体架构如下：PostgreSQL 在 **规划阶段** 根据代价和 GUC 设置（`jit_enabled`、`jit_above_cost`、`jit_expressions`、`jit_tuple_deforming`、`jit_inline`、`jit_optimize` 等）决定是否启用 JIT，并将标志位存入生成的 `PlannedStmt.jitFlags`。

在 **执行阶段**，`ExecutorStart` 将这些标志复制到 `EState.es_jit_flags`。各执行节点（如序列扫描、索引扫描等）在初始化表达式状态时，如果检测到对应的 JIT 标志，则调用 JIT 编译接口生成本机函数并替换默认的解释器。例如，`ExecReadyExpr` 会调用 `llvm_compile_expr(state)`；而在扫描堆表时，会使用 `llvmjit_deform.c` 中的函数生成针对特定表行格式的解包代码。

 

PostgreSQL JIT 基于 LLVM 实现，架构分为以下层次：

1. **前端接口层**：与执行器交互，触发 JIT 编译（`expr_jit.c`）。
2. **LLVM 抽象层**：封装 LLVM API，提供内存管理与线程安全（`llvmjit.c`）。
3. **代码生成层**：将表达式/节点转换为 LLVM IR（`jit_expr.c`）。
4. **优化层**：应用 LLVM 优化 Pass（`llvmjit_optimize.c`）。
5. **执行层**：加载编译后的机器码并执行。



PostgreSQL 的 JIT 功能划分为多个子模块，均位于 `src/backend/jit/llvm` 目录下：

- **核心上下文模块 (`llvmjit.c`)：** 定义了 JIT 编译上下文 `LLVMJitContext`（包含 LLVM 上下文、当前模块、资源管理等）和会话管理逻辑。该模块负责创建/销毁 JIT 上下文、管理 LLVM 运行时（含 OrcJIT 实例）和生成的代码句柄列表。核心函数如 `llvm_create_context(int jitFlags)` 会分配并初始化一个 `LLVMJitContext`，设置 JIT 标志、资源所有者等。`_PG_jit_provider_init(JitProviderCallbacks *cb)` 在模块加载时被调用，用于注册 JIT 回调（如 `cb->compile_expr = llvm_compile_expr` 等）。该模块还配置了两套 LLVM Orc JIT 实例（针对普通和高级优化）等。
- **表达式编译模块 (`llvmjit_expr.c`)：** 实现表达式的 LLVM IR 生成和编译。主要函数 `bool llvm_compile_expr(ExprState *state)` 从 `state->expr` 的表达式树生成对应的 LLVM 代码，并最终将生成的本机函数指针写入 `state->evalfunc`，以替代默认的解释器。其内部使用了辅助函数生成对 PgExecutor 数据结构（如 `ExprContext`、`TupleTableSlot` 等）的 LLVM 类型引用，并构建基本块执行表达式中的操作。编译成功返回 true，否则保持默认行为。该模块还支持将某些内置函数或操作内联进最终代码，以减少调用开销。
- **元组解包模块 (`llvmjit_deform.c`)：** 实现针对特定表行格式和列数的解包代码生成。核心函数 `LLVMValueRef slot_compile_deform(LLVMJitContext *context, TupleDesc desc, const TupleTableSlotOps *ops, int natts)` 根据 `TupleDesc`（表描述）和要解包的列数生成一个 LLVM 函数，用于将 `HeapTuple`（或其他物理/最小元组）转换为`TupleTableSlot`中的值和空值数组。在执行时，如果 `es_jit_flags` 包含 `PGJIT_DEFORM` 标志，则扫描节点会调用此函数生成定制的解包逻辑，从而替换通用的 C 函数，提高数据提取性能。
- **跨模块内联模块 (`llvmjit_inline.cpp`)：** 支持在 JIT 生成过程中内联 PostgreSQL 内部函数或运算符。该模块维护一个 LLVM 模块缓存，通过读取预先编译好的位码文件（存放在 `$pkglibdir/bitcode/` 目录下）将 C 函数的定义加载为 LLVM IR。见 `load_module` 函数示例，它根据函数名从 `pkglib_path/bitcode/*.bc` 读取位码，解析为 LLVM 模块，以便在生成表达式代码时直接将函数体内联。使用内联后，可显著减少函数调用开销，并发挥 LLVM 优化器作用。
- **类型定义模块 (`llvmjit_types.c`)：** 生成包含 PostgreSQL 数据结构（如 `HeapTupleHeaderData`、`TupleTableSlot` 等）的 LLVM 位码文件，供内联模块使用。该部分通常作为独立工具编译产生 `.bc` 文件，并由内联模块动态加载。通过预定义这些类型，JIT 代码可正确访问 PostgreSQL 内部结构。
- **辅助模块：** `llvmjit_wrap.cpp` 提供 C API 缺失的低级操作封装；`llvmjit_error.cpp` 处理 JIT 编译过程中的错误与 OOM；`llvmjit_backport.h` 等用于兼容不同 LLVM 版本；`SectionMemoryManager.cpp` 实现自定义内存管理（如需要在 LLVM 运行时加载对象）等。

这些模块协同工作：查询执行时由核心模块管理编译过程，表达式模块和解包模块负责具体代码生成，内联模块提供额外的函数定义，类型模块确保数据结构一致性。所有生成的对象通过 OrcJIT 添加到 `LLVMJitContext->handles` 列表中，待后续调用 `LLVMOrcLLJIT` 执行**即时编译**并获得函数指针。每次执行完毕或出现错误后，会通过 `llvm_release_context` 和 `llvm_reset_after_error` 清理相关资源。

## 接口函数与关键数据结构

- **JIT 上下文 (`LLVMJitContext`)**：这是核心数据结构，定义在 `include/jit/llvmjit.h`。它扩展了通用 `JitContext`，主要字段包括资源所有者 `ResourceOwner resowner`、当前模块编号 `size_t module_generation`、LLVM 环境句柄 `LLVMContextRef llvm_context` 和待生成代码的模块 `LLVMModuleRef module`，以及编译标志 `bool compiled`、名称计数器 `int counter`、和已生成代码对象句柄列表 `List *handles`。`JitContext base` 包含通用字段（如 JIT 选项 flags），整个结构用于在一次查询执行中累积生成 LLVM IR 并管理其生命周期。

    ```
    typedef struct LLVMJitContext {
        MemoryContext mctx;       // 所属内存上下文
        LLVMModuleRef module;     // LLVM Module
        List       *handles;       // 已编译函数句柄列表
        bool       opt_enabled;    // 是否启用优化
        int        opt_level;      // 优化级别 (0-3)
    } LLVMJitContext;
    ```

- **初始化与销毁**：`LLVMJitContext *llvm_create_context(int jitFlags)`（在 `llvmjit.c` 中）负责分配和初始化 JIT 上下文，将 GUC 标志赋给 `base.flags`，并注册到当前资源所有者，以便在事务结束时自动清理。对应地，`llvm_release_context(JitContext *ctx)` 释放所有 OrcJIT 资源并销毁 LLVM 模块、上下文等内部对象。

    

- **表达式编译接口**：`bool llvm_compile_expr(ExprState *state)` 是表达式 JIT 的入口（注册为 `JitProviderCallbacks.compile_expr`）。`ExprState` 包含待编译的表达式树和执行上下文信息。调用该接口时，JIT 会构造一段 LLVM 函数，遍历表达式 `state->expr` 的运算步骤，生成对应的 IR，并将生成的本机函数指针保存到 `state->evalfunc`。成功编译则返回 `true`；若因条件不满足或失败，则返回 `false`，执行回退到解释执行。
    相似地，虽然没有暴露的 `llvm_compile_deform` 接口，但在解包过程中内部调用了 `slot_compile_deform(LLVMJitContext*, TupleDesc, TupleTableSlotOps*, int)` 来生成解包函数​。

    ```
    typedef struct ExprState {
        // ... 其他字段
        JitExecFunction jit_func;  // JIT 编译后的函数指针
        bool        jit_compiled;  // 是否已编译
    } ExprState;
    ```

- **内存管理与调用**：`LLVMJitContext` 中使用 LLVM Orc JIT（`LLVMOrcLLJITRef`）来完成将 IR 编译为机器码的工作。核心模块在 `llvm_compile_module`、`llvm_optimize_module` 等内部函数中把构建好的 LLVM 模块提交给 OrcJIT，得到一个 `LLVMJitHandle`（封装了对象文件句柄），并将其添加到 `context->handles` 列表中。在表达式最终准备就绪后，通过 `JitProvider` 回调将函数指针注入 PostgreSQL 执行树。接口 `llvm_get_function(LLVMJitContext*, const char *funcname)` 可从上下文中查找并返回已编译的函数指针，用于在多次执行中复用编译结果。

    

- **回调注册**：在模块初始化时，函数 `_PG_jit_provider_init(JitProviderCallbacks *cb)` 将上面提到的函数（如 `llvm_compile_expr`）注册到 PostgreSQL 的 JIT 回调结构中。这样，在执行器的标准流程（如 `ExecReadyExpr`）中检测到 JIT 启用标志时，会自动调用这些回调函数完成编译、链接、替换步骤。

## 调用流程

1. **查询规划（Planner）**：在 `standard_planner` 函数中，会根据成本阈值和 GUC（`jit_enabled` 等）设置 `PlannedStmt->jitFlags`。具体代码如下（来自 PostgreSQL 源码示例）：

    ```
    c
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

    即若启用了 JIT 且估计成本高于阈值，就设置 `PGJIT_PERFORM`，并根据其他阈值依次设置 `PGJIT_OPT3`、`PGJIT_INLINE`，然后根据 GUC 决定是否启用表达式和元组解包加速。

2. **查询执行启动**：在 `ExecutorStart` 中，将 `plannedstmt->jitFlags` 复制到执行状态 `EState.es_jit_flags`。之后，`InitPlan` 构建计划树并调用各节点的 `ExecInit*` 函数。在节点初始化过程中，例如在初始化表达式（`ExecInitExpr`）或扫描节点时，会检查 `es_jit_flags` 中的标志。如果对应的 JIT 功能被启用，就触发编译流程。

3. **编译表达式**：以表达式为例，当节点需要评估一个表达式（如WHERE条件或目标列表），执行器调用 `ExecReadyExpr(ExprState *state)`。此时，若 JIT 表示支持且尚未编译，`ExecReadyExpr` 会调用我们注册的 `llvm_compile_expr(state)`。编译过程将生成一个定制的 LLVM 函数，最终取到本机代码地址并设置 `state->evalfunc = <编译后函数指针>`。后续在执行时，`ExecEvalExpr(state, econtext)` 将直接调用这一函数，绕过解释器逻辑。

4. **编译元组解包**：在表扫描或索引扫描过程中，每次从磁盘读出一个 `HeapTuple` 并填充到 `TupleTableSlot` 时，通常需要将行中各列的值提取出来。若 `PGJIT_DEFORM` 标志已设置，执行器会调用内部逻辑使用 `slot_compile_deform` 为该表当前列数生成一个解包函数，并将其编译为机器码；后续处理每行时直接调用这个专用函数进行解包。这样避免了通用版元组解包函数中的循环和检查开销。

5. **运行与清理**：一旦本机代码生成完成并注入执行环境，查询就以与平常相同的方式进行执行，只是执行速度更快。查询结束后或遇到错误时，`llvm_release_context` 和 `llvm_reset_after_error` 会被调用清理所有 JIT 相关资源（如 LLVM 模块、Orc 对象等），确保后续查询正常运行。



### JIT 初始化

**函数**：`llvm_session_initialize()`

- 初始化 LLVM 全局上下文。
- 注册类型映射（如 `TypeCacheEntry` → LLVM 类型）。

### 表达式编译

**入口函数**：`jit_expr(ExprState *state, LLVMJitContext *context)`

#### 流程：

1. **生成 IR**：

    ```
    LLVMValueRef llvm_build_expr(ExprState *expr, LLVMBuilderRef builder) {
        switch (expr->op) {
            case OP_FUNC: return build_func_call(expr, builder);
            case OP_CONST: return build_const(expr, builder);
            // ... 其他操作符
        }
    }
    ```

2. **优化 IR**：

    ```
    void llvm_optimize_module(LLVMModuleRef module, int opt_level) {
        LLVMPassManagerRef pass = LLVMCreateFunctionPassManagerForModule(module);
        LLVMAddInstructionCombiningPass(pass);
        if (opt_level > 2) LLVMAddSLPVectorizePass(pass);
        // ... 应用其他 Pass
    }
    ```

3. **生成机器码**：

    ```
    void *llvm_compile_module(LLVMJitContext *context) {
        LLVMExecutionEngineRef engine;
        LLVMCreateJITCompilerForModule(&engine, context->module, 0);
        return LLVMGetPointerToGlobal(engine, func);
    }
    ```

### 4.3 执行流程

```
Datum ExecEvalExpr(ExprState *state, ...) {
    if (state->jit_compiled) {
        return state->jit_func(...);
    } else {
        return ExecInterpExpr(state, ...);
    }
}
```



## 扩展 JIT 功能

要新增一种 JIT 优化功能（如为新表达式类型或新节点编写加速逻辑），通常需要：

1. **更新标志与参数**：如果需要新的 GUC 控制开关，应在 `src/include/jit/jit.h` 中定义新的 `PGJIT_` 标志位，并在 `standard_planner` 中根据阈值或配置设置 `PlannedStmt->jitFlags`。如果仅依赖现有的表达式或解包框架，可复用 `PGJIT_EXPR` 或 `PGJIT_DEFORM` 等标志。
2. **编写代码生成模块**：在 `src/backend/jit/llvm` 下创建或修改模块。例如，要为新类型表达式加速，可在 `llvmjit_expr.c` 中添加对应处理逻辑或辅助函数。要为新的执行节点加速，可新增文件（如 `llvmjit_sort.c`）并编写生成 LLVM IR 的代码。必要时，可在 `include/jit/llvmjit.h` 中声明新的函数接口。所有新生成的 IR 应遵循现有上下文模式：使用 `LLVMJitContext *context` 为当前查询生成函数，并通过 OrcJIT 添加到执行环境。
3. **注册回调与调用**：如果是新的表达式编译类型，可继续利用已有的 `compile_expr` 回调；否则，可能需要在执行器中适当位置插入调用新编译函数的代码。例如，在相应的 `ExecInit*` 或 `Exec*Node` 函数中，当检测到新标志时调用自定义的 `llvm_compile_*` 接口。若增加全局作用（如查询计划级 JIT），也可以在 `_PG_jit_provider_init` 中添加新的回调项。
4. **新增数据结构**：若新优化需要额外状态，可以扩展 `LLVMJitContext` 或创建新的结构体保存。例如，为 JIT 加速的节点维护特定缓存时，可把指向这些缓存的指针添加到上下文中，并在 `llvm_create_context`/`llvm_release_context` 处理它们。也可能需要为 LLVM 生成定义新的 LLVMType（在 `llvmjit_types.c` 中添加）以支持新数据类型。
5. **编译注册流程**：新生成的 LLVM 模块在代码生成完成后需调用 OrcJIT API（如 `LLVMOrcLLJITAddLLVMIRModule` 或相应高层封装函数）来注册本机代码，获得 `LLVMJitHandle` 并保存到 `context->handles`。这一步通常由核心模块的 IR emission 逻辑自动完成。新功能应使用与现有表达式相同的编译流程：先构建 IR 模块，再调用 OrcJIT，最后更新函数指针或执行状态，以让查询继续。

通过上述方法，可以将新的 JIT 优化集成到现有框架中：复用已有的上下文管理与资源清理逻辑，扩展代码生成路径和编译回调，并适当修改计划生成和执行路径，以在合适时机触发新优化。由于 PostgreSQL 提供了模块化的 JIT API（`JitProviderCallbacks`、`JitContext` 等），开发者可以按需向 `llvmjit` 子系统添加新的编译器前端和后端代码，而不必改动核心查询引擎的其余部分。

 

