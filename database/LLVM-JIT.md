## LLVM JIT 核心概念

### LLVM 中间表示 (IR)

​	LLVM IR 是 LLVM 编译器的核心，它是一种低级、目标无关的虚拟指令集，作为前端和后端之间的通用桥梁 。它是一种强类型 (strongly typed) 的中间表示，这意味着在 IR 验证阶段就能捕获类型错误 。

### 基本结构：模块、函数、基本块与指令

**Module (模块):** 是 LLVM IR 的最高层容器，代表一个编译单元。它包含了函数、全局变量、类型声明和外部声明等所有 IR 对象，并存储了目标特性信息。模块是 LLVM 中进行编译、分析和优化的最小独立单元 。

**Function (函数):** 是 `BasicBlock` 的容器，大致对应于 C 语言中的函数。一个函数包含其参数列表和一系列基本块 。

**BasicBlock (基本块):** 是函数内部的一系列 LLVM IR 指令的容器。这些指令按顺序执行，直到遇到终止指令（如 `return` 或 `jump`）。在编译为机器码时，基本块大致对应于原生汇编输出中的标签 。每个基本块都必须以一个终止指令结束，并且函数中的最后一个基本块必须以 `ret` 指令终止 。

**Instruction (指令):** 是 LLVM IR 的基本操作单元，由操作码和操作数向量组成。指令可以是编号的（例如 `%0`, `%1`）或被赋予显式变量名（例如 `%a`, `%foo`）。

**LLVM IR 示例：**

```c
declare i32 @putchar(i32) ; 外部函数声明
define i32 @add(i32 %a, i32 %b) { ; 函数定义
  %1 = add i32 %a, %b
  ret i32 %1
}
```



## JIT 编译流程概览

LLVM JIT 编译通常遵循一个动态的、按需执行的流程：

- **解析 (Parsing):** 外部源代码（例如，一个动态语言的表达式）或更高级的中间表示被前端解析，并转换为 LLVM 特定的中间表示 (LLVM IR) 。
- **优化 (Optimization):** 生成的 LLVM IR 模块会经过一系列的优化 Pass。这些 Pass 旨在提高性能、减少代码大小并消除冗余操作。优化级别可以根据具体用例进行配置，甚至可以在运行时根据性能反馈进行调整 。
- **JIT 编译 (JIT Compilation):** 当程序执行到需要动态编译的代码点时（例如，第一次调用一个函数），JIT 编译器会将相应的 LLVM IR 动态编译成目标平台的原生机器码。这种编译可以按函数粒度进行，甚至更小的代码单元 。
- **执行 (Execution):** 编译好的机器码立即被加载到内存中并执行，为正在运行的程序提供所需的功能。后续对同一代码的调用可以直接执行已编译的机器码，避免重复编译 。



## LLVM JIT 架构与工作原理

### ORC (On-Request Compilation) 框架

​	ORC (On-Request Compilation) 是 LLVM JIT API 的第三代，它取代了早期版本如 Legacy JIT 和 MCJIT，成为现代 LLVM JIT 的首选框架 。ORC 的设计哲学是提供一个高度灵活、模块化且可扩展的 JIT 基础设施，其核心思想是“按需编译”：即在程序执行后才动态生成机器码 。ORC 的架构演进是一个重要的里程碑。它旨在更精确地模拟静态和动态链接器的行为，包括符号解析和链接规则，从而能够更好地管理 JIT 编译代码与现有静态/动态库之间的复杂交互 。这种转变不仅仅是 API 的更新，它代表着 JIT 架构向更健壮、灵活和可扩展方向的范式转变。通过精心模仿传统链接器行为，ORC 简化了 JIT 编译代码与涉及现有静态和动态库的复杂依赖图的集成。这种能力对于构建需要与更广泛的系统环境无缝交互的大型生产级 JIT 应用程序至关重要。

#### 核心组件：ExecutionSession、JITDylib、MaterializationUnit、LLJIT

ORC 框架由几个核心组件构成，它们协同工作以实现按需编译和执行：

- **ExecutionSession (执行会话):** 

    ​	`ExecutionSession` 是 ORC JIT API 的核心，代表整个 JIT 编译的程序，并提供 JIT 的整体上下文 。它充当 JIT 的中央枢纽，负责管理 `JITDylib` 实例、处理错误报告机制，并调度 `Materializer`（具体化器）来编译和链接代码 。`ExecutionSession` 还管理用于唯一化符号字符串的字符串池，并处理 JIT 环境中的同步和符号查找操作 。所有 ORCv2 JIT 栈都需要一个 `ExecutionSession` 实例作为其基础 。

- **JITDylib (JIT 动态库):** 

    ​	`llvm::orc::JITDylib` 类表示一个 JIT 编译的动态库。其设计旨在模仿常规动态链接库或共享对象的行为，但其包含的代码表示无需在程序启动时预先编译 。`JITDylib` 的内容通过添加 `MaterializationUnits` 来定义，并且它依赖于自身的“链接到”顺序来解析外部引用，类似于传统动态库的链接方式 。它是一个轻量级包装器，实际引用的是 `ExecutionSession` 中持有的状态，因此其内存地址是稳定的，不能被移动或复制 。`JITDylib` 支持添加定义生成器 (`addGenerator`)，当符号在正常查找路径中未找到时，这些生成器会运行以尝试动态生成缺失的定义 。它提供了设置和修改链接顺序 (`setLinkOrder`, `addToLinkOrder`) 的功能，这对于管理 JIT 模块之间的依赖关系至关重要 。  

    ​	`JITDylib` 作为模块化和惰性加载的单元，其作用不仅仅是组织结构。它能够实现对代码加载和依赖的细粒度控制。通过将 JIT 编译的代码组织成 `JITDylib`，开发人员可以像管理传统共享库一样管理不同的代码单元、它们之间的相互依赖关系以及它们的生命周期（加载、卸载、符号解析）。其中，“惰性编译”方面对于优化启动时间和整体资源利用率尤为关键，因为代码仅在实际需要时才被编译和加载。因此，`JITDylib` 对于构建复杂的 JIT 应用程序至关重要，在这些应用程序中，代码的不同部分可能在不同时间生成或加载，或者需要无缝集成外部原生库。

- **MaterializationUnit (具体化单元):** 

    ​	`MaterializationUnit` 是一个抽象类，它代表一组符号定义，这些定义可以作为一个整体被“具体化”（即编译和链接为可执行代码），或者在遇到覆盖定义时被单独“丢弃” 。它们用于向 `JITDylib` 提供符号的惰性定义。当通过 `JITDylib` 的 `lookup` 方法请求某个符号的地址时，`JITDylib` 会调用相应的 `MaterializationUnit` 的 `materialize()` 方法来触发编译和链接过程 。`IRMaterializationUnit` 是一个方便的基类，专门用于包装 LLVM IR 模块的 `MaterializationUnit` 。  

    ​	具体化作为按需编译的引擎，其机制是 ORC 效率和响应性的核心。它确保计算密集型的编译和链接工作只在代码实际需要执行时才发生，从而最大限度地减少初始开销并提高响应速度。`discard()` 机制通过允许 JIT 有效地丢弃不再需要的定义（例如，由于更强的覆盖定义），进一步优化了资源使用。这种惰性具体化模型对于 ORC 的效率和响应能力至关重要。它实现了快速应用程序启动时间和高效的资源利用，因为只有实际执行的代码路径才会被编译并加载到内存中。这使得 LLVM JIT 特别适用于交互式环境、动态语言运行时或代码库非常庞大但任何给定时间只有一部分处于活动状态的应用程序。

- **LLJIT (简化的 JIT 栈):** 

    ​	`llvm::orc::LLJIT` 是一个预先配置好的 ORC JIT 栈，它作为 `MCJIT` 的现代替代品，旨在大大简化 JIT 编译环境的设置和管理过程 。`LLJIT` 的实例通常通过 `LLJITBuilder` 来创建，这提供了一种便捷的方式来配置 JIT 引擎 。每个 `LLJIT` 实例都内含一个 `ExecutionSession` 实例，负责整体的 JIT 管理 。`LLJIT` 提供了便捷的 API 来管理 `JITDylib`，例如 `getMainJITDylib()` 用于获取代表主程序的 JITDylib，通常用户会将自己的 IR 模块和对象文件添加到这里 。它还提供了 `addIRModule()` 和 `addObjectFile()` 等函数，用于将 LLVM IR 模块和编译后的对象文件添加到指定的 `JITDylib` 中 。`LLJIT` 内部使用 `IRCompileLayer` 和 `RTDyldObjectLinkingLayer` 来支持 LLVM IR 的编译和可重定位对象文件的链接 。  

    ​	`LLJIT` 作为 ORC 复杂性的用户友好抽象，其作用在于显著降低了使用 LLVM JIT 的门槛。虽然 ORC 提供了强大的功能和灵活性，但从头开始设置它可能令人生畏。`LLJIT` 提供了一个合理的默认配置和简化的 API，使得开发人员更容易开始 JIT 编译，而无需掌握底层 ORC 架构的每一个复杂细节。这有效地鼓励了更广泛的开发人员采用 LLVM JIT，即使他们没有深厚的编译器专业知识。它允许开发人员主要专注于为其动态代码生成正确的 LLVM IR，而 `LLJIT` 则处理编译、链接和执行的复杂编排，从而加速开发和原型设计。



## 使用实例

**1. 加法函数C代码 (`add_function.c`):** 一个简单的C文件，包含我们要优化的加法函数。

**2. 生成LLVM Bitcode (`add_function.bc`):** 使用Clang（LLVM前端）将C代码编译成LLVM Bitcode。

**3. JIT执行C代码 (`orc_jit_executor.cpp`):** 另一个CPP文件，它将：

- 初始化LLVM。
- 加载`.bc`文件。
- 使用LLVM的JIT引擎编译并执行加法函数。
- 测量执行时间以展示潜在的性能优势。

### 步骤1：创建加法函数C代码 (`add_function.c`)

```c
#include <stdio.h>

// 这是一个简单的加法函数，我们希望通过LLVM JIT来优化和执行它。
long long add_numbers(long long a, long long b) {
    return a + b;
}

int main() {
    long long result = add_numbers(100, 200);
    printf("Result of add_numbers(100, 200): %lld\n", result);
    return 0;
}
```

### 步骤2：生成LLVM Bitcode (`add_function.bc`)

使用Clang编译器将`add_function.c`编译成LLVM Bitcode。

在终端中执行:

```shell
clang -O3 -emit-llvm -c add_function.c -o add_function.bc
```

`-O3`: 启用O3级别优化，这在LLVM JIT中同样会生效。

`-emit-llvm`: 告诉Clang生成LLVM IR。

`-c`: 只编译不链接，生成目标文件。

`-o add_function.bc`: 指定输出文件名为`add_function.bc`。

### 步骤3：使用OrcJIT执行C++代码 (`orc_jit_executor.cpp`)

```cpp
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/Error.h" // For llvm::Error and ExitOnError
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/MemoryBuffer.h" // For llvm::MemoryBuffer::getFile

#include <iostream>
#include <memory>
#include <string>
#include <chrono>

using namespace llvm;
using namespace llvm::orc;

// 定义函数指针类型，用于JIT执行
typedef long long (*AddNumbersFunc)(long long, long long);

ExitOnError ExitOnErr; // 定义 ExitOnError 实例

int main() {
    // 1. 初始化LLVM
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    // 2. 创建LLJIT实例
    auto JIT = ExitOnErr(LLJITBuilder().create());

    // 3. 从Bitcode文件加载模块
    SMDiagnostic Err;

    // Use MemoryBuffer::getFile, and handle its ErrorOr<std::unique_ptr<MemoryBuffer>> return
    auto MemBufOrErr = MemoryBuffer::getFile("add_function.bc");
    if (std::error_code EC = MemBufOrErr.getError()) { // Check for error
        // Convert the std::error_code to an llvm::Error
        ExitOnErr(errorCodeToError(EC)); // Use errorCodeToError to convert
        return 1; // ExitOnErr will terminate, but return for completeness
    }
    // No error, get the unique_ptr<MemoryBuffer>
    std::unique_ptr<MemoryBuffer> MemBuf = std::move(MemBufOrErr.get());


    // Create a new LLVMContext for the module to be added to the JIT
    auto Ctx = std::make_unique<LLVMContext>();
    auto M = parseIR(*MemBuf, Err, *Ctx);

    if (!M) {
        Err.print("orc_jit_executor", errs());
        return 1;
    }

    // 将Module添加到JIT
    ExitOnErr(JIT->addIRModule(ThreadSafeModule(std::move(M), std::move(Ctx))));

    // 4. 获取函数指针
    auto AddNumbersSym = ExitOnErr(JIT->lookup("add_numbers"));
    AddNumbersFunc add_func = reinterpret_cast<AddNumbersFunc>(AddNumbersSym.getValue());

    if (!add_func) {
        std::cerr << "Error: 'add_numbers' function not found in the module." << std::endl;
        return 1;
    }

    // 5. 性能测试
    long long a = 123456789012345LL;
    long long b = 987654321098765LL;
    long long c_result;
    long long jit_result;
    int num_iterations = 100000000; // 1亿次迭代

    std::cout << "Starting performance test with " << num_iterations << " iterations..." << std::endl;

    // 传统的C函数调用（模拟如果不在JIT中）
    auto start_c = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        c_result = a + b; // 直接在C++代码中执行加法
    }
    auto end_c = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff_c = end_c - start_c;
    std::cout << "C++ direct addition time: " << diff_c.count() << " seconds, Result: " << c_result << std::endl;

    // JIT编译执行加法函数
    auto start_jit = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        jit_result = add_func(a, b);
    }
    auto end_jit = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff_jit = end_jit - start_jit;
    std::cout << "JIT compiled addition time: " << diff_jit.count() << " seconds, Result: " << jit_result << std::endl;

    std::cout << "\nComparison:\n";
    std::cout << "C++ direct addition vs JIT compiled addition\n";
    if (diff_jit.count() < diff_c.count()) {
        std::cout << "JIT is " << diff_c.count() / diff_jit.count() << "x faster!" << std::endl;
    } else {
        std::cout << "C++ direct is " << diff_jit.count() / diff_c.count() << "x faster (or similar)." << std::endl;
    }

    return 0;
}
```

### 步骤4：编译和运行 `orc_jit_executor.cpp`

```
clang++ -std=c++17 orc_jit_executor.cpp -o orc_jit_executor $(llvm-config --cflags --ldflags --libs core executionengine orcjit mcjit irreader bitreader)
```

### 运行：

`./orc_jit_executor`

