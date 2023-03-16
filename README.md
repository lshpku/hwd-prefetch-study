# HWD Prefetch Study
基于SiFive Inclusive Cache的硬件数据预取综合研究

### 项目内容
* 阅读Chipyard源码，写了内容详尽的[Chipyard代码导读](https://lshpku.github.io/hwd-prefetch-study/Chipyard代码导读.pdf)
* 在SiFive Inclusive Cache上实现了通用的预取框架，并且提供可以运行时配置的MMIO接口和性能计数器
* 正确实现了3个经典的预取器：Next-Line，BOP，SPP
* 使用SPEC2006进行性能评测，并分析了预取的准确率、覆盖率、及时性等指标

## 使用方法

### 增加访存延迟
由于FPGA的核心频率较低（50MHz），甚至低于DDR3内存（800MHz），导致访存周期数偏小，与真实处理器不符；为此我写了一个位于System Bus和L2 Cache之间的小模块，通过一个大小为8的buffer和一个宽度为100的shift register高效地实现了100个周期的延迟。

* 见`Makefile`，把`BankedL2Params.scala`映射到Chipyard镜像的
  ```text
  /root/chipyard/generators/rocket-chip/src/main/scala/subsystem/BankedL2Params.scala
  ```
* 不需要改其他地方，然后`make`就可以了

### 使用带预取功能的L2
* 先把整个仓库完整clone下来
  ```bash
  $ cd l2proj
  $ git submodule update --init --recursive
  ```
* 见`Makefile`，把`sifive-cache/design`映射到Chipyard镜像的
  ```text
  /root/chipyard/generators/sifive-cache/design
  ```
* 不需要改其他地方，然后`make`就可以了

### 测试访存延迟
我提供了一个简单的随机访存的C程序，用于探究数组大小与平均访存延迟的关系；它会输出数组大小和总访存周期，你可以用Python脚本分析结果。
* 编译
  ```bash
  $ riscv64-unknown-linux-gnu-gcc -Os -o test_latency example/test_latency.c
  ```
* 在FGPA上运行
  ```bash
  $ ./test_latency
  # begin
  # {"loop":625000,"n":1024,"cycle":35176796}
  # {"loop":500000,"n":1280,"cycle":53220544}
  # {"loop":416666,"n":1536,"cycle":35102682}
  # ...
  ```

### L2CTL
我的预取器提供了直接内存控制（MMIO）接口，即可以通过修改某处**物理内存**的内容来选择预取器和设置参数。我提供了一个使用这个接口的C程序，它通过映射操作系统的`/dev/mem`文件访问物理内存，因此**需要root权限**。
* 编译
  ```bash
  $ riscv64-unknown-linux-gnu-gcc -Os -o l2ctl example/l2ctl.c
  ```
* 在FGPA上运行，请使用root或sudo
  ```bash
  $ l2ctl on        # 开启L2预取
  # prefetch on
  $ l2ctl off       # 关闭L2预取
  # prefetch off
  $ l2ctl config    # 查看L2配置
  # banks     : 1
  # ways      : 8
  # sets      : 1024
  # blockBytes: 64
  $ l2ctl status    # 查看L2预取性能性能计数器
  # prefetch  : off
  # trains    : 1823464953
  # trainHits : 468795141
  # preds     : 1148053842
  # predGrants: 743084171
  ```

## 实现原理
见[硬件数据预取.pdf](https://lshpku.github.io/hwd-prefetch-study/梁书豪-硬件数据预取.pdf)

## 实验结果
### 加速比
![speedup](https://lshpku.github.io/hwd-prefetch-study/speedup.svg)
### 准确率
![accuracy](https://lshpku.github.io/hwd-prefetch-study/accuracy.svg)
### 覆盖率
![coverage](https://lshpku.github.io/hwd-prefetch-study/coverage.svg)
### 及时性
![timeliness](https://lshpku.github.io/hwd-prefetch-study/timeliness.svg)
