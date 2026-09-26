## 零、TODO
## 一、播放器原理
播放器线程：控制线程、播放线程、流读取线程。
播放线程：alsa pcm 硬件模式、MMAP 内存映射写入。
控制线程：通过 SPSC 原子队列与流线程通信。
流读取线程：使用 SPSC 原子缓冲区与播放线程连接，由此线程决定返回什么东西(空、指针、处理后数据)，这里此线程的链表保留并不修改 pcm 文件所有数据，直到手动销毁。
[注1]：因主流 PCM 编码均为有符号小端序，且计算机主要为小端序存储，故本播放器目前仅支持 S8、S16_LE、S24_LE、S24_3LE、S32_LE 。
[注2]：流处理线程与播放线程采用自指针，解决线程退出问题。
## 二、项目构成
```
src/
  spsc_atomic.cxx      -> 提供多个 SPSC 原子操作类
  main.cxx             -> 主文件，负责发送控制序列(SPSC 原子队列)
  pcm_stream.cxx       -> 负责 PCM 数据流的生成，并传送给 playback(SPSC 原子缓冲区)
  alsa_playback.cxx    -> 负责接受 stream，并把数据 hw 的 mmap 的提交给 alsa
  public.cxx           -> 几个文件公共使用到的数据: PCM 结构、消息队列、缓冲区
```
## 三、硬核知识
### 1.音量调节
pcm 采样点记录的是当前的振幅 $A_0$，为达到目标音量增益效果 $dB$，需要修改当前振幅为 $A_1$，则有
$$
\mathrm{dB}=20\lg\frac{A_1}{A_0}
$$
实际上 pcm 记录的数据是相对电平(分正负)，其唯一标准单位应该为相对满幅($\mathrm{dBFS}$)，若有当前大小 $point$ 与最大大小 $point_{max}$，则有
$$
\mathrm{dBFS}=20\lg\left|\frac{point}{point_{max}}\right|
$$
然而对于人耳而言，物理响度每增加 $10\ \mathrm{dB}$，人耳听觉响度翻倍，那么对于相对满幅 $\mathrm{dBFS}$ 与滑块比例 $ratio$ 有
$$
\mathrm{dBFS}=10\log_2{ratio}
$$
综上可得与 pcm 数据相乘的因数 $Q$ 与滑块比例 $ratio$ 的关系
$$
Q=x^\frac{1}{\lg4}
$$
### 2.淡入淡出
等功率曲线：
in  ->  $\sin p$
out ->  $\cos p$
