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
