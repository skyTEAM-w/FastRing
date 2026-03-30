# FastRing - 高性能ADC数据采集与传输系统

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)](README.md)
[![Language](https://img.shields.io/badge/language-C11-orange.svg)](README.md)
[![Build](https://img.shields.io/badge/build-CMake%20%7C%20Ninja-green.svg)](README.md)

一个基于多线程架构的高性能数据采集与传输系统，实现40kHz采样率、32位精度的ADC数据获取，并通过网络实时发送至上位机。

## 目录

- [核心特性](#核心特性)
- [系统架构](#系统架构)
- [快速开始](#快速开始)
  - [环境要求](#环境要求)
  - [编译](#编译)
  - [运行](#运行)
- [性能指标](#性能指标)
- [测试](#测试)
- [配置](#配置)
- [可视化](#代码结构可视化)
- [文档](#文档)
- [贡献指南](#贡献指南)
- [版本历史](#版本历史)
- [许可证](#许可证)
- [作者](#作者)

## 核心特性

- **高性能采集** - 支持40kHz采样率，32位精度
- **多线程架构** - 三级流水线设计（采集-处理-传输）
- **无锁缓冲区** - 使用无锁环形缓冲区，避免线程阻塞
- **实时传输** - TCP网络传输，支持自动重连
- **跨平台支持** - 完整支持Windows和Linux系统
  - Windows: Winsock2 + InterlockedIncrement
  - Linux: POSIX socket + stdatomic.h
- **配置管理** - 集中式配置管理模块
- **错误处理** - 完善的错误码转换机制

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

## 快速开始

### 环境要求

| 依赖 | 版本要求 |
|------|----------|
| CMake | >= 3.16 |
| C编译器 | C11兼容 |
| Windows | MinGW-w64 GCC 或 Visual Studio 2019+ |
| Linux | GCC 9+ 或 Clang 10+ |

### 编译

#### Windows (MinGW-w64 + Ninja)

```powershell
cmake -B build -G Ninja
cmake --build build
```

#### Windows (Visual Studio)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

#### Linux

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 运行

```bash
# 基本运行
./build/adc_acquisition

# 指定服务器
./build/adc_acquisition -s 192.168.1.100 -p 8080

# 指定运行时间
./build/adc_acquisition -t 60

# 启用详细日志
./build/adc_acquisition -v

# 查看帮助
./build/adc_acquisition -h
```

## 性能指标

### 跨平台性能对比

| 指标 | Windows | WSL | Ubuntu Server | 目标值 |
|------|---------|-----|---------------|--------|
| 采样率 | 39,990 Hz | 39,999 Hz | 39,999 Hz | 40,000 Hz |
| 采样率达成率 | 99.98% | 100.00% | 100.00% | > 95% |
| 批量吞吐量 | 3415 M/s | 2987 M/s | 1632 M/s | > 1000 M/s |
| 单样本延迟 | 0.018 μs | 0.017 μs | 0.096 μs | < 1 μs |
| 丢弃率 | 0% | 0% | 0% | < 1% |
| CPU占用率 | < 2% | < 2% | < 15% | < 30% |

### 验证平台

- ✅ Windows 11 (MinGW-w64 GCC 15.2.0)
- ✅ Ubuntu 22.04 WSL2 (GCC 13.3.0)
- ✅ Ubuntu Server (i7-8565U + 8GB RAM)

## 测试

```bash
# 运行所有测试
cd build && ctest --output-on-failure

# 运行单个测试
./build/tests/test_ring_buffer
./build/tests/test_adc_simulator
./build/tests/test_performance
```

### 测试结果

```
100% tests passed, 0 tests failed out of 3
Total Test time (real) =  10.87 sec
```

## 配置

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

## 代码结构可视化

项目提供交互式代码结构可视化工具：

```bash
cd visualization
python -m http.server 3000
# 浏览器访问 http://localhost:3000
```

功能包括：
- 依赖关系图
- 层级结构图
- 数据流图

## 文档

| 文档 | 说明 |
|------|------|
| [架构设计文档](docs/architecture.md) | 详细的系统架构设计 |
| [API参考文档](docs/api_reference.md) | 完整的API接口说明 |
| [测试报告](docs/test_report.md) | 跨平台测试报告 (v4.0) |

## 项目结构

```
FastRing/
├── CMakeLists.txt          # CMake构建配置
├── LICENSE                 # MIT许可证
├── README.md               # 项目说明
├── include/                # 头文件目录
│   ├── adc_types.h         # 类型定义
│   ├── adc_simulator.h     # ADC模拟器接口
│   ├── ring_buffer.h       # 无锁环形缓冲区
│   ├── thread_manager.h    # 线程管理器
│   ├── tcp_client.h        # TCP客户端
│   ├── config.h            # 配置管理
│   ├── timestamp.h         # 时间戳工具
│   └── logger.h            # 日志系统
├── src/                    # 源文件目录
│   ├── main.c              # 主程序入口
│   ├── adc/                # ADC模块
│   ├── buffer/             # 缓冲区模块
│   ├── thread/             # 线程模块
│   ├── network/            # 网络模块
│   └── utils/              # 工具模块
├── tests/                  # 测试程序
├── visualization/          # 代码结构可视化
└── docs/                   # 文档
```

## 贡献指南

欢迎贡献代码、报告问题或提出建议！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 提交 Pull Request

### 代码规范

- 遵循 C11 标准
- 使用 4 空格缩进
- 函数和变量使用 snake_case 命名
- 添加必要的注释和文档

## 版本历史

- **v4.0** (2026-03-29) - 跨平台测试完成，新增Ubuntu Server测试结果
- **v3.0** (2026-03-26) - 新增配置管理和错误处理模块
- **v2.0** (2026-03-25) - Windows平台性能优化
- **v1.0** (2026-03-24) - 初始版本，基本功能实现

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

## 作者

**WuChengpei_Sky**

如有问题或建议，欢迎提交 Issue。
