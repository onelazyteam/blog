## 编译安装leveldb

### Getting the Source

```
git clone --recurse-submodules https://github.com/google/leveldb.git
```

### Building

```
mkdir -p build && cd build
cmake -DCMAKE_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
sudo cmake --install .
```

cmake默认安装到/usr/local目录下，所以 LevelDB 安装后，主要文件会在这些位置：

- 头文件：
     `/usr/local/include/leveldb/*.h`
- 静态库：
     `/usr/local/lib/libleveldb.a`

### 示例代码

```c++
#include <cassert>
#include <iostream>
#include <string>
#include <leveldb/db.h>

int main() {
  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
  assert(status.ok());

  std::string key = "apple";
  std::string value = "A";
  std::string get;

  leveldb::Status s = db->Put(leveldb::WriteOptions(), key, value);
  
  if (s.ok()) s = db->Get(leveldb::ReadOptions(), key, &get);
  if (s.ok()) std::cout << "读取到的与(key=" << key << ")对应的(value=" << get << ")" << std::endl;
  else std::cout << "读取失败!" << std::endl;

  delete db;

  return 0;
}
```

#### 编译实例代码并运行

```shell
clang++ -o leveldb_demo leveldb.cpp -pthread -lleveldb -std=c++17

./leveldb_demo
读取到的与(key=apple)对应的(value=A)
```