// orc_jit_executor.cpp
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
