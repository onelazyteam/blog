# io_uring

## 引言

io_uring 是 Linux 内核自 5.1 版本引入的一种高性能异步 I/O 接口，由 Jens Axboe 开发，旨在解决传统 I/O 接口（如 select、poll、epoll 和 aio）的性能瓶颈。它通过共享的环形缓冲区实现用户空间与内核空间的高效通信，显著减少系统调用和内存拷贝的开销。io_uring 特别适合需要高吞吐量和低延迟的应用程序，例如数据库、存储引擎和网络服务器。

## io_uring 的原理

### 1. 传统 I/O 的挑战

传统的 I/O 操作，例如 `read()` 和 `write()`，通常是同步且阻塞的。这意味着当应用程序发起一个 I/O 请求时，它必须等待该请求完成后才能继续执行。这种方式存在以下几个主要的性能瓶颈：

- **上下文切换开销：** 每次 I/O 操作都需要应用程序从用户态切换到内核态，这涉及到大量的 CPU 寄存器保存和恢复操作，造成显著的性能开销。
- **数据拷贝开销：** 数据在内核缓冲区和用户缓冲区之间需要多次拷贝，这也会消耗大量的 CPU 时间。
- **阻塞等待：** 应用程序在等待 I/O 完成时会被阻塞，导致 CPU 资源的浪费。

为了解决这些问题，人们提出了异步 I/O (AIO) 的概念。AIO 允许应用程序发起多个 I/O 请求而无需等待它们完成，从而提高了 I/O 操作的并发性和效率。然而，传统的 Linux AIO 接口使用起来比较复杂，并且存在一些限制，例如不支持某些文件类型。

### 2. io_uring 简介

io_uring 是 Linux 中一种新的异步 I/O 框架，它克服了传统 AIO 的缺点，提供了一种更高效、更灵活的异步 I/O 接口。io_uring 的设计目标是：

- **高性能：** 通过减少系统调用次数和内存拷贝次数，最大程度地提高 I/O 性能。
- **低延迟：** 尽量减少 I/O 操作的延迟，以满足对延迟敏感的应用的需求。
- **灵活性：** 支持多种 I/O 操作类型，并提供丰富的配置选项，以满足不同应用的需求。
- **易用性：** 提供简单易用的 API，降低应用程序开发难度。

### 3. io_uring 的工作原理

io_uring 的核心在于两个环形缓冲区：**提交队列 (Submission Queue, SQ)** 和 **完成队列 (Completion Queue, CQ)**。

- **提交队列 (SQ):** 应用程序将 I/O 请求提交到 SQ 中。每个 I/O 请求在 SQ 中表示为一个提交队列条目 (Submission Queue Entry, SQE)。SQE 包含了 I/O 操作的详细信息，例如操作类型（读、写等）、文件描述符、缓冲区地址和长度等。
- **完成队列 (CQ):** 当 I/O 操作完成时，内核将生成一个完成队列条目 (Completion Queue Entry, CQE) 并将其放入 CQ 中。CQE 包含了已完成 I/O 操作的结果，例如操作状态和实际传输的字节数。

应用程序和内核通过共享内存来访问 SQ 和 CQ，从而避免了不必要的内存拷贝。io_uring 的工作流程如下：

1. **应用程序提交 I/O 请求：** 应用程序创建一个 SQE，设置 I/O 操作的参数，并将 SQE 放入 SQ 中。
2. **内核处理 I/O 请求：** 内核从 SQ 中获取 SQE，执行相应的 I/O 操作，并将操作结果放入 CQ 中。
3. **应用程序获取 I/O 完成事件：** 应用程序从 CQ 中获取 CQE，并根据 CQE 中的信息处理已完成的 I/O 操作。

通过这种方式，应用程序可以异步地提交多个 I/O 请求，并在后台处理已完成的 I/O 操作，从而实现高效的 I/O 并发。

### 4. io_uring 的优势

相比传统的 I/O 方式和 AIO，io_uring 具有以下显著优势：

- **减少系统调用：** io_uring 允许应用程序通过一次系统调用提交多个 I/O 请求，并通过一次系统调用获取多个 I/O 操作的完成事件，从而显著减少了系统调用的开销。
- **零拷贝：** 通过使用共享内存和 scatter/gather 机制，io_uring 可以减少甚至避免数据拷贝操作，从而提高了 I/O 效率。
- **支持多种 I/O 操作：** io_uring 不仅支持传统的读写操作，还支持诸如 `fsync()`、`open()`、`close()`、`stat()` 等多种文件系统操作，甚至支持网络 I/O 操作。
- **链式 I/O 操作：** io_uring 支持将多个 I/O 操作链接在一起，形成一个链式操作。这可以减少应用程序的干预，并提高 I/O 操作的效率。例如，应用程序可以将一个读操作和一个写操作链接在一起，从而实现“读完数据后立即写入到另一个文件”的功能。
- **轮询模式：** io_uring 支持轮询模式，在这种模式下，应用程序可以通过轮询 CQ 而不是使用阻塞的 `read()` 系统调用来获取 I/O 完成事件。这可以进一步减少延迟，并提高 I/O 性能。
- **内核侧轮询：** io_uring 还支持内核侧轮询，在这种模式下，内核线程会主动轮询 SQ，并将 I/O 请求提交到硬件设备。这可以进一步减少系统调用的开销，并提高 I/O 性能。

### 5. io_uring 的使用

使用 io_uring 进行 I/O 操作涉及以下几个主要步骤：

1. **创建 io_uring 实例：** 使用 `io_uring_setup()` 系统调用创建一个 io_uring 实例，并获取其文件描述符。
2. **映射共享内存：** 使用 `mmap()` 系统调用将 SQ 和 CQ 映射到用户空间的内存中，以便应用程序可以访问它们。
3. **准备 SQE：** 创建一个 `io_uring_sqe` 结构体，并设置 I/O 操作的参数，例如操作类型、文件描述符、缓冲区地址和长度等。
4. **提交 SQE：** 将准备好的 SQE 放入 SQ 中。
5. **提交 I/O 请求：** 使用 `io_uring_submit()` 或 `io_uring_enter()` 系统调用通知内核提交 SQ 中的 I/O 请求。
6. **获取 CQE：** 使用 `io_uring_wait()` 或 `io_uring_enter()` 系统调用等待 I/O 操作完成，并从 CQ 中获取 CQE。
7. **处理 CQE：** 检查 CQE 中的操作结果，并根据结果处理已完成的 I/O 操作。
8. **清理：** 在应用程序退出时，释放 io_uring 实例和相关的资源。

### 6. 性能优势

根据 [Efficient IO with io_uring](https://kernel.dk/io_uring.pdf)，io_uring 在轮询模式下可实现 170 万次 4KB IOPS，非轮询模式下为 120 万次，远超传统 aio 的 60.8 万次。对于空操作（no-op），io_uring 可达到每秒 1200 万至 2000 万次消息传递，展现其极高的吞吐量。

## 使用 io_uring

### 使用 liburing

虽然直接使用 io_uring 的低级接口可以提供最大灵活性，但 [liburing](https://github.com/axboe/liburing) 提供了一个更高层次的接口，简化了开发。liburing 被广泛应用于生产环境，例如 QEMU。开发者了解低级接口的工作原理，但在实际项目中优先使用 liburing。

### 示例：使用 liburing 读取文件

以下是一个使用 liburing 读取文件的简单 C 程序：

```c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <liburing.h> // 引入 liburing 库，简化 io_uring 的使用

#define BUFFER_SIZE 4096
#define QUEUE_DEPTH 128

int main() {
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    int fd;
    char *buffer;
    int ret;

    // 1. 创建 io_uring 实例
    ret = io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
    if (ret != 0) {
        perror("io_uring_queue_init");
        return 1;
    }

    // 2. 打开文件
    fd = open("test.txt", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // 3. 分配缓冲区
    buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        perror("malloc");
        return 1;
    }

    // 4. 准备 SQE
    sqe = io_uring_get_sqe(&ring); // 从 io_uring 实例中获取一个 SQE
    if (!sqe) {
        fprintf(stderr, "io_uring_get_sqe failed\n");
        return 1;
    }
    io_uring_prep_read(sqe, fd, buffer, BUFFER_SIZE, 0); // 设置 SQE 的参数，这里是读操作
    io_uring_sqe_set_data(sqe, buffer); // 将 buffer 设置为 SQE 的用户数据，方便在 CQE 中获取

    // 5. 提交 I/O 请求
    ret = io_uring_submit(&ring); // 提交 SQE
    if (ret < 0) {
        perror("io_uring_submit");
        return 1;
    }

    // 6. 获取 CQE
    ret = io_uring_wait_cqe(&ring, &cqe); // 等待 CQE
    if (ret < 0) {
        perror("io_uring_wait_cqe");
        return 1;
    }

    // 7. 处理 CQE
    if (cqe->res < 0) { // 检查操作结果
        fprintf(stderr, "Read error: %s\n", strerror(-cqe->res));
        return 1;
    } else {
        printf("Read %d bytes\n", cqe->res);
        printf("Content: %.*s\n", cqe->res, buffer); // 打印读取的内容
    }
    io_uring_cqe_seen(&ring, cqe); // 告诉 io_uring 已经处理过该 CQE

    // 8. 清理
    close(fd);
    free(buffer);
    io_uring_queue_exit(&ring);

    return 0;
}

```

### 实际应用

类似的方法已被数据库系统采用。例如，[PostgreSQL](https://www.phoronix.com/news/PostgreSQL-Lands-IO_uring) 已于 2025 年 3 月合并了对 io_uring 的初步支持，通过设置 `io_method=io_uring` 启用。此外，QuestDB 也在探索 io_uring 用于数据摄取。

## 注意事项

- **复杂性**：io_uring 的低级接口较为复杂，建议使用 liburing 简化开发。
- **安全性**：2023 年 Google 报告指出，io_uring 存在安全漏洞，占其漏洞赏金计划的 60%。因此，需确保使用最新内核版本并谨慎配置。
- **兼容性**：io_uring 仅在 Linux 5.1 及以上版本可用，需检查系统支持。

## 结论

io_uring 是一种革命性的异步 I/O 接口，为高性能存储引擎和数据库提供了强大的工具。通过共享环形缓冲区和固定缓冲区模式，io_uring 显著提高了 I/O 效率。开发者可以通过 liburing 快速上手，并利用其高级特性构建高效的存储系统。

## 关键引用

- [What is io_uring? — Lord of the io_uring documentation](https://unixism.net/loti/what_is_io_uring.html)
- [Efficient IO with io_uring](https://kernel.dk/io_uring.pdf)
- [io_uring by example: Part 1 – Introduction](https://unixism.net/2020/04/io-uring-by-example-part-1-introduction/)
- [io_uring by example: Part 2 – Queuing multiple requests](https://unixism.net/2020/04/io-uring-by-example-part-2-queuing-multiple-requests/)
- [ubdsrv GitHub repository](https://github.com/ming1/ubdsrv)
- [Building a high-performance database buffer pool in Zig](https://gavinray97.github.io/blog/io-uring-fixed-bufferpool-zig)
- [PostgreSQL Lands IO_uring Support](https://www.phoronix.com/news/PostgreSQL-Lands-IO_uring)