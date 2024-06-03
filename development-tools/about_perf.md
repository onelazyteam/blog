Perf 是用来进行软件性能分析的工具，使用时加上-g选项，可以记录堆栈信息	。

PMU 单元（performance monitor unit）：PMU 允许软件针对某种硬件事件设置 counter，此后处理器便开始统计该事件的发生次数，当发生的次数超过 counter 内设置的值后，便产生中断。比如 cache miss 达到某个值后，PMU 便能产生相应的中断。捕获这些中断，便可以考察程序对这些硬件特性的利用效率了。

Tracepoint 是散落在内核源代码中的一些 hook，一旦使能，它们便可以在特定的代码被运行到时被触发，这一特性可以被各种 trace/debug 工具所使用。

假如您想知道在应用程序运行期间，内核内存管理模块的行为，便可以利用潜伏在 slab 分配器中的 tracepoint。当内核运行到这些 tracepoint 时，便会通知 perf。

Perf 将 tracepoint 产生的事件记录下来，生成报告，通过分析这些报告，调优人员便可以了解程序运行时期内核的种种细节，对性能症状作出更准确的诊断。

perf top，类似于 top，它能够实时显示占用 CPU 时钟最多的函数或者指令，因此可以用来查找热点函数，使用界面如下所示：

```sh
$ perf top
Samples: 833  of event 'cpu-clock', Event count (approx.): 97742399
Overhead  Shared Object       Symbol
   7.28%  perf                [.] 0x00000000001f78a4
   4.72%  [kernel]            [k] vsnprintf
   4.32%  [kernel]            [k] module_get_kallsym
   3.65%  [kernel]            [k] _raw_spin_unlock_irqrestore
...
```

perf list，perf list命令可以列出当前perf可用的事件：

```shell
cpu-cycles OR cycles                               [Hardware event]
instructions                                       [Hardware event]
cache-references                                   [Hardware event]
cache-misses                                       [Hardware event]
branch-instructions OR branches                    [Hardware event]
branch-misses                                      [Hardware event]
bus-cycles                                         [Hardware event]
stalled-cycles-frontend OR idle-cycles-frontend    [Hardware event]
stalled-cycles-backend OR idle-cycles-backend      [Hardware event]
ref-cycles                                         [Hardware event]

alignment-faults                                   [Software event]
bpf-output                                         [Software event]
context-switches OR cs                             [Software event]
cpu-clock                                          [Software event]
cpu-migrations OR migrations                       [Software event]
dummy                                              [Software event]
emulation-faults                                   [Software event]
major-faults                                       [Software event]
minor-faults                                       [Software event]
page-faults OR faults                              [Software event]
task-clock                                         [Software event]

msr/tsc/                                           [Kernel PMU event]

rNNN                                               [Raw hardware event descriptor]
cpu/t1=v1[,t2=v2,t3 ...]/modifier                  [Raw hardware event descriptor]
(see 'man perf-list' on how to encode it)

mem:<addr>[/len][:access]                          [Hardware breakpoint]
```

这些事件可以分为三类(在文章开始介绍perf工作原理的时候也说了):Hardware Event, Software event, Tracepoint event.

每个具体事件的含义在perf_event_open的man page中有说明：

- cpu-cycles：统计cpu周期数，cpu周期：指一条指令的操作时间。
- instructions： 机器指令数目
- cache-references： cache命中次数
- cache-misses： cache失效次数
- branch-instructions： 分支预测成功次数
- branch-misses： 分支预测失败次数
- alignment-faults： 统计内存对齐错误发生的次数， 当访问的非对齐的内存地址时，内核会进行处理，已保存不会发生问题，但会降低性能
- context-switches： 上下文切换次数，
- cpu-clock： cpu clock的统计，每个cpu都有一个高精度定时器
- task-clock ：cpu clock中有task运行的统计
- cpu-migrations：进程运行过程中从一个cpu迁移到另一cpu的次数
- page-faults： 页错误的统计
- major-faults：页错误，内存页已经被swap到硬盘上，需要I/O换回
- minor-faults ：页错误，内存页在物理内存中，只是没有和逻辑页进行映射

关于perf的一些常用命令：

| sub command   | 功能说明                                                     |
| ------------- | ------------------------------------------------------------ |
| annotate      | 读取perf.data(由perf record生成)显示注释信息，如果被分析的进程含义debug符号信息，则会显示汇编和对应的源码，否则只显示汇编代码 |
| archive       | 根据perf.data(由perf record生成)文件中的build-id将相关的目标文件打包，方便在其他机器分析 |
| bench         | perf提供的基准套件的通用框架，可以对当前系统的调度，IPC，内存访问进行性能评估 |
| buildid-cache | 管理build-id,管理对于的bin文件                               |
| buildid-list  | 列出perf.data中的所以buildids                                |
| data          | 把perf.data文件转换成其他格式                                |
| diff          | 读取多个perf.data文件，并给出差异分析                        |
| evlist        | 列出perf.data中采集的事件列表                                |
| kmem          | 分析内核内存的使用                                           |
| kvm           | 分析kvm虚拟机上的guest os                                    |
| **list**      | **列出当前系统支持的所有事件名,可分为三类：硬件事件、软件事件，检查点** |
| lock          | 分析内核中的锁信息，包括锁的争用情况，等待延迟等             |
| **record**    | **对程序运行过程中的事件进行分析和记录，并写入perf.data**    |
| **report**    | **读取perf.data(由perf record生成) 并显示分析结果**          |
| sched         | 针对调度器子系统的分析工具。                                 |
| **script**    | **读取perf.data(由perf record生成)，生成trace记录，供其他分析工具使用** |
| **stat**      | **对程序运行过程中的性能计数器进行统计**                     |
| test          | perf对当前软硬件平台进行健全性测试，可用此工具测试当前的软硬件平台是否能支持perf的所有功能。 |
| timechart     | 对record结果进行可视化分析输出，record命令需要加上timechart记录 |
| top           | 对系统的性能进行分析，类型top命令，当然可以对单个进程进行分析 |
| probe         | 用于定义动态检查点。                                         |
| trace         | 类似于strace，跟踪目标的系统调用，但开销比strace小           |

perf top --no-children --call-graph=fp -g --dsos=/usr/pgsql-12/bin/postgres指定binary

perf record -g -p 1667291(进程号)

perf record -g -t 1667291(线程号)  （perf单独线程）

perf record -e cpu-clock -g -p 4522

sudo perf record -a -e cpu-clock --call-graph dwarf -p 14998(推荐使用该方式，堆栈信息完整，但依赖libunwind，参考链接：

https://gaomf.cn/2019/10/30/perf_stack_traceback/

[https://edward852.github.io/post/%E4%BD%BF%E7%94%A8perf%E5%88%86%E6%9E%90%E6%80%A7%E8%83%BD%E7%93%B6%E9%A2%88/](https://edward852.github.io/post/使用perf分析性能瓶颈/))

利用flamegraph生成火焰图：

perf script | ./stackcollapse-perf.pl | ./flamegraph.pl > output.svg

参考链接：

http://walkerdu.com/2018/09/13/perf-event/#5-3-perf-top