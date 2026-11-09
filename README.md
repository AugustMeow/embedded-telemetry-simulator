# Embedded Telemetry Simulator

基于 C++17 和 Linux 的嵌入式遥测模拟项目，用于模拟设备传感器数据采集、数据校验、异步日志记录以及 TCP 网络通信。

项目重点关注 Linux 环境下的 C++ 工程实践，包括多线程编程、Socket 网络通信、TCP 协议解析、非阻塞 I/O 和高低水位流量控制。

## 1. 项目功能

- **传感器模拟**：模拟温度、电压传感器数据采集，支持异常数据和读取故障模拟。
- **数据校验**：检查采集数据是否处于有效范围，处理读取错误和非法数值。
- **异步日志系统**：基于生产者-消费者模型实现多线程日志记录，支持滚动日志、队列管理和优雅退出。
- **配置管理**：读取配置文件，进行参数解析和严格校验。
- **TCP Server**：实现基于 Linux Socket 的多客户端 TCP 服务端，支持自定义二进制协议。
- **I/O 多路复用**：分别实现 poll 和 epoll LT 模式，管理多个客户端连接。

## 2. 技术栈

| 分类 | 技术 |
|---|---|
| 编程语言 | C++17 |
| 操作系统 | Linux / Ubuntu |
| 构建工具 | CMake |
| 编译器 | GCC |
| 调试工具 | GDB |
| 网络编程 | TCP Socket |
| I/O 多路复用 | poll / epoll |
| 并发编程 | std::thread / mutex / condition_variable / atomic |
| 测试 | CTest / Python 测试脚本 |
| 内存与并发检查 | ASAN / TSAN（已完成的测试阶段） |

## 3. 项目架构

### 3.1 传感器与数据校验

通过抽象传感器接口统一不同传感器的数据采集操作。

目前实现：
- TemperatureSensor
- VoltageSensor

支持模拟正常数据、超范围数据和读取失败等情况。

### 3.2 异步日志系统

采用生产者-消费者模型：

- 业务线程通过 log() 提交日志。
- 使用线程安全队列缓存日志。
- 独立 Worker 线程负责日志写入。
- 支持队列容量限制和丢弃统计。
- 支持日志文件滚动。
- close() 时等待队列数据处理完成后退出。

### 3.3 TCP Server

实现多客户端 TCP 服务端，采用自定义二进制协议。

协议格式：

| 字段 | 长度 |
|---|---:|
| Magic | 2 Bytes |
| Version | 1 Byte |
| Type | 1 Byte |
| Payload Length | 4 Bytes |
| Payload | Variable |

协议使用大端字节序，支持：
- TCP 粘包和拆包处理。
- 非法 Header 检查。
- 消息长度校验。
- 多客户端数据隔离。

### 3.4 I/O 多路复用

分别实现两种事件管理方式。

**poll 版本**
- 使用 poll() 监听多个客户端。
- 维护客户端连接及发送状态。
- 支持非阻塞收发。

**epoll 版本**
- 使用 epoll_create1() 创建 epoll 实例。
- 使用 epoll_ctl() 动态管理客户端事件。
- 使用 epoll_wait() 获取就绪事件。
- 使用 unordered_map 管理客户端状态。
- 支持 EPOLLIN / EPOLLOUT 事件切换。

### 3.5 高低水位流量控制

针对慢客户端和发送缓冲区积压问题，实现高低水位背压机制。

- 达到 HIGH_WATERMARK：暂停接收和继续解析请求。
- 低于 LOW_WATERMARK：恢复接收和缓存数据解析。
- 通过发送偏移量管理部分发送的数据。
- 通过缓冲区压缩减少已发送数据占用的内存。
- 设置最大接收和发送缓冲区，避免无限增长。

同时支持 TCP 半关闭处理，在收到 FIN 后，根据待发送数据情况决定是否延迟关闭连接。

## 4. 测试情况

使用 Python 编写客户端测试脚本，对 TCP Server 进行功能和压力验证。

| 测试场景 | 测试结果 |
|---|---|
| 基本 TCP 协议收发 | PASS |
| 多客户端隔离 | PASS |
| 粘包与拆包 | PASS |
| 连续 1000 帧处理 | PASS |
| TCP 半关闭 | PASS |
| 高低水位切换 | PASS |
| 多客户端水位隔离 | PASS |
| 1MB 数据压力测试 | PASS |
| 5MB 数据压力测试 | PASS |
| 10MB 数据压力测试 | PASS |
| 慢客户端测试 | PASS |

通过 SHA256 校验发送端和接收端数据，验证大数据量传输的完整性。

## 5. 构建方式

环境要求：
- Linux
- GCC
- CMake

构建项目：

```bash
git clone https://github.com/AugustMeow/embedded-telemetry-simulator.git

cd embedded-telemetry-simulator

mkdir -p build
cd build

cmake ..
cmake --build . -j$(nproc)
```

运行单元测试：

```bash
ctest --output-on-failure
```

## 6. 项目实践与技术重点

本项目主要用于实践以下 Linux C++ 开发能力：

- C++ 面向对象设计与模块化开发。
- 多线程同步和线程安全队列。
- Linux TCP Socket 编程。
- 非阻塞 I/O 和 I/O 多路复用。
- 自定义 TCP 应用层协议设计。
- TCP 粘包、拆包及半关闭处理。
- 高低水位背压和慢客户端处理。
- CMake 项目构建及自动化测试。

## 7. 后续计划

- 完善网络异常处理。
- 完善项目文档和测试覆盖。
- 持续优化代码结构和接口设计。
