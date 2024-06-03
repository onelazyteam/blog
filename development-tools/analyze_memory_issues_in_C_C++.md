大部分人觉得C/C++比较难，主要是因为指针的灵活性以及内存的使用，C和C++需要程序员自己来控制内存，自己申请，自己释放，很容易就会出现各种头疼难搞的内存问题从而导致系统core dump，这类问题，除了平时自己写code注意，也要在出现问题的时候懂得借鉴“巨人”为我们提供的定位方法，比如比较好用的clang引入，gcc4.8后支持的功能Address Sanitizer和Thread Sanitizer。

## **Address  Sanitizer**: 

顾名思义，就是用来检查非法内存错误的，这个工具比较适合检查如下几种类型的bug:

1. 越界问题
2. 使用释放后的内存
3. double free
4. 内存泄漏
5. ...

一个访问越界的例子：

```c++
#include <iostream>
using namespace std;
int main() {
  int *arr = new int[5];
  for (int i = 0; i < 6; ++i) {  // out-of-bounds access
    arr[i] = i;
  }
  delete [] arr;
  return 0;
}

compile：
clang++ -g -fsanitize=address out-of-bounds.cpp -o result（gcc使用方法一样）
execute：
./result
```

```c++
=================================================================
==69004==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x6030000002c4 at pc 0x000109f7eec5 bp 0x7ffee5c81750 sp 0x7ffee5c81748
WRITE of size 4 at 0x6030000002c4 thread T0
    #0 0x109f7eec4 in main out-of-bounds.cpp:6
    #1 0x7fff76c9a014 in start (libdyld.dylib:x86_64+0x1014)

0x6030000002c4 is located 0 bytes to the right of 20-byte region [0x6030000002b0,0x6030000002c4)
allocated by thread T0 here:
    #0 0x109fea122 in wrap__Znam (libclang_rt.asan_osx_dynamic.dylib:x86_64h+0x63122)
   #1 0x109f7ee4a in main out-of-bounds.cpp:4
   #2 0x7fff76c9a014 in start (libdyld.dylib:x86_64+0x1014)

SUMMARY: AddressSanitizer: heap-buffer-overflow out-of-bounds.cpp:6 in main
Shadow bytes around the buggy address:
  0x1c0600000000: fa fa 00 00 00 fa fa fa 00 00 00 00 fa fa 00 00
  0x1c0600000010: 00 00 fa fa 00 00 00 00 fa fa fd fd fd fa fa fa
  0x1c0600000020: fd fd fd fd fa fa 00 00 00 00 fa fa 00 00 00 fa
  0x1c0600000030: fa fa 00 00 00 fa fa fa 00 00 00 00 fa fa fd fd
  0x1c0600000040: fd fa fa fa fd fd fd fd fa fa 00 00 02 fa fa fa
=>0x1c0600000050: 00 00 00 00 fa fa 00 00[04]fa fa fa fa fa fa fa
  0x1c0600000060: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x1c0600000070: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x1c0600000080: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x1c0600000090: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x1c06000000a0: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==69004==ABORTING
Abort trap: 6
```

打印出来的信息非常详细，在代码的哪行code何种原因导致了非法内存访问，以及这块内存在哪里被申请了多少等等信息。



## Thread Sanitizer：

是用来检查数据竞争(Data Race)的，数据竞争指多个线程在没有正确加锁的情况下，同时访问同一块内存数据，并且至少有一个线程是写操作，对数据的读取和修改产生了竞争，从而导致各种不可预计的问题。Data Race的问题比较难查，一旦发生，结果是不可预期的，也许直接就Crash了，也许导致执行流程错乱了，也许把内存破坏导致之后某个时刻突然Crash了。

一个数据竞争的例子：

```C++
#include <pthread.h>
#include <iostream>

int Global;
pthread_mutex_t mutex_x = PTHREAD_MUTEX_INITIALIZER;

void *Thread1(void *x) {
  // pthread_mutex_lock(&mutex_x);
  ++Global;
  std::cout << "thread1 deal with global data" << std::endl;
  // pthread_mutex_unlock(&mutex_x);
  return NULL;
}

void *Thread2(void *x) {
  // pthread_mutex_lock(&mutex_x);
  ++Global;
  std::cout << "thread2 deal with global data" << std::endl;
  // pthread_mutex_unlock(&mutex_x);
  return NULL;
}

int main() {
  pthread_t thr[2];
  pthread_create(&thr[0], NULL, Thread1, NULL);
  pthread_create(&thr[1], NULL, Thread2, NULL);
  pthread_join(thr[0], NULL);
  pthread_join(thr[1], NULL);
  return 0;
}

在不加锁的情况下，这两个线程就会同时去修改Global变量，从而导致数据竞争。

compile：
clang++ -g -fsanitize=thread data-race.cpp -o result（gcc使用方法一样）
execute：
./result
```

```c
thread1 deal with global data
==================
WARNING: ThreadSanitizer: data race (pid=69339)
  Write of size 4 at 0x0001052a4198 by thread T2:
    #0 Thread2(void*) data-race.cpp:17 (result:x86_64+0x100001776)

  Previous write of size 4 at 0x0001052a4198 by thread T1:
    #0 Thread1(void*) data-race.cpp:9 (result:x86_64+0x100001486)

  Location is global 'Global' at 0x0001052a4198 (result+0x000100003198)

  Thread T2 (tid=1839532, running) created by main thread at:
    #0 pthread_create <null>:1600656 (libclang_rt.tsan_osx_dynamic.dylib:x86_64h+0x2936d)
    #1 main data-race.cpp:26 (result:x86_64+0x10000184e)

  Thread T1 (tid=1839531, finished) created by main thread at:
    #0 pthread_create <null>:1600656 (libclang_rt.tsan_osx_dynamic.dylib:x86_64h+0x2936d)
    #1 main data-race.cpp:25 (result:x86_64+0x10000182f)

SUMMARY: ThreadSanitizer: data race data-race.cpp:17 in Thread2(void*)
==================
==================
WARNING: ThreadSanitizer: data race (pid=69339)
  Read of size 4 at 0x7fffaefd66f8 by thread T2:
    #0 std::__1::basic_ostream<char, std::__1::char_traits<char> >& std::__1::__put_character_sequence<char, std::__1::char_traits<char> >(std::__1::basic_ostream<char, std::__1::char_traits<char> >&, char const*, unsigned long) ostream:732 (result:x86_64+0x100001c30)
    #1 std::__1::basic_ostream<char, std::__1::char_traits<char> >& std::__1::operator<<<std::__1::char_traits<char> >(std::__1::basic_ostream<char, std::__1::char_traits<char> >&, char const*) ostream:864 (result:x86_64+0x100001553)
    #2 Thread2(void*) data-race.cpp:18 (result:x86_64+0x100001792)

  Previous write of size 4 at 0x7fffaefd66f8 by thread T1:
    #0 std::__1::basic_ostream<char, std::__1::char_traits<char> >& std::__1::__put_character_sequence<char, std::__1::char_traits<char> >(std::__1::basic_ostream<char, std::__1::char_traits<char> >&, char const*, unsigned long) ostream:732 (result:x86_64+0x100001d52)
    #1 std::__1::basic_ostream<char, std::__1::char_traits<char> >& std::__1::operator<<<std::__1::char_traits<char> >(std::__1::basic_ostream<char, std::__1::char_traits<char> >&, char const*) ostream:864 (result:x86_64+0x100001553)
    #2 Thread1(void*) data-race.cpp:10 (result:x86_64+0x1000014a2)

  Location is global 'std::__1::cout' at 0x7fffaefd6660 (libc++.1.dylib+0x00003a3ee6f8)

  Thread T2 (tid=1839532, running) created by main thread at:
    #0 pthread_create <null>:1599920 (libclang_rt.tsan_osx_dynamic.dylib:x86_64h+0x2936d)
    #1 main data-race.cpp:26 (result:x86_64+0x10000184e)

  Thread T1 (tid=1839531, finished) created by main thread at:
    #0 pthread_create <null>:1599920 (libclang_rt.tsan_osx_dynamic.dylib:x86_64h+0x2936d)
    #1 main data-race.cpp:25 (result:x86_64+0x10000182f)

SUMMARY: ThreadSanitizer: data race ostream:732 in std::__1::basic_ostream<char, std::__1::char_traits<char> >& std::__1::__put_character_sequence<char, std::__1::char_traits<char> >(std::__1::basic_ostream<char, std::__1::char_traits<char> >&, char const*, unsigned long)
==================
==================
WARNING: ThreadSanitizer: data race (pid=69339)
  Read of size 8 at 0x7fffaefd6680 by thread T2:
    #0 std::__1::ostreambuf_iterator<char, std::__1::char_traits<char> > std::__1::__pad_and_output<char, std::__1::char_traits<char> >(std::__1::ostreambuf_iterator<char, std::__1::char_traits<char> >, char const*, char const*, char const*, std::__1::ios_base&, char) locale:1388 (result:x86_64+0x10000222b)
    #1 std::__1::basic_ostream<char, std::__1::char_traits<char> >& std::__1::__put_character_sequence<char, std::__1::char_traits<char> >(std::__1::basic_ostream<char, std::__1::char_traits<char> >&, char const*, unsigned long) ostream:725 (result:x86_64+0x100001dc1)
    #2 std::__1::basic_ostream<char, std::__1::char_traits<char> >& std::__1::operator<<<std::__1::char_traits<char> >(std::__1::basic_ostream<char, std::__1::char_traits<char> >&, char const*) ostream:864 (result:x86_64+0x100001553)
    #3 Thread2(void*) data-race.cpp:18 (result:x86_64+0x100001792)

  Previous write of size 8 at 0x7fffaefd6680 by thread T1:
    #0 std::__1::ostreambuf_iterator<char, std::__1::char_traits<char> > std::__1::__pad_and_output<char, std::__1::char_traits<char> >(std::__1::ostreambuf_iterator<char, std::__1::char_traits<char> >, char const*, char const*, char const*, std::__1::ios_base&, char) locale:1420 (result:x86_64+0x100002943)
    #1 std::__1::basic_ostream<char, std::__1::char_traits<char> >& std::__1::__put_character_sequence<char, std::__1::char_traits<char> >(std::__1::basic_ostream<char, std::__1::char_traits<char> >&, char const*, unsigned long) ostream:725 (result:x86_64+0x100001dc1)
    #2 std::__1::basic_ostream<char, std::__1::char_traits<char> >& std::__1::operator<<<std::__1::char_traits<char> >(std::__1::basic_ostream<char, std::__1::char_traits<char> >&, char const*) ostream:864 (result:x86_64+0x100001553)
    #3 Thread1(void*) data-race.cpp:10 (result:x86_64+0x1000014a2)

  Location is global 'std::__1::cout' at 0x7fffaefd6660 (libc++.1.dylib+0x00003a3ee680)

  Thread T2 (tid=1839532, running) created by main thread at:
    #0 pthread_create <null>:1598912 (libclang_rt.tsan_osx_dynamic.dylib:x86_64h+0x2936d)
    #1 main data-race.cpp:26 (result:x86_64+0x10000184e)

  Thread T1 (tid=1839531, finished) created by main thread at:
    #0 pthread_create <null>:1598912 (libclang_rt.tsan_osx_dynamic.dylib:x86_64h+0x2936d)
    #1 main data-race.cpp:25 (result:x86_64+0x10000182f)

SUMMARY: ThreadSanitizer: data race locale:1388 in std::__1::ostreambuf_iterator<char, std::__1::char_traits<char> > std::__1::__pad_and_output<char, std::__1::char_traits<char> >(std::__1::ostreambuf_iterator<char, std::__1::char_traits<char> >, char const*, char const*, char const*, std::__1::ios_base&, char)
==================
thread2 deal with global data
ThreadSanitizer: reported 3 warnings
Abort trap: 6
```

信息同样也很详细，会指出哪些线程会如何产生data race, 以及这两个线程是在何处create出来的。 

## 参考链接：

https://gcc.gnu.org/onlinedocs/gcc-4.9.2/gcc/Debugging-Options.html#index-fsanitize_003dthread-595

http://clang.llvm.org/docs/AddressSanitizer.html

http://clang.llvm.org/docs/ThreadSanitizer.html