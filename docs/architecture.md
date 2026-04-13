# 高性能ADC数据采集与传输系统 - 架构设计文档

## 1. 系统概述

### 1.1 项目背景
本项目实现了一个高性能数据采集与传输系统，满足40kHz采样率、32位精度的ADC数据获取需求，并通过网络实时发送至上位机。

### 1.2 性能指标
| 指标 | 目标值 | 实测值 | 说明 |
|------|--------|--------|------|
| 采样率 | 40 kHz | 39,992 Hz | 每秒40,000个样本 |
| 分辨率 | 32 bits | 32 bits | 每个样本4字节 |
| 数据速率 | 156.25 KB/s | ~156 KB/s | 原始数据吞吐量 |
| 端到端延迟 | < 10 ms | < 1 ms | 采集到发送完成 |
| CPU占用 | < 30% | < 2% | 单核 |
| 采样率达成率 | > 95% | 99.98% | 实际/目标采样率 |
| 丢弃率 | < 1% | 0% | 数据完整性 |

## 2. 系统架构

### 2.1 三级流水线架构

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   ADC采集线程    │────▶│   数据处理线程   │────▶│   网络发送线程   │
│  (最高优先级)    │     │  (高优先级)      │     │  (中高优先级)    │
└─────────────────┘     └─────────────────┘     └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  采集→处理缓冲区  │     │ 处理→网络缓冲区  │     │    TCP客户端    │
│  (无锁环形缓冲)  │     │  (无锁环形缓冲)  │     │  (批量发送)     │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

### 2.2 线程职责

#### 2.2.1 采集线程 (THREAD_CAPTURE)
- **优先级**: TIME_CRITICAL (Windows) / SCHED_FIFO 99 (Linux)
- **职责**:
  - 与ADC硬件交互（模拟器中为定时生成数据）
  - 严格25μs采样周期控制
  - 将数据推入采集→处理缓冲区
- **关键要求**: 最小化抖动，确保采样率稳定

#### 2.2.2 处理线程 (THREAD_PROCESS)
- **优先级**: HIGHEST (Windows) / SCHED_FIFO 80 (Linux)
- **职责**:
  - 从采集缓冲区批量读取数据
  - 数据预处理（滤波、格式转换等）
  - 将处理后的数据推入处理→网络缓冲区

#### 2.2.3 网络发送线程 (THREAD_NETWORK)
- **优先级**: ABOVE_NORMAL (Windows) / SCHED_FIFO 60 (Linux)
- **职责**:
  - 从处理缓冲区批量读取数据
  - 通过TCP连接发送至上位机
  - 处理连接断开和重连

### 2.3 数据流

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  ADC硬件    │───▶│  采集缓冲区  │───▶│  处理缓冲区  │───▶│  上位机     │
│  (模拟器)   │    │  (SPSC)     │    │  (SPSC)     │    │  (TCP)      │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
      │                  │                  │                  │
      │                  │                  │                  │
      ▼                  ▼                  ▼                  ▼
   40kHz采样        无锁push/pop       批量处理          批量发送
   32bit精度        缓存256样本        256样本/批        网络传输
```

## 3. 核心模块设计

### 3.1 环形缓冲区 (Ring Buffer)

#### 3.1.1 设计特点
- **无锁设计**: 使用原子操作实现单生产者单消费者(SPSC)无锁队列
- **内存对齐**: 64字节对齐，避免伪共享
- **批量操作**: 支持批量push/pop，减少原子操作次数
- **容量归一化**: 自动对齐到2的幂次方，优化取模运算
- **C11原子屏障**: 使用 `atomic_thread_fence` 保证内存序

#### 3.1.2 关键实现
```c
typedef struct {
    adc_sample_t *buffer;       // 缓冲区数组
    uint32_t capacity;          // 容量（2的幂次方）
    uint32_t mask;              // 容量掩码 (capacity - 1)
    atomic_uint head;           // 写指针（原子）
    atomic_uint tail;           // 读指针（原子）
    uint64_t dropped;           // 丢弃计数
} ring_buffer_t;
```

#### 3.1.3 性能优化
- 批量操作：256样本/批，减少原子操作频率
- 编译器屏障：使用 `atomic_thread_fence(memory_order_release/acquire)`
- 对齐分配：使用 `posix_memalign` 分配64字节对齐内存

### 3.2 线程管理器 (Thread Manager)

#### 3.2.1 功能
- 线程生命周期管理（创建、启动、停止、销毁）
- 线程优先级设置
- 线程状态监控
- 统计信息收集

#### 3.2.2 跨平台实现
- **Windows**: 使用 `CreateThread` + `SetThreadPriority`
- **Linux**: 使用 `pthread_create` + `pthread_setschedparam`

#### 3.2.3 线程安全
- 使用互斥锁保护共享状态
- 使用条件变量实现优雅停止

### 3.3 TCP客户端

#### 3.3.1 功能特性
- 自动连接管理（连接、断开、重连）
- 批量数据发送
- 发送超时控制
- 引用计数式初始化（避免多实例重复初始化）

#### 3.3.2 跨平台实现
- **Windows**: Winsock2 API
- **Linux**: POSIX socket API

#### 3.3.3 关键修复（v5.0）
- 修复 `send_all` 遇到 0 字节发送时的死循环问题
- WSAStartup/WSACleanup 改为引用计数式初始化

### 3.4 时间戳模块

#### 3.4.1 功能
- 高精度时间获取
- 微秒级延时
- 时间格式化

#### 3.4.2 跨平台实现
- **Windows**: `QueryPerformanceCounter` + `QueryPerformanceFrequency`
- **Linux**: `clock_gettime(CLOCK_MONOTONIC)`

#### 3.4.3 优化（v5.0）
- POSIX 分支补充 `nanosleep` 的 `EINTR` 重试
- 减少跨平台定时行为差异

### 3.5 ADC模拟器

#### 3.5.1 功能
- 模拟40kHz采样率
- 生成正弦波信号
- 添加可控噪声
- 32位分辨率输出

#### 3.5.2 关键修复（v5.0）
- 修正 32 位分辨率移位溢出风险
- 采样线程加锁读取回调，消除数据竞争

## 4. 关键算法

### 4.1 无锁环形缓冲区算法

#### 4.1.1 Push操作（生产者）
```c
bool ring_buffer_push(ring_buffer_t *rb, const adc_sample_t *sample) {
    uint32_t current_head = atomic_load(&rb->head);
    uint32_t next_head = (current_head + 1) & rb->mask;
    
    if (next_head == atomic_load(&rb->tail)) {
        // 缓冲区满，丢弃最旧数据
        atomic_fetch_add(&rb->tail, 1);
        rb->dropped++;
    }
    
    rb->buffer[current_head & rb->mask] = *sample;
    atomic_thread_fence(memory_order_release);
    atomic_store(&rb->head, next_head);
    return true;
}
```

#### 4.1.2 Pop操作（消费者）
```c
bool ring_buffer_pop(ring_buffer_t *rb, adc_sample_t *sample) {
    uint32_t current_tail = atomic_load(&rb->tail);
    
    if (current_tail == atomic_load(&rb->head)) {
        return false; // 缓冲区空
    }
    
    *sample = rb->buffer[current_tail & rb->mask];
    atomic_thread_fence(memory_order_acquire);
    atomic_store(&rb->tail, (current_tail + 1) & rb->mask);
    return true;
}
```

### 4.2 高精度定时

#### 4.2.1 Windows实现
```c
void sleep_us(uint32_t us) {
    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    LARGE_INTEGER due;
    due.QuadPart = -((LONGLONG)us * 10); // 100ns单位
    SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}
```

#### 4.2.2 Linux实现（v5.0优化）
```c
void sleep_us(uint32_t us) {
    struct timespec req = {
        .tv_sec = us / 1000000,
        .tv_nsec = (us % 1000000) * 1000
    };
    struct timespec rem;
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        req = rem; // 处理中断，继续睡眠剩余时间
    }
}
```

## 5. 跨平台设计

### 5.1 编译系统

#### 5.1.1 CMake配置（v5.0优化）
```cmake
# 移除 CPU 绑定编译选项，提升可移植性
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adc_acquisition PRIVATE -O3 -Wall -Wextra)
    if(CMAKE_GENERATOR MATCHES "Ninja")
        target_compile_options(adc_acquisition PRIVATE -fdiagnostics-color=always)
    endif()
elseif(MSVC)
    target_compile_options(adc_acquisition PRIVATE /O2 /W4 /MP)
endif()
```

#### 5.1.2 平台特定库
```cmake
if(WIN32)
    target_link_libraries(adc_acquisition Threads::Threads ws2_32)
else()
    target_link_libraries(adc_acquisition Threads::Threads m)
endif()
```

### 5.2 条件编译

```c
#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    typedef LONG atomic_long;
    #define atomic_fetch_add(obj, val) InterlockedExchangeAdd(obj, val)
#else
    #include <pthread.h>
    #include <stdatomic.h>
    typedef atomic_long atomic_long;
#endif
```

## 6. 性能优化

### 6.1 已完成的优化

| 优化项 | 优化前 | 优化后 | 效果 |
|--------|--------|--------|------|
| 定时器 | Sleep(1ms) | WaitableTimer + 自旋 | 微秒级精度 |
| 内存屏障 | MemoryBarrier() | atomic_thread_fence | 跨平台可移植 |
| 批量处理 | 64样本/批 | 256样本/批 | 5x吞吐提升 |
| 内存分配 | malloc | posix_memalign(64B) | 避免伪共享 |
| 编译选项 | -march=native | target级选项 | 提升可移植性 |
| 网络初始化 | 重复WSAStartup | 引用计数式 | 避免重复初始化 |

### 6.2 v5.0 优化重点

- **跨平台稳定性**: 移除 CPU 绑定编译选项，改用标准 C11 原子操作
- **运行时开销**: 优化定时器实现，减少跨平台行为差异
- **数据竞争**: 修复 ADC 模拟器和网络模块的线程安全问题
- **测试覆盖**: 新增 timestamp 测试，4/4 测试全部通过

## 7. 测试验证

### 7.1 测试项目
- ring_buffer: 环形缓冲区功能和性能测试
- adc_simulator: ADC模拟器精度和稳定性测试
- performance: 系统整体性能测试
- timestamp: 时间戳精度和单调性测试（v5.0新增）

### 7.2 验证结果
```
100% tests passed, 0 tests failed out of 4
Total Test time (real) =  10.87 sec
```

## 8. 部署指南

### 8.1 构建步骤

```bash
# Windows (MinGW Makefiles)
cmake -B build-make -G "MinGW Makefiles"
cmake --build build-make

# Windows (Ninja)
cmake -B build -G Ninja
cmake --build build

# Linux
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 8.2 运行测试

```bash
ctest --test-dir build-make --output-on-failure
```

### 8.3 运行程序

```bash
./build-make/adc_acquisition -s 127.0.0.1 -p 8080 -t 60
```

## 9. 关键性能数据

### 9.1 理论计算
- 采样间隔: 25 μs
- 每样本处理时间预算: < 25 μs
- 网络带宽需求: ~200 KB/s (含协议开销)

### 9.2 实测性能（i7-14700KF + 32GB DDR5）
- 缓冲区写入吞吐量: 1267.43 M 样本/秒
- 缓冲区读取吞吐量: 1662.23 M 样本/秒
- 批量操作吞吐量: 3415.79 M 样本/秒 (256样本/批)
- 单操作延迟: 0.018 μs
- 批量操作延迟: 0.114 μs (256样本)
- 系统采样率达成率: 99.98%
- CPU占用率: < 2%
- 内存占用: ~8 MB

### 9.3 跨平台性能（v5.0 优化后）

| 平台 | 采样率达成率 | 批量吞吐量 | 单样本延迟 |
|------|-------------|-----------|-----------|
| Windows (i7-14700KF) | 99.98% | 3415 M/s | 0.018 μs |
| WSL2 | 100.00% | 2987 M/s | 0.017 μs |
| Ubuntu Server (i7-8565U) | 100.00% | 1632 M/s | 0.096 μs |

### 9.4 v5.0 优化重点

本次优化以**跨平台稳定性和运行时开销**为主：

- **构建层**: 移除 `-march=native/-mtune=native`，改用 target 级编译选项，避免产物绑死当前 CPU
- **时间模块**: POSIX 分支补充 `nanosleep` 的 `EINTR` 重试，减少跨平台定时行为差异
- **环形缓冲区**: 改用 C11 `atomic_thread_fence`，SPSC 可移植性更好
- **网络模块**: WSAStartup/WSACleanup 改为引用计数式初始化，修复 0 字节发送死循环
- **ADC 模拟器**: 修正 32 位分辨率移位溢出风险，采样线程加锁读取回调
- **测试补强**: 新增 timestamp 测试，4/4 测试全部通过

---

**文档版本**: 5.0  
**更新日期**: 2026-03-29  
**作者**: WuChengpei_Sky
