​	**IVFFlat（Inverted File with Flat vectors）** 是一种常见的近似最近邻（ANN, Approximate Nearest Neighbor）向量索引结构，广泛用于向量数据库（如 **pgvector、FAISS、Milvus**）。其核心思想是：**先把向量空间分成多个簇（cluster），查询时只搜索部分簇，从而减少计算量。**

#### 一、IVFFlat 基本结构

IVFFlat由两部分组成：

##### 1. Coarse Quantizer（粗量化器）

用于把向量空间划分为多个簇：

- 使用 **k-means++ 聚类**
- 得到 `nlist` 个中心点（centroids），每个中心代表一个 **inverted list**

##### 2. Inverted Lists（倒排表）

每个簇对应一个列表：

```typescript
List 0 (centroid C0):
  v1
  v7
  v25

List 1 (centroid C1):
  v2
  v9
  v14
```



#### 二、构建流程（Build Index）

##### Step1：采样训练数据

大部分向量数据库都会使用K-Means++算法用来避免K-Means算法出现的不稳定场景

IVFFlat必须先训练：

```typescript
Training vectors
     ↓
KMeans++
     ↓
Centroids
```

##### Step2：向量分配到簇

对于每个向量：`v → 最近 centroid`，存入对应list

##### Step3：写入倒排表

对于每个list保存：

```c++
struct InvertedList {
    int64_t* ids;      // [n * 1]  每个向量的 ID
    float*   vectors;  // [n * dim] 向量数据（连续存储）
}

list 17:
id=10 vector=[...]
id=28 vector=[...]
id=55 vector=[...]
```

之所以***大部分生成系统的向量数据库都记录了id***，是为了方便与外部原始数据建立联系，不然只拿到topn个向量值，没有任何意义



#### 三、查询流程（Search）

##### Step1：找最近的centroids

例如：`nprobe = 5`，查找最近的5个簇

计算该向量与每个向量簇中心点的距离，选择最近的前5个簇

##### Step2：扫描倒排表

##### Step3：Top-K选择

`k = 10`，从5个倒排表中选择出k个最相近的向量点



#### 四、关键参数

IVFFlat核心参数只有两个：

##### 1. nlist（簇数量），k-means训练多少个聚类簇

例如：`nlist = 1000`

| nlist大  | nlist小  |
| -------- | -------- |
| list更短 | list更长 |
| 精度高   | 精度低   |
| 训练慢   | 训练快   |

***经验值：***总的数据量开方

`nlist ≈ sqrt(N)`

##### 2. nprobe（查询簇数）

| nprobe大 | nprobe小 |
| -------- | -------- |
| 精度高   | 精度低   |
| 查询慢   | 查询快   |

之所以查询簇不能选择1，是因为选1可能会出现极端点不准确的问题



#### 五、工程版本需要优化的地方

##### 1. SIMD距离计算

```c++
// 使用 AVX2/FMA 指令一次计算 8 个 float
#include <immintrin.h>

float fvec_L2sqr_avx2(const float* x, const float* y, size_t d) {
    __m256 sum = _mm256_setzero_ps();
    for (size_t i = 0; i < d; i += 8) {
        __m256 a = _mm256_loadu_ps(x + i);
        __m256 b = _mm256_loadu_ps(y + i);
        __m256 diff = _mm256_sub_ps(a, b);
        sum = _mm256_fmadd_ps(diff, diff, sum);  // 乘加融合
    }
    // 水平求和...
    return hsum256_ps_avx(sum);
}
```

##### 2. 更好的KMeans

选择kmeans++

##### 3. 内存布局优化

```c++
// ❌ 差：AoS，缓存不友好
struct VectorWithId {
    int64_t id;
    float data[128];  // 128维
};
std::vector<VectorWithId> invlist;

// ✅ 好：SoA，连续内存，SIMD友好
struct InvertedListSoA {
    std::vector<int64_t> ids;      // 分开存储ID
    std::vector<float> data;        // 连续存储向量 (n_vectors × d)
    
    // 访问第 i 个向量：&data[i * d]
    const float* get_vector(size_t i) const {
        return data.data() + i * d;
    }
};
```

##### 4. 多线程

search的时候，每个list并行

##### 5. 预取指令

```c++
void search_with_prefetch(const float* query, ...) {
    for (size_t i = 0; i < n_candidates; i++) {
        // 计算当前距离时，预取下一个向量
        if (i + 4 < n_candidates) {
            _mm_prefetch(candidates[i + 4].data, _MM_HINT_T0);
        }
        
        // 当前计算...
        compute_distance(query, candidates[i].data);
    }
}
```

