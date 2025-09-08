cmake ..   -DBUILD_JAVA=OFF   -DBUILD_CPP=ON   -DBUILD_TOOLS=OFF   -DUSE_SYSTEM_ZSTD=ON   -DZSTD_HOME=$(brew --prefix zstd)   -DSNAPPY_HOME=$(brew --prefix snappy)   -DLZ4_HOME=$(brew --prefix lz4)   -DPROTOBUF_HOME=$(brew --prefix protobuf)   -DZLIB_INCLUDE_DIR=$(brew --prefix zlib)/include   -DZLIB_LIBRARY=$(brew --prefix zlib)/lib/libz.dylib   -DCMAKE_PREFIX_PATH="$(brew --prefix)"
make -j4
sudo cmake --install . --prefix /usr/local
