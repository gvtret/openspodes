# OpenSPODES — IEC 62056-21 DLMS/COSEM 协议库

[![CI](https://github.com/gvtret/openspodes/actions/workflows/ci.yml/badge.svg)](https://github.com/gvtret/openspodes/actions/workflows/ci.yml)

OpenSPODES 是 **IEC 62056 DLMS/COSEM** 协议栈的便携式 C11 实现，参照 [spodes-rs](https://github.com/gvtret/spodes-rs) 设计，兼容 **СПОДЭС / GOST** 配置文件（Р 1323565.1）。

适用于嵌入式和服务器场景：**核心库无堆内存分配**，HAL 加密通过函数指针注入，使用静态缓冲区。

**版本：** 1.0.0  
**许可证：** GPL-3.0-or-later（参见 [LICENSE](LICENSE)）

---

## 目录

- [什么是 IEC 62056-21 / DLMS/COSEM](#什么是-iec-62056-21--dlmscosem)
- [协议分层架构](#协议分层架构)
- [OpenSPODES 实现概览](#openspodes-实现概览)
- [核心概念](#核心概念)
- [快速上手](#快速上手)
- [集成指南](#集成指南)
- [安全机制](#安全机制)
- [项目结构](#项目结构)
- [参考资料](#参考资料)

---

## 什么是 IEC 62056-21 / DLMS/COSEM

### DLMS/COSEM 简介

**DLMS**（Device Language Message Specification，设备语言消息规范）是一套国际标准协议族，用于智能电表、燃气表、水表等能源计量设备与主站系统之间的通信。

**COSEM**（Companion Specification for Energy Metering，能源计量配套规范）定义了计量设备的数据模型——将设备功能抽象为**接口类**（Interface Classes），每个类包含若干**属性**（Attributes）和**方法**（Methods）。

IEC 62056 是 DLMS/COSEM 的国际标准编号，包含以下子标准：

| 标准编号 | 内容 |
|---------|------|
| IEC 62056-21 | 基于双工通信的直接本地（Local）数据交换 |
| IEC 62056-41 | 应用层安全（ACSE 认证 + HLS） |
| IEC 62056-42 | 物理层和数据链路层——HDLC 帧格式 |
| IEC 62056-46 | HDLC 会话层封装 |
| IEC 62056-47 | COSEM TCP/UDP 传输层封装 |
| IEC 62056-5-3 | xDLMS 应用层服务（GET/SET/ACTION） |
| IEC 62056-6-1 | COSEM 对象模型（42 个接口类） |
| IEC 62056-6-2 | COSEM 接口类定义 |

### 适用场景

DLMS/COSEM 广泛应用于：

- **智能电网**：电表与抄表系统之间的数据采集
- **能源管理**：远程读取用电量、电压、电流等参数
- **设备控制**：远程设置费率、断电/复电等操作
- **需求响应**：实时负荷控制和价格信号下发
- **燃气/水表**：通过 concentrator 集中抄表

---

## 协议分层架构

```
┌─────────────────────────────────────┐
│         应用层 (Application)         │
│   xDLMS: GET / SET / ACTION         │
│   ACSE: AARQ / AARE / RLRQ         │
│   COSEM 对象模型 (42 个接口类)        │
├─────────────────────────────────────┤
│         安全层 (Security)            │
│   HLS (High-Level Security)         │
│   glo/ded 加密 (AES-GCM/Kuznyechik) │
│   重放保护 (Invocation Counter)      │
├─────────────────────────────────────┤
│         传输层 (Transport)           │
│   HDLC 会话 (IEC 62056-46)          │
│   COSEM Wrapper (IEC 62056-47)      │
├─────────────────────────────────────┤
│         数据链路层 (Data Link)        │
│   HDLC 帧 (IEC 62056-42)           │
│   FCS 校验 / 地址 / 控制             │
├─────────────────────────────────────┤
│         物理层 (Physical)            │
│   串口 / TCP-IP / 无线               │
└─────────────────────────────────────┘
```

### 各层说明

**物理层**：支持串口（RS-485/RS-232）、TCP/IP、Wi-SUN 等物理介质。

**数据链路层**：使用 HDLC（High-level Data Link Control）帧格式，包含标志字节（0x7E）、地址、控制字段、信息字段和 FCS 校验。支持 I 帧（信息帧）、S 帧（监督帧）、U 帧（无编号帧）。

**传输层**：
- **HDLC 会话**（IEC 62056-46）：通过 SNRM/UA 建立连接，支持序列号 N(S)/N(R) 实现确认和重传，DISC/UA 断开连接。
- **COSEM Wrapper**（IEC 62056-47）：基于 TCP/UDP 的简单封装，适合 IP 网络。

**安全层**：
- **LLS**（Low-Level Security）：明文传输，仅适用于可信链路
- **HLS**（High-Level Security）：基于 GMAC 的认证机制，支持 MD5/SHA1/SHA256/GOST 8-10
- **glo 加密**：AES-GCM 或 Kuznyechik-CTR-CMAC，保护数据机密性和完整性
- **ded 加密**：专用密钥传输，仅在 glo-initiate-request 中使用

**应用层**：
- **ACSE**（Association Control Service Element）：管理应用关联（AARQ 建立 / AARE 响应 / RLRQ 释放）
- **xDLMS 服务**：GET（读取属性）、SET（写入属性）、ACTION（执行方法）
- **通用块传输（GBT）**：支持大数据分块传输，包含确认/未确认模式和丢块恢复
- **COSEM 对象模型**：将设备功能抽象为接口类，每个实例通过 6 字节 OBIS 编码寻址

---

## OpenSPODES 实现概览

### 功能矩阵

| 功能领域 | 状态 | 说明 |
|---------|------|------|
| BER/AXDR 编解码 | ✅ | 所有 A-XDR 类型 + 复合结构 |
| HDLC 会话 | ✅ | SNRM/UA + XID + N(S)/N(R) + DISC/DM |
| COSEM Wrapper | ✅ | TCP/UDP 封装 |
| 会话驱动（Client/Server） | ✅ | 连接/认证/数据交换/断开 |
| GET/SET/ACTION | ✅ | 含 with-list 和 block transfer |
| 通用块传输（GBT） | ✅ | 未确认 + 确认 + 丢块恢复 |
| glo/ded 加密 | ✅ | AES-GCM + Kuznyechik suite 8/9 |
| HLS 认证 | ✅ | GMAC + MD5/SHA1/SHA256/GOST |
| general-ciphering / general-signing | ✅ | 通用加密和签名 |
| Push + 事件通知 | ✅ | 服务器主动推送 |
| 42 个 COSEM 接口类 | ✅ | 完整实现 |
| GOST 密码学 | ✅ | Streebog / Kuznyechik / GOST 34.10 / VKO/KDF |
| 选择性访问 | ✅ | encode/decode |
| СПОДУС 集中器 | ✅ | 通道表 / 直连表 / 代理 / 轮询 |
| Linux HAL | ✅ | TCP + OpenSSL + POSIX 定时器 + 随机数 |

### 关键设计原则

1. **零堆分配**：核心库使用 1KB 固定 PDU 缓冲区，不调用 `malloc`/`free`，适合裸机和 MCU 环境。
2. **HAL 可插拔**：加密、传输、定时、随机数均通过函数指针结构体注入，平台无关。
3. **BER 长度编码**：<128 字节用 1 字节，128–65535 字节用 2 字节（`0x81` + 长度字节）。
4. **ASN.1 默认 EXPLICIT TAGS**：字段未标注 IMPLICIT/EXPLICIT 时使用 EXPLICIT（构造上下文标签包装通用编码）。

---

## 核心概念

### OBIS 编码

OBIS（Object Identification System）是 6 字节标识符，用于唯一寻址 COSEM 对象：

```
A.B.C.D.E.F
│ │ │ │ │ └─ F: 通道号（255 = 默认）
│ │ │ │ └─── E: 参数组
│ │ │ └───── D: 参数号
│ │ └─────── C: 逻辑设备名
│ └───────── B: 组
└─────────── A: 媒介
```

示例：
- `{0, 0, 1, 0, 0, 255}` — 当前正向有功电能（总）
- `{0, 0, 96, 1, 0, 255}` — 设备序列号
- `{0, 0, 42, 0, 0, 255}` — 通信端口默认参数

### 接口类（Interface Classes）

COSEM 定义了 42 个标准接口类，OpenSPODES 全部实现：

| 类 ID | 名称 | 用途 |
|-------|------|------|
| 1 | Data | 通用数据对象（整数、字符串、枚举） |
| 3 | Register | 带缩放系数的计量值 |
| 4 | ExtendedRegister | 带时间戳和状态的扩展寄存器 |
| 5 | Demand Register | 需量值（最大/最小） |
| 6 | RegisterActivation | 寄存器激活列表 |
| 7 | ProfileGeneric | 通用曲线/日志（历史数据） |
| 8 | Push | 事件推送配置 |
| 9 | SMP | 安全消息保护 |
| 15 | IEC HSDL Configuration | IEC 高速数据链路配置 |
| 23 | Disconnect Control | 断电/复电控制 |
| 40 | Association LN | 逻辑名关联对象 |
| 41 | Association SN | 序列号关联对象 |
| 42 | SAP Assignment | SAP 地址分配 |
| ... | ... | 共 42 个类 |

### HDLC 帧结构

```
┌──────┬────────┬────────┬────────┬───────┬──────┬──────┐
│ 0x7E │ 地址   │ 控制   │ 信息   │ 地址  │ FCS  │ 0x7E │
│ 标志 │ (1-2B) │ (1B)   │ (变长) │ (1-2B)│ (2B) │ 标志 │
└──────┴────────┴────────┴────────┴───────┴──────┴──────┘
```

- **I 帧**（信息帧）：携带应用数据，包含发送/接收序列号
- **S 帧**（监督帧）：RR（接收就绪）、RNR（接收未就绪）、REJ（拒绝）
- **U 帧**（无编号帧）：SNRM（建立连接）、UA（确认）、DISC（断开）、DM（断开模式）

---

## 快速上手

### 环境要求

- CMake ≥ 3.16
- C11 编译器（GCC / Clang）
- OpenSSL（可选，启用 AES-GCM/ECDSA 测试和示例）

### 编译

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### 运行测试

```bash
ctest --test-dir build --output-on-failure
```

### 覆盖率

```bash
cmake -S . -B build-cov -DENABLE_COVERAGE=ON
cmake --build build-cov && ctest --test-dir build-cov
python3 scripts/coverage_report.py   # 在 build-cov/ 目录下运行
```

### 示例程序

**回环模式（进程内通信）：**

```bash
./build/openspodes_loopback_cli demo
```

**TCP 服务端 + 客户端（端口 4059）：**

```bash
./build/openspodes_tcp_server &
./build/openspodes_tcp_client
```

**Linux HAL 演示：**

```bash
./build/openspodes_tcp_server &
./build/openspodes_linux_demo 127.0.0.1:4059
```

---

## 集成指南

### 客户端示例

```c
#include "openspodes.h"
#include "client/client.h"

osp_client_t client;
osp_client_init(&client, &transport, OSP_FRAMING_WRAPPER);

osp_sec_context_t sec;
osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, system_title);
osp_client_set_security(&client, &sec);

osp_client_enable_gbt(&client, 64);
osp_client_connect(&client, 5000);

osp_value_t result;
osp_obis_t obis = {1, 0, 1, 8, 0, 255};  // 正向有功电能
osp_client_get(&client, 1, &obis, 1, &result);

osp_client_release(&client);
osp_client_disconnect(&client);
```

### 服务端示例

```c
#include "openspodes.h"
#include "server/server.h"
#include "ic/data.h"

osp_server_t server;
osp_server_init(&server, &transport, OSP_FRAMING_WRAPPER);

osp_ic_data_t data_obj;
osp_ic_data_init(&data_obj, (osp_obis_t){1, 0, 1, 8, 0, 255});
data_obj.value = osp_val_u32(123456);
osp_server_register(&server, osp_ic_data_class(), &data_obj);

for (;;) {
    osp_err_t r = osp_server_accept(&server, 30000);
    if (r == OSP_ERR_TIMEOUT) continue;
    if (r != OSP_OK) break;
}
```

### HDLC 会话示例

```c
#include "transport/hdlc_session.h"

osp_hdlc_session_t session;
osp_hdlc_session_init_client(&session, &transport, 2, 1, 3, 1);

osp_hdlc_session_connect(&session, 5000);  // SNRM/UA + XID
osp_hdlc_session_send_apdu(&session, apdu, apdu_len);
osp_hdlc_session_recv_apdu(&session, buf, sizeof(buf), &len, 5000);
osp_hdlc_session_disconnect(&session, 2000);  // DISC/UA
```

---

## 安全机制

### HLS 认证流程

1. 客户端发送 **AARQ**，携带 system_title
2. 服务端返回 **AARE**，包含随机数（challenge）
3. 客户端使用密钥对随机数进行 GMAC 计算，通过 **GlobalTransferRequest** 发送
4. 服务端验证 GMAC，验证通过后完成关联建立

### glo 加密流程

1. 建立关联后，客户端发送 glo-initiate-request 建立加密上下文
2. 后续所有 GET/SET/ACTION 请求使用 glo-封装：
   - 生成随机数（12 字节）
   - 使用 AES-GCM 或 Kuznyechik-CTR-CMAC 加密
   - 附加 invocation counter 防重放
3. 服务端解密后验证 IC，防止重放攻击

### 生产环境建议

- 为每个关联设置**唯一的** system_title、密钥和单调递增的 invocation counter
- 在不可信链路上使用 **HLS**（而非 LLS）
- 优先使用 suite 1/2 或 GOST suite 8/9
- **专用密钥**仅在 glo-initiate-request 中传输
- 重放保护：`osp_glo_unprotect` 拒绝 IC 非递增的请求
- 生产环境使用经过认证的加密模块，库自带的 GOST 仅用于可移植性和测试

---

## 项目结构

```
src/
├── codec/          BER/AXDR 编解码、COSEM 序列化
├── transport/      HDLC 会话 + COSEM Wrapper
├── service/        xDLMS APDUs、ACSE、GBT、initiate
├── security/       HLS、glo/ded、GOST（Streebog、Kuznyechik、GOST 34.10）
├── client/         会话客户端
├── server/         调度器 + accept 循环
├── ic/             42 个接口类实现
├── spodus/         СПОДУС 集中器运行时
├── hal/            平台抽象层（传输、加密、定时、随机数）
└── openspodes.h    主头文件，类型定义和错误码

tests/              CMocka 测试套件（14 个 CTest 目标，271 个测试函数）
examples/           回环、TCP 客户端/服务端、Linux HAL 演示
docs/               ARCHITECTURE.md、HANDOFF.md
thirdparty/         dlms-codec（参考实现）
scripts/            覆盖率报告脚本
```

---

## HAL 接口

OpenSPODES 通过函数指针结构体实现平台抽象，MCU 仅需实现以下接口：

| 接口结构体 | 用途 | 方法 |
|-----------|------|------|
| `osp_transport_t` | TCP/串口通信 | `open` / `send` / `recv` / `close` / `is_connected` |
| `osp_crypto_t` | 密码学操作 | `aes_gcm` / `md5` / `sha1` / `sha256` |
| `osp_random_t` | 随机数生成 | `generate` |
| `osp_timer_t` | 定时器 | `now_ms` / `delay_ms` |
| `osp_system_t` | 系统信息 | `system_title` / `key_store` |

完整的 Linux 实现参见 `examples/linux_hal.h` 和 `examples/linux_hal.c`。

---

## 参考资料

- [IEC 62056-5-3](https://webstore.iec.ch/) — xDLMS 应用层服务
- [IEC 62056-46](https://webstore.iec.ch/) — HDLC 会话层封装
- [IEC 62056-47](https://webstore.iec.ch/) — COSEM TCP/UDP 传输层封装
- [ISO/IEC 13239](https://webstore.iec.ch/) — HDLC 帧格式
- [Р 1323565.1](https://docs.cntd.ru/) — GOST 传输与 HLS 规范
- [spodes-rs](https://github.com/gvtret/spodes-rs) — Rust 参考实现（API 和测试向量对齐）
- [DLMS/COSEM](https://www.dlms.com/) — DLMS 用户协会官方网站
- [IEC 62056 标准族](https://webstore.iec.ch/) — IEC 国际电工委员会标准商城

---

## 许可证

GPL-3.0-or-later。参见 [LICENSE](LICENSE)。
