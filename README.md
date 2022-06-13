L2 Project
===

这是我在SiFive Inclusive Cache（L2）上做实验存放测试文件的仓库

## 使用方法

### 在Memory Bus和L2之间添加延迟
* 见`Makefile`，把`BankedL2Params.scala`映射到Chipyard镜像的
  ```text
  /root/chipyard/generators/rocket-chip/src/main/scala/subsystem/BankedL2Params.scala
  ```
* 不需要改其他地方，然后`make`就可以了。

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
* 不需要改其他地方，然后`make`就可以了。

### 测试访存延迟
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
这是一个通过MMIO控制L2预取的小脚本
* 编译
  ```bash
  $ riscv64-unknown-linux-gnu-gcc -Os -o l2ctl example/l2ctl.c
  ```
* 在FGPA上运行，**需要root权限**
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
  $ l2ctl status    # 查看L2预取性能统计
  # prefetch  : off
  # trains    : 1823464953
  # trainHits : 468795141
  # preds     : 1148053842
  # predGrants: 743084171
  ```
