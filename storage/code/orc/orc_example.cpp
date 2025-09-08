// orc_example.cpp
// 示例：写入 example.orc（3 列：id:int, name:string, salary:double）
// 然后读取并打印内容。
// 依赖：Apache ORC C++ 库

#include <orc/OrcFile.hh>
#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>

int main() {
  const std::string filename = "example.orc";

  // ---------- 写入阶段 ----------
  {
    // 定义 schema
    std::unique_ptr<orc::Type> schema(
      orc::Type::buildTypeFromString("struct<id:int,name:string,salary:double>"));

    // 输出流（写本地文件）
    std::unique_ptr<orc::OutputStream> outStream = orc::writeLocalFile(filename);
    orc::WriterOptions writerOptions;
    std::unique_ptr<orc::Writer> writer = orc::createWriter(*schema, outStream.get(), writerOptions);

    const uint64_t batchSize = 1024;
    std::unique_ptr<orc::ColumnVectorBatch> batch = writer->createRowBatch(batchSize);

    // 根是 struct
    orc::StructVectorBatch *root = dynamic_cast<orc::StructVectorBatch *>(batch.get());
    orc::LongVectorBatch   *idBatch     = dynamic_cast<orc::LongVectorBatch *>(root->fields[0]);
    orc::StringVectorBatch *nameBatch   = dynamic_cast<orc::StringVectorBatch *>(root->fields[1]);
    orc::DoubleVectorBatch *salaryBatch = dynamic_cast<orc::DoubleVectorBatch *>(root->fields[2]);

    // 为字符串数据准备一个固定大小的池（注意：指针必须在 writer->add 之前保持有效）
    // 这里示例中每批预分配固定大空间；生产环境要做更稳健的容量管理。
    std::vector<char> stringPool(batchSize * 32);
    size_t poolOffset = 0;

    uint64_t rows = 0;
    const uint64_t totalRows = 10; // 示例写 10 行
    for (uint64_t i = 0; i < totalRows; ++i) {
      // id
      idBatch->data[rows] = static_cast<int64_t>(i);

      // name (string)
      std::string name = "user_" + std::to_string(i);
      size_t need = name.size();
      // 如果池空间不够，先把当前batch flush 写入文件并重置池
      if (poolOffset + need > stringPool.size() || rows == batchSize) {
        // 先写入已有 rows
        root->numElements = rows;
        idBatch->numElements = rows;
        nameBatch->numElements = rows;
        salaryBatch->numElements = rows;
        writer->add(*batch);

        // reset
        rows = 0;
        poolOffset = 0;
      }

      // 把字符串拷贝到池中并记录指针/长度
      memcpy(stringPool.data() + poolOffset, name.data(), need);
      nameBatch->data[rows]   = stringPool.data() + poolOffset;
      nameBatch->length[rows] = static_cast<int64_t>(need);
      poolOffset += need;

      // salary
      salaryBatch->data[rows] = 1000.0 + static_cast<double>(i) * 10.5;

      ++rows;
    }

    // 写入最后残余的 rows（如果有）
    if (rows != 0) {
      root->numElements = rows;
      idBatch->numElements = rows;
      nameBatch->numElements = rows;
      salaryBatch->numElements = rows;
      writer->add(*batch);
      rows = 0;
    }

    writer->close();
    std::cout << "Wrote ORC file: " << filename << "\n";
  }

  // ---------- 读取阶段 ----------
  {
    std::unique_ptr<orc::InputStream> inStream = orc::readLocalFile(filename);
    orc::ReaderOptions readerOptions;
    std::unique_ptr<orc::Reader> reader = orc::createReader(std::move(inStream), readerOptions);

    orc::RowReaderOptions rowOpts;
    std::unique_ptr<orc::RowReader> rowReader = reader->createRowReader(rowOpts);
    std::unique_ptr<orc::ColumnVectorBatch> batch = rowReader->createRowBatch(1024);

    while (rowReader->next(*batch)) {
      auto *structBatch = dynamic_cast<orc::StructVectorBatch *>(batch.get());
      auto *idB = dynamic_cast<orc::LongVectorBatch *>(structBatch->fields[0]);
      auto *nameB = dynamic_cast<orc::StringVectorBatch *>(structBatch->fields[1]);
      auto *salaryB = dynamic_cast<orc::DoubleVectorBatch *>(structBatch->fields[2]);

      for (uint64_t r = 0; r < batch->numElements; ++r) {
        int64_t id = idB->data[r];
        std::string name(nameB->data[r], static_cast<size_t>(nameB->length[r]));
        double salary = salaryB->data[r];
        std::cout << "row: id=" << id
                  << ", name=\"" << name << "\""
                  << ", salary=" << salary << "\n";
      }
    }
  }

  return 0;
}

