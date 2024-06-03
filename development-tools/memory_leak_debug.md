内存泄漏(memory leak)是指程序在申请内存之后，没有在期望释放的时候释放掉，从而造成内存空间的浪费。内存泄漏有时不易被察觉，但有时候也会很严重，就会出现out of memory的情况。

检查内存泄漏有几种比较常用的工具：**TcMalloc，Valgrind，Instruments**，这几种工具网络上资料比较多，在这里就不再赘述它的具体使用，这次想聊一下另外一种个人认为相对比较好的方法，在开始之前先简单说下这几个工具的情况。

**Instruments**：Xcode自带的一个工具，所以只能在Mac上使用

**TcMalloc**：大多数使用TcMalloc是出于提升性能的考虑，曾做过简单实验，单纯的重复内存申请释放，tcmalloc可以提升2-3倍的性能，对于它如何提升性能在这里不过多阐述，有兴趣的同学可以自行上网搜索相关资料。可能是因为设计之初内存检测的相关功能本就不是重点的缘故，tcmalloc在内存检测上比较鸡肋，为了打开内存检测功能，需要设置许多环境变量，而且内存检测报告的可读性不是很好，对内存检测也不是很充分(例如无法检测数组删除内存泄漏，内存越界检查功能也很多情况检测不出)，而且打开tcmalloc的内存检测功能后，可能导致系统无法正常运行。

*Valgrind*：Valgrind的内存泄漏检查功能和内存非法使用检测一样优秀，报告简单明了，可读性强，而且检查十分充分，但是我们都应该明白一个道理-----没有完美的东西，Valgrind由内核（core）以及基于内核的其他调试工具组成，内核类似于一个框架，它模拟了一个CPU环境，并提供服务给其他工具；而其他工具则类似于插件，利用内核提供的服务完成各种特定的内存调试任务，这就会导致在大型软件中使用Valgrind的时候反应很慢，而且可能会导致系统无法正常运行。

所以，基于上面的情况，这就有人发明了下面这种方式。

工欲善其事，必先利其器，我们先通过man看一下这种方式主要使用到的**gcc的wrap选项**，man ld，之后使用/--wrap搜索wrap

```c++
       --wrap=symbol
           Use a wrapper function for symbol.  Any undefined reference to symbol will be resolved to
           "__wrap_symbol".  Any undefined reference to "__real_symbol" will be resolved to symbol.

           This can be used to provide a wrapper for a system function.  The wrapper function should be
           called "__wrap_symbol".  If it wishes to call the system function, it should call
           "__real_symbol".

           Here is a trivial example:

                   void *
                   __wrap_malloc (size_t c)
                   {
                     printf ("malloc called with %zu\n", c);
                     return __real_malloc (c);
                   }

           If you link other code with this file using --wrap malloc, then all calls to "malloc" will call
           the function "__wrap_malloc" instead.  The call to "__real_malloc" in "__wrap_malloc" will call
           the real "malloc" function.

           You may wish to provide a "__real_malloc" function as well, so that links without the --wrap
           option will succeed.  If you do this, you should not put the definition of "__real_malloc" in
           the same file as "__wrap_malloc"; if you do, the assembler may resolve the call before the
           linker has a chance to wrap it to "malloc".
```

我们使用wrap选项来定制我们自己的malloc/free,new/delete

```c++
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
extern "C" {
    void *__real_malloc(size_t sz);
    void *__wrap_malloc(size_t sz) {
        printf("use my malloc!\n");
        return __real_malloc(sz);
    }   

    void __real_free(void* p); 
    void __wrap_free(void* p) {
        printf("use my free!\n");
        __real_free(p);
        return;
    }   
}

int main() {
    void *p = malloc(12);
    free (p);
}

编译：
g++ wrap.cpp -Wl,--wrap,malloc -Wl,--wrap,free

执行：
use my malloc!
use my free!
```

```c++
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
void *__real_malloc(size_t sz);

void *__wrap_malloc(size_t sz) {
    void *p = __real_malloc(sz);
    std::cout << __func__ << std::endl;
    // put the stack into one container in the future(insert)
    return p;
}

void __real_free(void *p);

void __wrap_free(void *p) {
    std::cout << __func__ << std::endl;
    // delete the stack from the container in the future(erase)
    __real_free(p);
    return;
}

void *__real_realloc(void *p, unsigned int newsize);

void *__wrap_realloc(void *p, unsigned int newsize) {
    std::cout << __func__ << std::endl;
    void *q = __real_realloc(p, newsize);
    return q;
}

void *__real_calloc(size_t n, size_t size);

void *__wrap_calloc(size_t n, size_t size) {
    std::cout << __func__ << std::endl;
    void *p = __real_calloc(n, size);
    return p;
}
void __real_cfree(void* p);

void __wrap_cfree(void* p) {
    std::cout << __func__ << std::endl;
    __real_cfree(p);
    return;
}
}

void* operator new(size_t sz) {
    std::cout << __func__ << std::endl;
    void *p = __real_malloc(sz);
    // put the stack into one container in the future(insert)
    return p;
}

void* operator new[] (size_t sz) {
    std::cout << __func__ << std::endl;
    void *p = __real_malloc(sz);
    // put the stack into one container in the future(insert)
    return p;
}


void operator delete(void* p) {
    std::cout << __func__ << std::endl;
    __real_free(p);
    // delete the stack from the container in the future(erase)
    return;
}

void operator delete[](void* p) {
    std::cout << __func__ << std::endl;
    // delete the stack from the container in the future(erase)
    __real_free(p);
    return;
}

int main() {
    void* p = malloc(10);
    free (p);
    void* s = new char[10];
    delete []s;
    return 0;
}


编译：
g++ memoryLeak.cpp -Wl,--wrap,malloc -Wl,--wrap,free -Wl,--wrap,realloc -Wl,--wrap,calloc -Wl,--wrap,cfree

执行：
__wrap_malloc
__wrap_free
operator new []
operator delete
```

说上面两个例子，是想说明，我们可以根据自己需求来实现一套malloc/free, new/delete，这样我们就可以基于这个功能来实现我们内存泄漏检测的功能，具体步骤简述如下：

1.定制自己的内存操作接口

2.使用glog的backtrace或者系统的backtrace来记录