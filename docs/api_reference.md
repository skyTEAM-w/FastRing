# API 参考文档

## 目录

1. [ADC模拟器 API](#adc模拟器-api)
2. [环形缓冲区 API](#环形缓冲区-api)
3. [线程管理器 API](#线程管理器-api)
4. [TCP客户端 API](#tcp客户端-api)
5. [日志系统 API](#日志系统-api)
6. [时间戳 API](#时间戳-api)

---

## ADC模拟器 API

### 头文件

`#include "adc_simulator.h"`

### 数据类型

#### adc_config_t

```c
typedef struct {
    uint32_t sample_rate;       // 采样率（Hz）
    uint32_t resolution;        // 分辨率（位）
    uint32_t channel_count;     // 通道数
    double   signal_freq;       // 模拟信号频率（Hz）
    double   noise_level;       // 噪声水平（0.0-1.0）
    bool     enable_trigger;    // 启用触发
} adc_config_t;
```

#### adc_callback_t

```c
typedef void (*adc_callback_t)(const adc_sample_t *sample, void *user_data);
```

### 函数

#### adc_config_get_default

```c
void adc_config_get_default(adc_config_t *config);
```

获取默认ADC配置。

**参数:**
- `config` - 配置结构体指针

#### adc_simulator_create
```c
adc_simulator_t* adc_simulator_create(const adc_config_t *config);
```
创建ADC模拟器实例。

**参数:**
- `config` - 配置参数（可为NULL，使用默认配置）

**返回:**
- 成功: ADC模拟器句柄
- 失败: NULL

#### adc_simulator_destroy
```c
void adc_simulator_destroy(adc_simulator_t *adc);
```
销毁ADC模拟器实例。

**参数:**
- `adc` - ADC模拟器句柄

#### adc_simulator_start
```c
adc_error_t adc_simulator_start(adc_simulator_t *adc);
```
启动ADC采集。

**参数:**
- `adc` - ADC模拟器句柄

**返回:**
- `ADC_OK` - 成功
- `ADC_ERROR_THREAD` - 线程创建失败

#### adc_simulator_stop
```c
adc_error_t adc_simulator_stop(adc_simulator_t *adc);
```
停止ADC采集。

**参数:**
- `adc` - ADC模拟器句柄

#### adc_simulator_is_running
```c
bool adc_simulator_is_running(const adc_simulator_t *adc);
```
检查ADC是否正在运行。

**参数:**
- `adc` - ADC模拟器句柄

**返回:**
- `true` - 运行中
- `false` - 已停止

#### adc_simulator_set_callback
```c
adc_error_t adc_simulator_set_callback(adc_simulator_t *adc, 
                                        adc_callback_t callback, 
                                        void *user_data);
```
设置采样回调函数。

**参数:**
- `adc` - ADC模拟器句柄
- `callback` - 回调函数
- `user_data` - 用户数据指针

#### adc_simulator_get_sample
```c
adc_error_t adc_simulator_get_sample(adc_simulator_t *adc, 
                                      adc_sample_t *sample);
```
获取单个样本（非线程方式）。

**参数:**
- `adc` - ADC模拟器句柄
- `sample` - 样本数据指针

---

## 环形缓冲区 API

### 头文件
`#include "ring_buffer.h"`

### 数据类型

#### ring_buffer_t
无锁环形缓冲区结构（不透明类型）。

#### buffer_manager_t
缓冲区管理器结构（三级流水线）。

### 函数

#### ring_buffer_create
```c
ring_buffer_t* ring_buffer_create(uint32_t capacity);
```
创建环形缓冲区。

**参数:**
- `capacity` - 容量（会自动调整为2的幂次方）

**返回:**
- 成功: 缓冲区句柄
- 失败: NULL

#### ring_buffer_destroy
```c
void ring_buffer_destroy(ring_buffer_t *rb);
```
销毁环形缓冲区。

**参数:**
- `rb` - 缓冲区句柄

#### ring_buffer_push
```c
bool ring_buffer_push(ring_buffer_t *rb, const adc_sample_t *sample);
```
推入单个样本。

**参数:**
- `rb` - 缓冲区句柄
- `sample` - 样本数据

**返回:**
- `true` - 成功
- `false` - 缓冲区已满

#### ring_buffer_pop
```c
bool ring_buffer_pop(ring_buffer_t *rb, adc_sample_t *sample);
```
弹出单个样本。

**参数:**
- `rb` - 缓冲区句柄
- `sample` - 样本数据指针

**返回:**
- `true` - 成功
- `false` - 缓冲区为空

#### ring_buffer_push_batch
```c
bool ring_buffer_push_batch(ring_buffer_t *rb, 
                            const adc_sample_t *samples, 
                            uint32_t count, 
                            uint32_t *pushed);
```
批量推入样本。

**参数:**
- `rb` - 缓冲区句柄
- `samples` - 样本数组
- `count` - 请求推入数量
- `pushed` - 实际推入数量（可为NULL）

**返回:**
- `true` - 全部推入
- `false` - 部分推入

#### ring_buffer_pop_batch
```c
bool ring_buffer_pop_batch(ring_buffer_t *rb, 
                           adc_sample_t *samples, 
                           uint32_t count, 
                           uint32_t *popped);
```
批量弹出样本。

**参数:**
- `rb` - 缓冲区句柄
- `samples` - 样本数组
- `count` - 请求弹出数量
- `popped` - 实际弹出数量（可为NULL）

#### ring_buffer_count
```c
uint32_t ring_buffer_count(const ring_buffer_t *rb);
```
获取当前元素数量。

#### ring_buffer_is_empty
```c
bool ring_buffer_is_empty(const ring_buffer_t *rb);
```
检查缓冲区是否为空。

#### ring_buffer_is_full
```c
bool ring_buffer_is_full(const ring_buffer_t *rb);
```
检查缓冲区是否已满。

#### ring_buffer_available
```c
uint32_t ring_buffer_available(const ring_buffer_t *rb);
```
获取可用空间。

#### ring_buffer_clear
```c
void ring_buffer_clear(ring_buffer_t *rb);
```
清空缓冲区。

### 缓冲区管理器函数

#### buffer_manager_create
```c
buffer_manager_t* buffer_manager_create(void);
```
创建缓冲区管理器。

#### buffer_manager_destroy
```c
void buffer_manager_destroy(buffer_manager_t *bm);
```
销毁缓冲区管理器。

#### buffer_manager_init
```c
adc_error_t buffer_manager_init(buffer_manager_t *bm);
```
初始化三级流水线缓冲区。

---

## 线程管理器 API

### 头文件
`#include "thread_manager.h"`

### 数据类型

#### thread_type_t
```c
typedef enum {
    THREAD_CAPTURE = 0,     // 数据采集线程
    THREAD_PROCESS,         // 数据处理线程
    THREAD_NETWORK,         // 网络发送线程
    THREAD_COUNT
} thread_type_t;
```

#### thread_state_t
```c
typedef enum {
    THREAD_STATE_IDLE = 0,
    THREAD_STATE_RUNNING,
    THREAD_STATE_PAUSED,
    THREAD_STATE_ERROR,
    THREAD_STATE_STOPPED
} thread_state_t;
```

#### thread_stats_t
```c
typedef struct {
    uint64_t loop_count;        // 循环次数
    uint64_t item_count;        // 处理项目数
    uint64_t error_count;       // 错误次数
    double   avg_cycle_time_us; // 平均周期时间（微秒）
    double   max_cycle_time_us; // 最大周期时间（微秒）
} thread_stats_t;
```

#### thread_worker_t
```c
typedef void (*thread_worker_t)(thread_manager_t *tm, 
                                thread_type_t type, 
                                void *user_data);
```

### 函数

#### thread_manager_create
```c
thread_manager_t* thread_manager_create(void);
```
创建线程管理器。

#### thread_manager_destroy
```c
void thread_manager_destroy(thread_manager_t *tm);
```
销毁线程管理器。

#### thread_manager_init
```c
adc_error_t thread_manager_init(thread_manager_t *tm, 
                                buffer_manager_t *bm);
```
初始化线程管理器。

#### thread_manager_start
```c
adc_error_t thread_manager_start(thread_manager_t *tm);
```
启动所有线程。

#### thread_manager_stop
```c
adc_error_t thread_manager_stop(thread_manager_t *tm);
```
停止所有线程。

#### thread_manager_pause
```c
adc_error_t thread_manager_pause(thread_manager_t *tm);
```
暂停所有线程。

#### thread_manager_resume
```c
adc_error_t thread_manager_resume(thread_manager_t *tm);
```
恢复所有线程。

#### thread_manager_is_running
```c
bool thread_manager_is_running(const thread_manager_t *tm);
```
检查是否运行中。

#### thread_manager_get_state
```c
thread_state_t thread_manager_get_state(const thread_manager_t *tm, 
                                        thread_type_t type);
```
获取线程状态。

#### thread_manager_get_stats
```c
adc_error_t thread_manager_get_stats(const thread_manager_t *tm, 
                                     thread_type_t type, 
                                     thread_stats_t *stats);
```
获取线程统计信息。

#### thread_manager_set_worker
```c
adc_error_t thread_manager_set_worker(thread_manager_t *tm, 
                                      thread_type_t type, 
                                      thread_worker_t worker, 
                                      void *user_data);
```
设置线程工作函数。

#### thread_manager_get_buffer_manager
```c
buffer_manager_t* thread_manager_get_buffer_manager(thread_manager_t *tm);
```
获取缓冲区管理器。

#### thread_manager_get_system_stats
```c
system_stats_t* thread_manager_get_system_stats(thread_manager_t *tm);
```
获取系统统计信息。

---

## TCP客户端 API

### 头文件
`#include "tcp_client.h"`

### 数据类型

#### tcp_config_t
```c
typedef struct {
    char     server_ip[16];     // 服务器IP地址
    uint16_t server_port;       // 服务器端口
    uint32_t send_timeout_ms;   // 发送超时（毫秒）
    uint32_t recv_timeout_ms;   // 接收超时（毫秒）
    uint32_t buffer_size;       // 缓冲区大小
    bool     keep_alive;        // 保持连接
} tcp_config_t;
```

### 函数

#### tcp_config_get_default
```c
void tcp_config_get_default(tcp_config_t *config);
```
获取默认TCP配置。

#### tcp_client_create
```c
tcp_client_t* tcp_client_create(const tcp_config_t *config);
```
创建TCP客户端。

#### tcp_client_destroy
```c
void tcp_client_destroy(tcp_client_t *client);
```
销毁TCP客户端。

#### tcp_client_connect
```c
adc_error_t tcp_client_connect(tcp_client_t *client);
```
连接到服务器。

#### tcp_client_disconnect
```c
adc_error_t tcp_client_disconnect(tcp_client_t *client);
```
断开连接。

#### tcp_client_is_connected
```c
bool tcp_client_is_connected(const tcp_client_t *client);
```
检查连接状态。

#### tcp_client_send
```c
adc_error_t tcp_client_send(tcp_client_t *client, 
                            const void *data, 
                            size_t len, 
                            size_t *sent);
```
发送数据。

#### tcp_client_send_all
```c
adc_error_t tcp_client_send_all(tcp_client_t *client, 
                                const void *data, 
                                size_t len);
```
发送所有数据（阻塞直到全部发送）。

#### tcp_client_receive
```c
adc_error_t tcp_client_receive(tcp_client_t *client, 
                               void *buffer, 
                               size_t len, 
                               size_t *received);
```
接收数据。

#### tcp_client_send_data_block
```c
adc_error_t tcp_client_send_data_block(tcp_client_t *client, 
                                       const data_block_t *block);
```
发送数据块。

---

## 日志系统 API

### 头文件
`#include "logger.h"`

### 数据类型

#### log_level_t
```c
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_COUNT
} log_level_t;
```

### 函数

#### logger_init
```c
int logger_init(const char *filename, log_level_t level);
```
初始化日志系统。

**参数:**
- `filename` - 日志文件路径（NULL表示仅控制台输出）
- `level` - 日志级别

#### logger_close
```c
void logger_close(void);
```
关闭日志系统。

#### logger_set_level
```c
void logger_set_level(log_level_t level);
```
设置日志级别。

#### logger_get_level
```c
log_level_t logger_get_level(void);
```
获取当前日志级别。

### 宏

```c
LOG_DEBUG(fmt, ...)  // 调试日志
LOG_INFO(fmt, ...)   // 信息日志
LOG_WARN(fmt, ...)   // 警告日志
LOG_ERROR(fmt, ...)  // 错误日志
LOG_FATAL(fmt, ...)  // 致命错误日志
```

---

## 时间戳 API

### 头文件
`#include "timestamp.h"`

### 函数

#### timestamp_init
```c
void timestamp_init(void);
```
初始化时间戳系统。

#### get_timestamp_us
```c
uint64_t get_timestamp_us(void);
```
获取当前时间戳（微秒）。

#### get_timestamp_ms
```c
uint64_t get_timestamp_ms(void);
```
获取当前时间戳（毫秒）。

#### get_timestamp_ns
```c
uint64_t get_timestamp_ns(void);
```
获取当前时间戳（纳秒）。

#### sleep_us
```c
void sleep_us(uint64_t us);
```
睡眠指定微秒。

#### sleep_ms
```c
void sleep_ms(uint32_t ms);
```
睡眠指定毫秒。

#### get_cpu_cycles
```c
uint64_t get_cpu_cycles(void);
```
获取CPU周期计数（或微秒时间戳）。

---

## 错误码

```c
typedef enum {
    ADC_OK = 0,                 // 成功
    ADC_ERROR_INIT = -1,        // 初始化错误
    ADC_ERROR_BUFFER = -2,      // 缓冲区错误
    ADC_ERROR_NETWORK = -3,     // 网络错误
    ADC_ERROR_THREAD = -4,      // 线程错误
    ADC_ERROR_TIMEOUT = -5,     // 超时
    ADC_ERROR_INVALID_PARAM = -6 // 无效参数
} adc_error_t;
```
