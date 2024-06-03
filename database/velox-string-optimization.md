## 概述

`StringView` 是 Velox 中用于表示可变长度字符串的一种轻量级封装，旨在在大规模字符串处理场景下兼顾空间利用与访问效率。



## 整体内存布局

```c++
  // We rely on all members being laid out top to bottom . C++
  // guarantees this.
  uint32_t size_;
  char prefix_[4];
  union {
    char inlined[8];
    const char* data;
  } value_;
```

**`size_`（4 字节）**：字符串实际长度（字节数）。

**`prefix_[4]`（4 字节）**：直接存放原始字符串的前 4 个字节（不足 4 字节时，用 `'\0'` 填充）。用于快速比较，提升过滤和排序的性能。

**`value_`（8 字节）**：`union`，二选一：

- 当 `size_ ≤ 12` 时，将后续的第 5~12 字节全部存入 `inlined[8]`。如果实际字符不足 8 字节，剩余位置填 `'\0'`。
- 当 `size_ > 12` 时，仅保存一个 `const char* data`，指向堆上新分配的一块 `size_` 字节空间，用于存放整个字符串的内容。



## 设计动机与优化点

### 前缀（`prefix_`）的双重作用

**快速长度与前四字节校验**

- 在常见的“字符串相等”或“字典序”比较中，如果两个字符串长度不同，或前四字节不一致，大多数情况下都能直接判定它们不相等、或依赖前四字节即可决定大小。这时只需一次 `uint64_t` 比较（高 32 位为长度、低 32 位为前 4 字节），便可在冷热路径上快速短路。

**避免堆访问**

- 当字符串前 4 字节不同，比较逻辑会在进入“inline”或“out-of-line”部分之前就提前结束，无需访问堆内存或后续 bytes，大幅降低 cache miss 与内存带宽消耗。

### 短字符串内联（`size_ ≤ 12`）

**零堆分配、零开销**

- 大量业务场景中，字符串长度往往集中在“极短”区间（如单词、标签、枚举值等）。将 “≤12 字节” 归为内联模式，可以完全绕过 `new/delete`、绕过堆分配成本。

**一次性 8 字节原子比较**

- 内联后，位置 `[5..12]` 的 8 字节已经在构造时填满了“有效字符 + `\0`”，两两对比可直接用一次 `uint64_t` 原子比较替代 `memcmp`，既保证正确性，也减少了分支和循环开销。

**缓存友好**

- 整个对象（16 字节）紧凑地存储在堆栈或连续数组里，对 CPU 缓存更友好。对短字符串的访问、拷贝与比较均落在 L1/L2 缓存层，无需访问堆。

### 拷贝与移动逻辑

**短字符串复制时无额外开销**

- 如果 `size_ ≤ 12`，复制时直接 memcpy 16 字节即可（`size_`、`prefix_`、`inlined` 三部分一并复制，不存在深拷贝）。

**长字符串复制需深拷贝**

- 当 `size_ > 12` 时，复制会触发一次堆分配与内存拷贝，将原字符串完整 `memcpy(data, other.data, size_)`；
- 移动则只需“窃取”原指针，并将原对象置空，避免额外的复制。

**析构释放仅针对 long 模式**

- 短字符串析构时，无需任何内存释放；长字符串析构时，仅需 `delete[] data`。



## 核心函数实现要点

1. `sizeAndPrefixAsInt64()`

```c++
inline int64_t sizeAndPrefixAsInt64() const {
    return reinterpret_cast<const int64_t*>(this)[0];
  }
```

- 将 `size_` 放在高 32 位，将 `prefix_[4]`（前四字符原始字节）依次放在低 32 位，共同组成一个 `uint64_t`。
- 在 `operator==` 中，只需一步 `if (sizeAndPrefixAsInt64() != other.sizeAndPrefixAsInt64()) return false;` 即可同时判定“长度不同”或“前四字节不同”两种情况。



2. `inlinedAsInt64()` & `isInline()`

```c++
  bool isInline() const {
    return isInline(size_);
  }

  FOLLY_ALWAYS_INLINE static constexpr bool isInline(uint32_t size) {
    return size <= kInlineSize;
  }
  
  inline int64_t inlinedAsInt64() const {
    return reinterpret_cast<const int64_t*>(this)[1];
  }
  
```

当 `size_ ≤ 12` 时，`inlined[0..7]` 中保存的是第 5~12 字节的真实数据或 `\0` 填充。把它们解释成一个 `uint64_t`，可一次性完成对后半段的 8 字节比较。

`isInline()` 仅判断 `size_` 是否已落在“内联阈值”以内。



3. `operator==`核心逻辑

```c++
  bool operator==(const StringView& other) const {
    // Compare lengths and first 4 characters.
    if (sizeAndPrefixAsInt64() != other.sizeAndPrefixAsInt64()) {
      return false;
    }
    if (isInline()) {
      // The inline part is zeroed at construction, so we can compare
      // a word at a time if data extends past 'prefix_'.
      return size_ <= kPrefixSize || inlinedAsInt64() == other.inlinedAsInt64();
    }
    // Sizes are equal and this is not inline, therefore both are out
    // of line and have kPrefixSize first in common.
    return memcmp(
               value_.data + kPrefixSize,
               other.value_.data + kPrefixSize,
               size_ - kPrefixSize) == 0;
  }
```

**Step 1 (高效短路)** 通过 `sizeAndPrefixAsInt64()` 在一次 64 位比较里快速判定“长度”或“前 4 字节”是否一致。

**Step 2 (长度 ≤ 4)** 若字符串长度 ≤ 4，则前 4 字节的 `prefix_` 就覆盖了全部有效字符，直接返回 `true`。

**Step 3 (短串范围 5~12 字节)** 利用 `inlinedAsInt64()` 一次比后续 8 字节，若有不等，则返回 false；否则 `true`。

**Step 4 (长串范围 >12 字节)** 剩余要比较的是从原始数据第 5 个字节开始，总共 `rem = size_ - 4` 字节。



4. `compare`（字典序比较）

```c++
// Returns 0, if this == other
//       < 0, if this < other
//       > 0, if this > other
int32_t compare(const StringView& other) const {
  // 1) 先比较前缀（prefixAsInt() 是把前 4 字节看作一个 uint32_t）
  if (prefixAsInt() != other.prefixAsInt()) {
    // 只要前缀不同，直接返回 memcmp(prefix_, other.prefix_, 4)
    // “较短的被零填充前缀”保证补齐后依然按字典序正确
    return memcmp(prefix_, other.prefix_, kPrefixSize);
  }

  // 2) 前缀相等，决定要比较的长度：取两者较短字符串的长度 - 4
  //    如果这里得到 size <= 0，说明至少有一个字符串长度 ≤ 4，
  //    在“前缀内”就已经分出了较短/较长
  int32_t size = std::min(size_, other.size_) - kPrefixSize;
  if (size <= 0) {
    // One ends within the prefix: 前缀相等且某个长度 ≤ 4，
    // 那直接按总长度差 size_ - other.size_ 来决定。短的排前面。
    return size_ - other.size_;
  }

  // 3) 前缀相等且两者长度都 > 4：
  //    如果都属于 Inline 模式（≤ 12 字节），对后续 inlined 部分一次 memcmp
  if (isInline() && other.isInline()) {
    int32_t result = memcmp(value_.inlined, other.value_.inlined, size);
    return (result != 0) ? result : size_ - other.size_;
  }

  // 4) 至少有一个是 Out‐of‐line（>12 字节），对堆上 data + 4 开始的 size 字节做 memcmp
  int32_t result =
      memcmp(data() + kPrefixSize, other.data() + kPrefixSize, size);
  return (result != 0) ? result : size_ - other.size_;
}
```

