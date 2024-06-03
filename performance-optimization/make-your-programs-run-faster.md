## restrict关键字

当多个指针可能指向同一内存区域时，编译器必须假设它们可能相互影响，无法进行某些优化（如循环展开、向量化等）。`restrict` 告诉编译器这些指针不会重叠，使其生成更高效的代码，这可以带来多种优化，例如更长时间地将值保存在寄存器中、向量化 (SIMD)，循环展开，指令重排序等。

如下是一个在数据库向量化执行器中常用的写法：

[clickhouse的restrict优化](https://github.com/ClickHouse/ClickHouse/pull/19946/files) 

```C++
// 无 restrict：编译器必须假设 a、b、c 可能指向重叠内存，无法进行向量化优化。
void vec_add(size_t n, double *a, double *b, double *c) {
    for (size_t i = 0; i < n; ++i) {
        c[i] = a[i] + b[i];
    }
}
// 汇编代码
add_arrays:
    // ; 每次循环需检查内存依赖
    mov     eax, DWORD PTR [rdi + rdx*4]
    add     eax, DWORD PTR [rsi + rdx*4]
    mov     DWORD PTR [rcx + rdx*4], eax
    inc     rdx
    cmp     rdx, r8
    jl      .L3


// 在 GCC/Clang 下加上 __restrict__ 后，编译器可假设三者不重叠，可能使用 SIMD 指令（如 SSE/AVX）并行处理多个数据，显著提升性能。
void vec_add(size_t n,
    double * __restrict__ a,
    double * __restrict__ b,
    double * __restrict__ c)
{
    for (size_t i = 0; i < n; ++i) {
        c[i] = a[i] + b[i];
    }
}
// 汇编代码
add_arrays:
    // ; 使用向量化指令（如 vmovdqa, vpaddd）
    vmovdqu ymm0, YMMWORD PTR [rdi + rax]
    vpaddd  ymm0, ymm0, YMMWORD PTR [rsi + rax]
    vmovdqu YMMWORD PTR [rcx + rax], ymm0
    add     rax, 32
    cmp     rax, r8
    j
```



## constexpr关键字

在 C++11 中，引入了“广义常量表达式”机制，通过 `constexpr` 关键字允许函数或对象构造在编译期求值，并强制编译器检查其返回或初始化必须是常量表达式。

对于被标记为 `constexpr` 的函数，编译器会在参数为常量表达式时执行计算；若参数在运行时才确定，则该函数又表现得像普通函数，不影响正确性。

例如：

```C++
constexpr int factorial(int n) {
    return n <= 1 ? 1 : (n * factorial(n - 1));
}
constexpr int f5 = factorial(5);  // 编译期计算，f5 == 120
```

该递归函数在编译期展开并计算，消除了运行时调用与乘法的开销。

```C++
struct Point {
    double x, y;
    constexpr Point(double x_, double y_) : x(x_), y(y_) {}
};
constexpr Point origin{0.0, 0.0};  // 在编译期间构造
```

对象若能在编译期构造，整个初始化过程无需在运行时重复执行，可存放在只读数据段，提升性能并增强线程安全性。



## inline内联

提示编译器将函数体展开到调用点，消除函数调用开销。

可以使用\__attribute__((always_inline))强制内联。

```C++
inline void get_data() {
  return data_;
}
```



## 分支预测提示[[likely]]` / `[[unlikely]]

```C++ 
if (likely(c1 > 1)) {
  // ...
} else {
  // ...
}
```



## 预取__builtin_prefetch

显式地在内存访问前发出预取指令，将指定地址的数据提前加载到缓存，适用于**大数据量**且硬件预取器难以自动识别的访问模式

```C++
for (size_t i = 0; i + 8 < n; ++i) {
    __builtin_prefetch(&arr[i + 8]);
    process(arr[i]);
}
```

