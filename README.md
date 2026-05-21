# Chat Room 使用指南

## 项目概述

这是一个基于 **Linux 环境** 的命令行聊天程序，使用 **C++** 编写，采用 **epoll** 事件驱动模型实现高并发通信。

### 项目结构

```
chat_room/
├── client/
│   ├── Client.hpp    # 客户端核心实现
│   ├── client.cpp    # 客户端入口
├── server/
│   ├── server.hpp    # 服务端核心实现
│   ├── server.cpp    # 服务端入口
└── README.md
```

### 核心功能

- ✅ 支持最多 **5 个用户** 同时在线聊天
- ✅ 消息广播：任一用户发送消息，其他所有用户都能收到
- ✅ 非阻塞 I/O，基于 epoll 高效处理
- ✅ 自动检测用户连接/断开

---

## 快速开始

### 1. 启动服务端

打开一个终端窗口，运行服务端程序：

```bash
cd /home/sanziyue/code/chat_room/server
./server <IP地址> <端口号>
```

**示例：**
```bash
./server 127.0.0.1 8888
```

> **说明：**
> - IP 地址可以是 `127.0.0.1`（本地回环）或服务器的实际 IP
> - 端口号范围：1024-65535
> - 服务端启动后会持续运行，等待客户端连接

### 2. 启动客户端

打开新的终端窗口（可打开多个），运行客户端程序：

```bash
cd /home/sanziyue/code/chat_room/client
./client <服务端IP> <服务端端口>
```

**示例：**
```bash
./client 127.0.0.1 8888
```

### 3. 开始聊天

连接成功后，直接在命令行输入消息并回车，消息会广播给所有在线用户。

---

## 使用示例

**服务端输出：**
```
one user enter
one user enter
from client: Hello everyone!
```

**客户端聊天：**
```
Hello everyone!
Hi there!
```

---

## 重新编译（可选）

如果需要修改源代码后重新编译：

### 编译服务端
```bash
cd /home/sanziyue/code/chat_room/server
g++ server.cpp -o server -std=c++17
```

### 编译客户端
```bash
cd /home/sanziyue/code/chat_room/client
g++ client.cpp -o client -std=c++17
```

---

## 注意事项

1. **用户限制**：最多支持 5 个用户同时在线，超出时新用户会收到提示
2. **端口占用**：确保指定的端口未被其他程序占用
3. **网络权限**：如果使用非本地 IP，确保防火墙允许相应端口
4. **退出方式**：按 `Ctrl+C` 终止程序

---

## 技术实现要点

- **服务端**：单例模式 + epoll 事件循环，管理多个客户端连接
- **客户端**：同时监听标准输入和服务器消息
- **通信协议**：基于 TCP/IP，使用 socket 进行数据传输
        
