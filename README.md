# 高性能ADC数据采集与传输系统

## 项目简介

这是一个基于多线程架构的高性能数据采集与传输系统，实现了40kHz采样率、32位精度的ADC数据获取，并通过网络实时发送至上位机。

## 核心特性

- **高性能采集**: 支持40kHz采样率，32位精度
- **多线程架构**: 三级流水线设计（采集-处理-传输）
- **无锁缓冲区**: 使用无锁环形缓冲区，避免线程阻塞
- **实时传输**: TCP网络传输，支持自动重连
- **跨平台**: 支持Windows和Linux系统

## 系统架构

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

## 项目结构

```
ADCDataAcquisition/
├── CMakeLists.txt          # CMake构建配置
├── README.md               # 项目说明
├── include/                # 头文件目录
│   ├── adc_types.h         # 类型定义和常量
│   ├── adc_simulator.h     # ADC模拟器接口
│   ├── ring_buffer.h       # 无锁环形缓冲区
│   ├── thread_manager.h    # 线程管理器
│   ├── tcp_client.h        # TCP客户端
│   ├── timestamp.h         # 时间戳工具
│   └── logger.h            # 日志系统
├── src/                    # 源文件目录
│   ├── main.c              # 主程序入口
│   ├── adc/
│   │   └── adc_simulator.c # ADC模拟器实现
│   ├── buffer/
│   │   └── ring_buffer.c   # 环形缓冲区实现
│   ├── thread/
│   │   ├── thread_manager.c    # 线程管理器实现
│   │   └── thread_workers.c    # 线程工作函数
│   ├── network/
│   │   └── tcp_client.c    # TCP客户端实现
│   └── utils/
│       ├── timestamp.c     # 时间戳实现
│       └── logger.c        # 日志系统实现
├── tests/                  # 测试程序
│   ├── CMakeLists.txt
│   ├── test_server.c       # 测试服务器（模拟上位机）
│   ├── test_ring_buffer.c  # 环形缓冲区测试
│   ├── test_adc_simulator.c # ADC模拟器测试
│   └── test_performance.c  # 性能测试
└── docs/                   # 文档
    ├── architecture.md     # 架构设计文档
    └── api_reference.md    # API参考文档
```

## 编译与运行

### 环境要求

- CMake 3.16+
- C11/C++17兼容编译器
- Windows: MinGW或Visual Studio
- Linux: GCC

### 编译步骤

```bash
# 创建构建目录
mkdir build && cd build

# 生成构建文件
cmake ..

# 编译
cmake --build .

# 或者使用多线程编译
cmake --build . --parallel
```

### 运行程序

```bash
# 运行主程序
./adc_acquisition

# 指定服务器地址
./adc_acquisition -s 192.168.1.100 -p 8080

# 指定运行时间（秒）
./adc_acquisition -t 60

# 启用详细日志
./adc_acquisition -v

# 查看帮助
./adc_acquisition -h
```

### 运行测试服务器

在另一个终端运行测试服务器（模拟上位机）：

```bash
./test_server
```

## 性能指标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 采样率 | 40 kHz | 每秒40,000个样本 |
| 分辨率 | 32 bits | 每个样本4字节 |
| 数据速率 | 156.25 KB/s | 原始数据吞吐量 |
| 端到端延迟 | < 10 ms | 采集到发送完成 |
| 缓冲区吞吐量 | > 10M 样本/秒 | 无锁环形缓冲区 |
| 单操作延迟 | < 1 μs | push/pop操作 |

## 测试

### 运行单元测试

```bash
# 运行所有测试
ctest

# 运行特定测试
./test_ring_buffer
./test_adc_simulator
./test_performance
```

### 测试说明

- **test_ring_buffer**: 测试无锁环形缓冲区的功能、性能和线程安全性
- **test_adc_simulator**: 测试ADC模拟器的采样精度、稳定性和性能
- **test_performance**: 系统整体性能测试，包括吞吐量和延迟
- **test_server**: 模拟上位机接收数据，用于网络传输测试

## 配置参数

### ADC配置

```c
adc_config_t config = {
    .sample_rate = 40000,      // 采样率 (Hz)
    .resolution = 32,          // 分辨率 (位)
    .channel_count = 1,        // 通道数
    .signal_freq = 1000.0,     // 模拟信号频率 (Hz)
    .noise_level = 0.01,       // 噪声水平 (0.0-1.0)
    .enable_trigger = false    // 启用触发
};
```

### 网络配置

```c
tcp_config_t config = {
    .server_ip = "127.0.0.1",      // 服务器IP
    .server_port = 8080,           // 服务器端口
    .send_timeout_ms = 5000,       // 发送超时 (ms)
    .recv_timeout_ms = 5000,       // 接收超时 (ms)
    .buffer_size = 65536,          // 缓冲区大小
    .keep_alive = true             // 保持连接
};
```

## 文档

- [架构设计文档](docs/architecture.md) - 详细的系统架构设计
- [API参考文档](docs/api_reference.md) - 完整的API接口说明

## 许可证

MIT License

## 作者

高性能数据采集与传输系统开发团队
