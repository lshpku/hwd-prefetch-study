# 硬件数据预取论文阅读

## 简介
### 存储墙
* 处理器核心周期和访存延迟的差距不断扩大，形成存储墙（Memory Wall）
* 多级Cache一定程度解决了这个问题，但Cache非常依赖于局部性
* 多线程和SMT也可以解决存储墙对吞吐量的影响，但不能改善响应时间
### 预取
* 预取通过预测将要访问的地址，提前把数据取到Cache中，从而降低miss延迟
* 预取不能消除miss，因为miss是Cache的固有属性，无法消除
* 预取只是降低了miss的副作用之一——延迟，但仍然保留甚至加剧了miss的其他副作用，如功耗、带宽
* <b>硬件数据预取：</b>使用非体系结构可见的方式，由硬件自动生成预取请求
#### 图：预取在处理器体系结构中的位置
<p align="center"><img src="https://lshpku.github.io/hwd-prefetch-study/tree01.svg" width="700"></p>

### 分类
* <b>基于模式的：</b>假设访存的地址存在某种模式（Pattern），如空间（Spatial）局部性、时间（Temporal）局部性，那么只要在访存历史中识别出这一模式，就可以根据该模式推算接下来要访问的地址
* <b>基于执行的：</b>不靠历史去推测，而是通过额外的线程（Helper-thread）或OOO处理器推测式执行的能力（Runahead），在主线程访存之前算出访存地址并预取，从而达到快主线程一步预取的效果
* <b>基于特殊性质的：</b>针对特殊的访存，例如Pointer-chasing、散列表做预取；解决方法比较多元，融合了模式和执行的方法，故单独列为一类
#### 图：硬件数据预取的分类
<p align="center"><img src="https://lshpku.github.io/hwd-prefetch-study/tree02.svg" width="800"></p>

## Stream/Stride
* **定义**
  * 最简单的一种访存模式，假设访存序列是一个连续的流`{X, X+1, X+2, ...}`或跨步为`d`的序列`{X, X+d, X+2d, ...}`
  * 流或跨步其实也是空间局部性的一种，是一种最简单的空间局部性，只是比较特殊所以单独拿出来研究
* <b>优点：</b>流或跨步的模式很简单，而出现次数又特别多，所以对其做预取容易取得很好的效果
* **Next-Line** *自古有之*
  * 可以调整激进程度，比如Distance（就是后来的Offset，提前多久开始预取）和Degree（发出几个预取）
* **Stream Buffer** *ISCA’90*
  * 检测访存地址中的流，用一个Stream Buffer的结构做预取
  * 原文对流的形态做了严格限定，必须是连续的，而且每个Stream都要一个单独的Stream Buffer装载，所以适用范围有限
  * 后续论文有不少显而易见的改进，比如允许流偶尔不连续，多个流共享一个Stream Buffer等
* **Stride** *SC’91*
  * PC-localized地检测每个load指令访存序列的stride并预取
  * 原文其实考虑得非常周全，还考虑了lookahead和timeliness
* **Predictor-Directed Stream Buffers** *MICRO’00*
  * 把Stream Buffer和预取器（Stride、Markov）结合在一起，使得Stream Buffer可以预取非规则的流
  * 另外研究了Stream Buffer的分配和替换问题
* **Feedback-Directed Prefetching** *HPCA’07*
  * 通过监控预取的Accuracy、Lateness和Cache Pollution指标调整预取器的激进程度（即Distance和Degree）
  * 另外也可以调整预取数据在LRU中放置的位置（头或尾）
### Offset
* **定义**
  * 也是针对流或跨步的访存序列进行预取，但是并不去提取跨步`d`或检测有多少个流
  * 而是使用一个全局的Offset变量`D`，对于miss地址`X`，总是预取`X+D`
* **优点**
  * 结构更为简单，因为不需要为每个流分配一个Stream Buffer或每个PC分配一个Stride表项，很大程度节约存储空间
  * 可用于变化但有规律的stride，例如假设stride为`{d, 2d, d, 2d, ...}`，如果是传统的Stride预取器就会因为stride一直变化无法提取出stride，但Offset预取器可以取`D=3d`，达到100%的准确率和覆盖率
  * 甚至可用于无规律但是bias的stride，例如`{3d, -d, 2d, -d, 2d, d, -2d, ...}`，虽然没有明确的规律，但是可以发现地址大体上是在不断往前的，所以只要取一个合适的`D>0`，就可以达到较好的准确率和覆盖率
* **Sandbox** *HPCA’14*
  * 首次提出Offset预取
  * 提出Sandbox的概念，即把多个offset做出的预取结果放到Sandbox中但不发出，然后根据实际的访存统计哪个offset的命中率最高，把最高的这个offset作为接下来一段时间的offset
  * Sandbox其实有很多用途，不一定用在Offset预取上，它几乎可以用来对任意预取器的组合进行选择
* **Best Offset** *HPCA’16*
  * 以Timeliness为目标，因为作者认为精确但不及时的预取是压根没有用的
  * <b>原理：</b>找到一个Best Offset `D`，在访问`X`时预取`X+D`，使得从访问`X`到访问`X+D`这段时间足以让`X+D`被取回来
  * **缺点**
    * Timeliness高不代表Accuracy和Coverage高，实验表明Best Offset有时候不等于令Speedup最高的offset
    * 对于一个miss只发出一个预取（即degree=1）有点保守了，不利于Coverage
* **Multi-Lookahead Offset Prefetching** *ISCA-DPC3’19*
  * 为了解决BOP的Coverage低的问题
  * 考虑多个Lookahead（Lookahead表示提前几个访存发出预取），每个Lookahead上都算出Best Offset，访问`X`时，对于每个Lookahead上的Best Offset `Di`都发出预取`X+Di`
  * Sandbox也可以实现同时发多个预取，但是原理完全不一样，Sandbox即使发出多个预取也都是lookahead=1的，而MLOP的多个预取有不同的lookahead，在提高Coverage的同时也保证了Timeliness
* **Berti** *MICRO’22*
  * 感觉这篇文章的创新不多，就是把Offset预取和其他一些常见的提高预取效果的方法结合在一起
  * 一是把预取器放在L1上，使用PC-local的表项和虚拟地址，其他预取器都是放在L2上；显然放在L1的信息更多，PC-local也比global的可预测性更强
  * 二是可以选择预取到哪一层Cache，在减少对L1的污染的同时提高Coverage

## Spatial
* **定义**
  * 假设访存的模式在空间上重复出现，即如果曾经在`X`附近访问了`{X, X+A, X+B, ...}`，那么也会在`Y`附近访问`{Y, Y+A, Y+B, ...}`
  * Spatial利用的是数据结构的重复性（Pattern局部性），因为同一个数据结构在内存各处都具有相同的访问模式
* 根据**信息记录**的方式，Spatial可以分为Region/Footprint和Delta两种
### Region/Footprint
* **定义**
  * 把内存均匀划分为多个Region，例如每1K为一个Region，或者一页4K为一个Region，以Region为单位进行预取
  * 初始时记录第一个Region内被访问过的行，以bit-vector形式记录`10110...`，称为这个Region的Footprint；由于一个Region不会很大，所以这个bit-vector不会很长，例如64
  * 当访问到下一个Region后，把之前的Footprint应用到新的Region中，也就是预取新的Region中那些被bit-vector标记为`1`的行
  * 一个程序中可能存在多种不同的Footprint，这时涉及到Region的Trigger Event的问题，不同的Trigger Event会让这个Region应用不同的Footprint
* **优点**
  * 节省空间，因为bit-vector极大地压缩了一个Region中的访问模式
  * 可以容忍乱序访问，因为bit-vector不考虑其中的bit添加的顺序，使用不同顺序访问一个Region得到的Footprint是一样的
* **缺点**
  * Timeliness不好，因为每个Region都要在遇到Trigger Event之后才能预取，而且由于bit-vector不区分顺序，不知道先取哪一行，只能一次性全部发出
  * 切分Region这件事假设了数据结构和Region是对齐的，但其实可能并不对齐，导致同一个数据结构出现多个Footprint
* **SFP** *ISCA’98*：首个提出用Region/Footprint进行Spatial预取
* **SMS** *ISCA’06*
  * 在SFP基础上对Trigger Event索引、Footprint记录方式做了优化，说实话SFP写得很混乱，这篇则写得明白得多
  * 这篇文章最大区别是用PC+Region Offset去索引，而不是PC+Address，这样可扩展性大大提高了
* **AMPM** *ICS’09*
  * 复杂的在Region历史中检测Stride的逻辑，可以同时检测多个Stride，且可容忍乱序访存
  * 这篇文章其实划到Spatial里不是很合适，它虽然用了Spatial的技术，但它只能检测Stride，
  * 但是划到Spatial里也有一定道理，因为Spatial往大了看也是一个Stride预取器，只是Stride很大，大到Region级别
* **Bingo** *HPCA’19*
  * 同时用PC+Address和PC+Offset做索引，解决PC+Address精度高但覆盖率低，和PC+Offset覆盖率高但精度低的问题，类似于TAGE
  * 为了节约存储空间，原文巧妙地只用一张表，用PC+Offset做下标，用PC+Address做tag，单表实现TAGE的效果
* **DSPatch** *MICRO’19*
  * 同时训练针对Coverage和Accuracy训练两个Footprint，即用OR和AND累加同一个Trigger Event的Region的Footprint；监控带宽，在带宽充足时用Coverage的Footprint，否则用Accuracy的
  * 对Footprint做rotate，使得第一个bit总是Trigger Access，解决不对齐的问题
### Delta
* **定义**
  * 考察访存的Delta之间的关系，例如`{+A, -B, +C, +A, ...}`，认为Delta的历史可以预测出下一个Delta，例如有理由认为`{+A, -B}`的下一个Delta是`+C`
  * 又叫做Delta Correlation，即挖掘Delta之间的关系
* **优点**
  * 不受数据结构对齐的限制，因为Delta本身和对齐无关
  * 精度更高，因为可以很方便地通过延长Delta历史的方式获得更高的置信度
  * 适合通过Lookahead提高Timeliness，因为Delta的预测可以自递归，预测了一个Delta再预测下一个Delta
* **缺点**
  * 无法容忍乱序访存，稍微有点乱序都会让Delta无法得到足够的置信度
  * 假设Delta之间存在强关系这个出发点本身就有问题，Delta之间常常只有弱关系，比如`{+1, +2, -1, +2, +1, -2, ...}`，这里Delta是正bias的，即它们大致是向前的，但并没有确定的关联性
  * Delta的数学形式很优美，但只有这两篇文章；虽然16年的SPP至今仍然经常被引用，但Delta本身就再没人研究了，说明它的适用性比较窄，SPP已经达到完美水平了
* **VLDP** *MICRO’15*
  * 首篇提出使用Delta Correlation做Spatial预取
  * 记录了最近的4个Delta历史，像TAGE一样有4张表，分别用不同长度的Delta历史去访问（Delta历史并不做hash）
  * 使用固定的lookahead=4，但更新精确度时只更新第一个预取
* **SPP** *MICRO’16*
  * 把Delta历史做hash，不管多长的历史都只有一个hash值，所以只用一张表
  * 记录每个Delta的准确率，在Lookahead时把准确率累乘，低于阈值就停止，平衡Lookahead和Accuracy
  * 作为在L2上的预取器，对跨页时的预热问题做了优化
### 综合
* **IPCP** *ISCA’20*
  * 论述了在L1上做Spatial预取的好处，发现把SMS、VLDP、SPP、Bingo、DSPatch这些原本用于L2的搬到L1会得到好处，但是也要注意L1存储空间的限制
  * 发现不同的IP的访存行为不同，不应该混在一起，所以用了PC-local的Stride，和类似于SPP的Delta预取
  * 发现全局访存有时可以得到PC-local不具备的信息，比如同一个循环中不同的load，单看分别是`{1, 3, 6}`、`{2, 4, 5}`，但合在一起是`{1, 2, 3, 4, 5, 6}`，所以在global上用了Region预取
### 总结
* **优点**
  * 空间局部性和Pattern局部性是程序中非常常见的性质，所以Spatial预取有效性很高
  * 相比Temporal比较节约空间，因为无论是Footprint还是Delta都是高度压缩的Pattern，是对程序访存行为的抽象
  * 可以解决义务miss，因为可以很方便地把过去学到的Pattern应用到新的地址上
* **缺点**
  * Spatial虽然用了比Offset复杂的策略，但效果常常和Offset差不多，因为Spatial往大了看不过是多个Offset的组合
    * 例如对于一组Delta`{+4, -2, +1}`，它其实就是起点分别为`{0, 4, 2}`的`D=3`的3个Offset组合
    * Spatial只有在Pattern*相当*复杂的时候才能超过Offset
  * 仍然有很大的局限性，一旦访存不满足Spatial性质就无法发挥作用（或退化为Next-Line）

## Temporal
* **定义**
  * 以某个顺序访问过的地址还会以相同的顺序再次访问，即假设曾经访问过`{A, B, C, ...}`，则之后还会再访问`{A, B, C, ...}`
  * 注意和Spatial不同的是，这里重复的地址是绝对地址，Spatial里面重复的只是相对地址（Offset或Delta）
* 按**信息记录**的方式可以分为关系型（Correlation）和流型（Streaming）
### Correlation
* <b>定义：</b>记录的是时间上相邻的访存地址之间的关系，例如对于访存序列`{A, B, C}`，它会记录`A->B`、`B->C`
* **Markov** *ISCA’97*
  * 用Markov概率建模访存地址，即假设访存序列是`{A, B, C, ..., B, D, ...}`，就可以得出`A--100%->B`、`B--50%->C`、`B--50%->D`的关系
  * 自然地，可以通过转移的概率控制Degree和Lookahead
* <b>优点：</b>数学形式优美，理论上有很高的Accuracy和Coverage
* **缺点**
  * 查询延迟高，每个转移都要查一次；由于Markov表很大（见总结），查询延迟会和访存本身一样长，所以基本没有Timeliness可言
  * 其实Markov有很多后续文章，但是由于可行性堪忧，后面就没有人用它来做数据预取了，倒是有不少人拿它来做指令预取
### Streaming
* <b>定义：</b>直接把访存序列原封不动地记下来，例如`{A, B, C, D, ...}`，下次访问到`A`时就知道接下来应该访问`{B, C, D, ...}`
* **优点**
  * 查询速度快，可以按batch读历史访问序列，一次读上来多个地址
  * 相邻地址之间可能有比较简单的关系，适合做压缩
* <b>缺点：</b>缺乏一种概率机制，无法描述多个后继和多种路径的情况
* **GHB** *HPCA’04*
  * 首个提出Temporal Streaming数据结构，即index表+history表，但没说怎么实现
  * 为了像Markov一样支持多个后继，它的history表除了FIFO结构外还多了一个链表指针，每个地址指向上次出现的同一个地址，这样查到`A`的最近一次出现就可以知道`A`的上一次、上上一次、…出现在什么地方，因此GHB理论上是可以做像Markov一样的按概率预取的
  * 除了用Address做index，还提出可以用Delta做index，这时history表中的指针含义也发生变化，变为指向上次同一个Delta出现的地址
* **TSE** *ISCA’05*
  * 强调在多核环境上进行Temporal Streaming预取，并给出了实现
  * 抛弃了Markov的模型，不再考虑多个后继的情况，而是总是index到最近一次出现的位置，即压根不考虑多个后继
  * 其实这篇文章才是真正的Temporal Streaming的提出者，因为它完全用一维的Streaming看待访存序列，而不是多维的Markov模型
* **STMS** *HPCA’09*
  * 把index表放在内存上，使用bucketized probabilistic hash table的数据结构，降低访问延迟
  * 提出概率更新的方法，不是每次访存都更新，而是按概率更新其中一部分，实验表明并没有损失Coverage
* **ISB** *MICRO’13*
  * 首个支持PC和Address联合索引的Temporal Streaming预取器，注意PC+Address并不平凡，因为GHB是天生是global的
  * 提出structural地址的概念，和物理地址相对，用P2S和S2P两张表互相转换structural地址和物理地址，两张表充当了GHB的作用，不再需要额外的GHB
    * 训练时将同一个PC的连续访存映射到连续的structural地址，预测时再将structural地址转换回物理地址
  * 将P2S表的加载和TLB PTW同步，用PTW的长延迟掩盖P2S访问的延迟
* **Domino** *HPCA’18*
  * 为了解决Address索引不精确的问题，提出使用两个连续地址作为index
  * 为了让一个地址或两个地址都能索引，而且不用索引两次，提出一种只用索引一次的结构
* **MISB** *ISCA’19*：ISB的改进版，更细粒度的meta管理，meta预取
* **Triage** *MICRO’19*：大幅缩减meta大小，不再需要放在内存里，而是放在LLC里
### 综合
* **STeMS** *ISCA’09*：在粗粒度上使用Temporal，在细粒度上使用Spatial，因为细粒度上的Pattern常常是重复的，可以提高存储效率
### 总结
* **优点**
  * 可以预测无规律的访存序列，比如pointer chasing，而且可以做到非常及时，这是无可比拟的优点
* **缺点**
  * 太占空间，每个转移地址都要一个条目，典型的Temporal表大小为64MB
    * 用预取器不正是因为Cache太小装不下所有地址，如果要让记下这些地址，就需要比Cache还大的空间
  * 大多Temporal的实现都把表放在内存上，确实现在的机器都不缺内存，64MB不算什么，但是把表项在处理器和内存之间交换也有带宽和能耗成本
  * 无法处理义务miss，因为Temporal要求miss至少出现一次才能预测

## Pointer
* **定义**
  * 此类预取器专门为Pointer而设计，用的方法比较多元，融合了Correlation、值预测、Precomputation等思想，所以单独列为一类
* **Jump Pointer** *ISCA’99*
  * Jump Pointer是一种特殊的链表结构，一般的链表是`A->B->C->...`，而Jump Pointer同时记录了往后几跳的指针，变为`A->BC, B->CD, C->...`，跳数可以根据访存延迟配置
  * Jump Pointer本身是一个软件方案，但本文也讨论了硬件实现的方法，提出可以存在内存上，这样倒是有点Temporal的感觉了
* **Pointer Cache** *MICRO’02*
  * 识别内容为pointer的load并记录在一个小cache中，下次访问的时候直接从小cache拿，其实有点像值预测
  * 为了尽可能只存关键的pointer，只存储指向堆的pointer，因为这些更有可能是链表指针
* **Content-Directed Data Prefetching** *ASPLOS’02*
  * 在LLC中识别cache line中的指针并预取，识别方法为把每个dword的值和上面发下来的地址比较，然后在LLC发起预取，省了上层cache的延迟
  * 问题是识别的精度很差，只能识别原始指针，不能识别修改过的指针
* **IMP** *MICRO’15*：从访存地址流中识别`A[B[i]]`的模式，通过预取B来预取A
* **优点**
  * 对于指针类的应用还是很有帮助的，用最小的代价实现了最好的功能
  * 虽然这些论文都很老了，但近期有一些做图应用预取的也在用类似的方法，说明它们并没有过时，只是适用范围太窄了

## Runahead
* **定义**
  * 流水线遇到长延迟的load miss时会堵塞commit，导致ROB满发生停顿，此时流水线中许多部件是空闲的
  * Runahead就是利用流水线停顿时的空闲算力，继续推测式执行后续指令，尤其是计算出后续load的地址，把这些load作为预取地址发送出去
* **关键问题**
  * 何时开始Runahead：Runahead是和正常执行不一样的状态，有切换代价，只有在Runahead的收益大于切换开销时才开始Runahead
  * 如何进入Runahead：Runahead会破坏流水线中的状态，需要在进入Runahead时保存关键信息
  * 如何利用流水线资源：Runahead过程中并非所有流水线资源都可用，例如ROB就是满的，如何在资源缺乏的情况下仍然能执行
  * 如何处理推测：Runahead是推测式执行的过程，不应改变体系结构状态，如何处理推测过程中的数据与控制
  * 如何退出Runahead：如何恢复到进入Runahead之前的状态
* **Improving Data Cache Performance by Pre-executing Instructions Under a Cache Miss** *ICS’97*
  * 一篇小小的论文，惊为天人地提出了近乎完整的Runahead的概念，和今天的Runahead大体相同
  * 不过面向的是in-order的五级流水线；实验非常水，只模拟了单周期处理器
* **Runahead Execution** *HPCA’03*：首篇讨论O3下Runahead的实现和性能
* **Techniques for Efficient Processing in Runahead Execution Engines** *ISCA’05*
  * 在前一篇的基础上提出了一些改进方案
  * 避免不必要的Runahead：Runahead模式切换有很大成本，如果一次Runahead发出的预取有限，Runahead的收益就会小于成本，就不应该触发这次Runahead；使用3种硬件和1种软件方法判断是否应该触发Runahead
  * 提高Runahead过程中找到的L2 miss数：减少不必要的指令（浮点），立即发射INV指令（因为INV指令的依赖也是INV的）
* **Filtered Runahead Execution with a Runahead Buffer** *MICRO’15*
  * 发现大多数miss是由少部分重复的短的dependence chain生成的，其他指令都没有用
  * 使用一个Runahead Buffer存储当前stalling load的slice并执行，在一个Runahead周期内重复执行同一个slice
  * 另外在功耗上有个好处，就是Runahead Buffer可以代替前端，降低Runahead过程的功耗
* **Continuous Runahead: Transparent Hardware Acceleration for Memory Intensive Workloads** *MICRO’16*
  * 认为core上的Runahead只能在stall的时候进行，间隔和时长都太短了，限制了预取能力
  * 在内存控制器上弄了个Runahead引擎（CRE），core像Runahead Buffer一样识别load slice，识别完后发给CRE执行
  * CRE的好处是可以时刻运行，提高Coverage（13%→70%）；比较简单；可以多核共享，避免竞争；靠近内存，访存延迟低
* **Precise Runahead Execution** *HPCA’20*
  * 认为过去的Runahead在退出时清空流水线代价太大；提出用进入Runahead时空闲的寄存器和IQ资源来Runahead，不影响原有的状态；结束时只需清除进入Runahead后才取的指令，之前取的（即已经在ROB中的）不需清空
  * 认为Runahead Buffer每次只运行一个load slice，Coverage不足；提出一个统一的slice buffer，不再指令是哪个slice，只要属于任意一个slice就可以执行
* **Vector Runahead** *ISCA’21*
  * 发现Runahead通常在执行一个循环，循环中的第一层访存通常是strided的，如果一次一个load地执行会比较浪费
  * 在普通Runahead模式的基础上新增一个Vector模式，在发现strided load后，进入Vector模式，将该load和依赖于它的指令全部向量化
  * Vector模式在遇到strided load的下一个动态实例时结束；本文没有找load slice，但这个想法很像load slice，不仅起到了结束循环的作用，还不用特意去找slice
  * 另有Vector Unrolling和Vector Pipelining两个优化，允许一个Runahead周期内执行多轮Vector
  * 为了让Runahead进行得更充分，放宽了Runahead结束条件；若stalling load返回就结束，则其他的miss必定还没有返回，导致只能预取一层；故放宽结束条件，可以预取多层dependent的miss
* **优点**
  * 可以处理无依赖的无规律的访存序列，例如hash array；由于使用程序自己的代码进行预取，Accuracy极高（95%）
  * 可以处理义务miss
  * 不会占用太多的存储空间，只需要修改逻辑
  * 可以同时应用于预取和转移预测
* **缺点**
  * 能耗高，因为Runahead期间的指令都要重新执行，相当于白执行了很多指令；今天的处理器即使是桌面级或服务器级也需要考虑功耗墙的问题，这个缺点很致命
  * 逻辑复杂，为了保证流水线功能正确需要添加大量逻辑，因为它是和流水线紧耦合的，不像其他和流水线分离的方法可以比较自由地设计，这给设计带来很大负担
  * Timeliness有限，因为它只是在指令级并行这个级别上做预测，最多只能达到百条指令级别的Lookahead；另外由于它的预测是对L1发出的，而L1的MSHR数量通常很小，所以也限制了Lookahead
  * 只能生成源数据都在片上的miss，如果源数据不在片上，就无法在Runahead期间计算出miss地址；故可以说Runahead针对的是independent cache miss，而不是dependent cache miss

## Precomputation/Helper-thread
* **定义**
  * 利用空余的SMT资源或额外的硬件运行一个helper-thread，由该helper-thread发起预取
  * 和Runahead的区别是，Precomputation用的是另外的硬件线程资源，而不是流水线本身；另外Precomputation和主线程是可以同时运行的，不像Runahead只能在主线程停顿的时候才能运行
* **Dependence Based Prefetching** *ASPLOS’98*
  * 硬件识别相互依赖的load并提前执行
* **Understanding the Backward Slices of Performance Degrading Instructions** *ISCA'00*
  * 提出可以通过提前执行slice来给主线程提供预测的方法，但没说怎么实现，只是在trace上研究了slice的特征，提出缩小slice的方法；那个年代的人认为只要缩小slice就有很大提升
  * 将slice分为4种sub-slice：value、address、existence、control flow，用一个FSM来回溯找slice
  * 缩小slice的关键是利用speculation，虽然越完整的slice准确率越高，但只要很小的slice就能达到不错的准确率，因为大部分branch和load-store dependence都是稳定的
* **Slipstream Processors** *SIGPLAN'01*
  * 构造一个比原始程序（R-Stream）短的程序（A-Stream），并行地和原始程序在SMT或多处理器上跑，A-Stream比R-Stream跑得快，给R-Stream提供预测；R-Stream也要验证A-Stream，纠正错误的预测
  * A-Stream是可以自动找的，开始的时候A-Stream就是R-Stream，它用硬件在运行中检测无效写、重复写、跳转结果确定的branch和它们的后向slice，把这些指令去掉就得到了A-Stream
  * 找slice的方法也是用寄存器依赖关系，不过是在commit时保留一段trace，然后在trace里面找，如果超出trace的长度就算了
  * 其实目的不只有加速，还可以做错误容忍；而且是否开A-Stream是可以由操作系统控制的，所以可以对SMT进行更灵活的使用
* **Execution-based Prediction Using Speculative Slices** *ISCA'01*
  * 针对load和branch，它发现程序里面出问题的指令集中在少数几条，而这些指令都是在loop里面的，所以可以提取slice来提前执行
  * slice的定义就是计算load和branch的必要指令，slice用硬件存在前端，不过这篇文章并没有说怎么找slice，只能手动找
  * 用SMT的另一个线程来执行slice，在遇到一个fork point的时候分出新线程，还要拷贝live-in寄存器给新线程
  * branch的结果通过queue同步给前端，同步机制非常复杂，要考虑预测错、延迟的情况；load的结果直接放在cache里面
* **Data prefetching by dependence graph precomputation** *ISCA’01*
  * 提取地址依赖图并提前计算
* **Speculative precomputation: Long-range prefetching of delinquent loads** *ISCA’01*
  * 软件提取关键load的slice并用SMT提前计算
* **Dynamic Speculative Precomputation** *MICRO’01*
  * 硬件实现的Speculative Precomputation
* **B-Fetch** *MICRO’14*
  * 借助转移预测来预测load地址，只能预测Stride的load地址
  * 不是整个流水线Runahead，只是用一个小组件，解决了能耗问题
* **Branch Runahead** *MICRO’21*
  * 基本的想法中规中矩，就是提取Branch Slice，然后在一个独立的DCE引擎上循环执行，把预测结果同步给core；特点是如何提取Branch Slice和何时执行slice
  * 提取Branch Slice：开一个512的retire queue，记录已经retire的指令；遇到预测错误的难预测的branch retire时，就在这个queue上反向搜索，直到找到另一个相同PC的指令或Guard/Affector Branch
  * 何时执行slice：当branch预测错误时，去slice表里查询，若命中就执行；当一个slice运行完后，用结束的PC再去触发下一个slice
  * Guard/Affector Branch：导致diverge的branch，它可以影响到后续的branch，所以被当做触发条件；找法很复杂，找merge point->找Guard，找dest set->找Affector
* **优点**
  * 和Runahead有些类似，可以处理无规律的访存，可以处理义务miss
  * 比Runahead的逻辑简单，因为是和流水线松耦合的
  * Timeliness比Runahead好，因为Precomputation线程可以和主线程同时运行，不用等到主线程停顿
* **缺点**
  * 对于大多数应用，很难得到一个高效的helper-thread，因为helper-thread本身就有很多先验在里面，纯硬件实现更加困难，所以目前的论文都限制在load slice上，无法处理更复杂的依赖关系
  * 依赖关系是需要存储空间的，而且很占空间

## Value Prediction
* **定义**
  * 不预测load的地址，而是预测load的值，利用OOO处理器的乱序执行能力，即使预测错了也没关系
  * 一些方法也预测地址，但总之效果是比老老实实访问Cache更早拿到值，所以归为值预测
* **Load Value Prediction via Path-based Address Prediction** *MICRO’17*
  * 针对Stride的load，在fetch阶段预测load的地址，提前访问L1，在rename阶段拿到值，在memory阶段重新访存并验证
* **Register File Prefetching** *ISCA’22*
  * 也是针对Stride的load，在rename阶段访问L1，尽可能早地拿到值，之后无需再访问cache
  * 这篇文章的目的是解决x86处理器L1访存延迟过长的问题，因为L1访存延迟虽然比下层cache短，但L1的访问特别多，所以即使是缩小一些hit latency也能产生很大的影响
  * 比DLVP好的是不用访问两次cache，不过它的流水线结构也不一样，它是先iss再rrd，DLVP是先rrd再iss
* **优点**
  * 和其他预取方法正交，解决的是流水线一级的miss penalty（或者叫hit latency），看似每个latency很小，但合在一起的作用很大
* **缺点**
  * 并不能预测复杂的情况，无论是直接预测值还是预测地址都局限于固定Delta的情况
  * 不是所有处理器能从中获益，只有像x86这样连访问L1都要4个周期的处理器才能有明显好处，对于本来L1延迟就低的处理器没什么作用

## Prefetch Filtering/Throttling
* **定义**
  * 一种独辟蹊径的和预取“对抗”的方法，本质是保留预取好的地方而避免预取带来的负面影响
  * 实现上有多种方法，例如把预取器发出来的请求过滤掉一部分，或者直接暂停预取
  * 虽然很多预取器的设计里面都含有Throttling的思想，但作为一个单独的模块去设计Prefetch Filtering/Throttling可以做得更好
* **Perceptron-Based Prefetch Filtering** *ISCA’19*
  * 用Perception方法过滤掉不精确的预取，使预取器可以自由发挥，预取器提高Coverage，过滤器保证Accuracy
  * 原文用的预取器是SPP，因为有了Filter所以取消掉了SPP原有的阈值
* **缺点**
  * 这种方法确实比较独特，做的人少，缺乏样本对比，不知道真实有效性怎么样
  * 而且上来直接就用Perception，说明传统的方法难以应付，但既然都用Perception了为什么不直接用Perception的预取器

## Prefetching-Aware Replacement
* **定义**
  * 替换策略对预取的数据区别对待，例如将其放到LRU的队尾，因为预取的数据和义务访存不一样，总之希望实现更小的miss
  * 其实预取和替换都是Cache管理的一部分，只要预取结果是放在Cache里的，它们联合起来做就理应有更好的效果
* **PACMan** *MICRO’11*
  * 针对LLC，检测预取数据的re-reference interval，决定插入到LRU的头还是尾
* **Kill the Program Counter** *ASPLOS’17*
  * 联合预取和替换，两者互有沟通，预取器使用SPP，替换策略使用基于re-reference prediction value (RRPV)的策略
  * 预取器的预测会训练替换策略的全局计数器，替换策略会提供预取器的level阈值
  * 不需要PC（正如题目所说），因为原文是在L2和LLC上做，一方面PC很难传下来，另一方面大部分的访存都被L1过滤了，L2和LLC即使知道PC也分析不出什么
* **Demand-MIN & Harmony** *ISCA’18*
  * 发现MIN替换策略只能实现最小的miss，但不能实现最小的义务miss，提出Demand-MIN的理想替换策略
  * 提出一个Demand-MIN的硬件实现，称作Harmony

## Machine Learning
* **Semantic Locality and Context-based Prefetching Using Reinforcement Learning** *ISCA’15*
* **Learning Memory Access Patterns** *ICML’18*
  * 首篇使用LSTM预测访存地址，使用类似于NLP的评价标准，使用离线学习，且不考虑真实Accuracy
  * 为了减少输出空间，输出的是Delta而不是地址，只允许一定范围内的Delta，导致无法预测全局地址
* **A Neural Network Prefetcher for Arbitrary Memory Access Patterns** *TACO’19*
* **A Hierarchical Neural Model of Data Prefetching** *ASPLOS’21*
  * 通过LSTM在线预测访存地址，在模拟器上得到了很好的效果，比目前所有的硬件预取器都要好，证明LSTM的推测能力是Spatial和Temporal的综合
  * 使用两个LSTM，分别预测ppn和page offset，故名Hierarchical，事实证明不仅预测效果好，而且可以预测全局地址
  * 使用的空间比Temporal少但效果还比Temporal好，说明Temporal目前的空间使用存在大量浪费，而深度学习模型可以很好地压缩信息以节省空间
  * 虽然它的模型无法在硬件上实现，但它给出了基于历史的预取器的上限，证明我们对历史信息的利用还是很不充分的
* **Pythia: A Customizable Hardware Prefetching Framework Using Online Reinforcement Learning** *MICRO’21*
  * 提出一个可实现的在线的强化学习算法，输入程序运行时的特征（控制流、地址流等），输出预取的Delta（限一页，即±63之间）
  * 使用强化学习的原因是希望考虑多种因素，而不是像传统的预取方法一样只能考虑简单的Address序列

## Cache Bypassing
* **定义**
  * 现有多级Cache结构虽然可以较大程度地利用时间局部性（最近使用的块在Cache的上层），但多级Cache的hit time也会在整个访存延迟中占很大的部分
  * Cache Bypassing通过预测数据的位置（在第几级Cache或内存），直接向对应的层发请求，跳过miss的层，节约在miss的层上查询的时间
* **Reducing Load Latency with Cache Level Prediction** *HPCA’22*
  * 白兔
* **Hermes: Accelerating Long-Latency Load Requests via Perceptron-Based Off-Chip Load Prediction** *MICRO’22*
  * 使用Perceptron的方法在L1处进行二元判断，是在任一级Cache上还是在内存上，如果在内存上就并行查询Cache和内存，否则只查Cache
  * Perceptron的好处的预测能力很强，所以只用很小的存储空间，

## Software Prefetching
* **Classifying Memory Access Patterns for Prefetching** *ASPLOS’20*
  * 把常见的访存序列分为6种
    * <b>Constant：</b>总是访问同一个地址，即访问固定的对象，例如`*ptr`
    * <b>Delta：</b>以固定的间隔访问一串地址，即数组遍历，例如`A[i]`
    * <b>Pointer Chase：</b>指针的直接追逐，不带修改，例如`B=*A, C=*B`
    * <b>Indirect Delta：</b>把地址存在数组中，按顺序取出这些地址访问，例如`*(A[i])`
    * <b>Indirect Index：</b>把数组`A`的下标存在数组`B`中，按顺序从`B`取出这些下标，然后访问`A`，例如`A[B[i]]`
    * <b>Constant plus Offset Reference：</b>更加通用的指针追逐，指针取值时带偏移，典型例子是链表遍历，即`A=B->next`
  * 容易看出，Stride/Stream和Spatial预取顶多能处理Delta，一些Pointer专用预取器可以处理Indirect Delta和Index，其他只能交给不依赖Pattern的预取器，比如Temporal和Runahead
  * 剩下没分类的访存序列就更难处理了，所以硬件数据预取还有很多没有解决的问题
* **APT-GET: Profile-Guided Timely Software Prefetching** *EuroSys'22*
