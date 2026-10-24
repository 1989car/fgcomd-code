[English](README.md) | [简体中文](README.zh-CN.md)

# fgcomd-code

> **本仓库是 [SourceForge 上 fgcomd 项目](https://sourceforge.net/projects/fgcomd/) 的备份镜像，不是上游官方开发仓库。**
>
> 上游项目已长期停止维护（最后一次代码提交为 2011-06-14），且原始托管在 SourceForge 的 SVN 上，存在不可访问或丢失的风险，因此在此处保留一份完整快照作为备份。提交、拉取请求请以**上游**为准，镜像仓库仅用于保存与查阅。

## 镜像来源

| 项目 | 地址 |
| --- | --- |
| 上游项目主页 | <https://sourceforge.net/projects/fgcomd/> |
| 上游代码浏览 | <https://sourceforge.net/p/fgcomd/code/> |
| 上游 SVN 仓库（HTTPS） | <https://svn.code.sf.net/p/fgcomd/code/> |
| 上游 SVN 仓库（svn://） | `svn://svn.code.sf.net/p/fgcomd/code/` |
| 上游项目站点 | <https://fgcomd.sourceforge.io/> |
| 上游维护者 | Holger Wirtz（SourceForge 用户 `dcoredump`） |

### 快照版本

本镜像对应上游 SVN 的 **`trunk` 分支 r55**（提交时间 `2011-06-14 20:06:45 +0800`），文件清单与上游 `trunk` 目录逐项一致：

```bash
# 如需从上游拉取原始版本进行比对
svn checkout https://svn.code.sf.net/p/fgcomd/code/trunk
```

上游仓库结构为 `trunk` / `tags` / `branches` 三目录；本镜像仅收录 `trunk`（即开发主线），其中 `tags` 与 `branches` 未包含在内。

## 项目简介

fgcomd 是为飞行模拟器 **FlightGear** 实现的无线电（语音）会议服务器，用于在模拟器内进行多人无线电通话：

- 独立运行的守护进程，通过 UDP 与客户端通信；
- 客户端上传自身位置坐标，服务器据此计算用户间距离，并按距离做**通话距离衰减**与音量混合（超过 `MAX_DISTANCE` 即不再互相可闻）；
- 音频以 8 kHz 采样、单帧 256 字节的固定帧长在网络上传输，服务器侧带 AGC（自动增益控制）与底噪（squelch）处理；
- 协议报文在 `packet.h` 中定义，支持位置、用户信息、文字聊天、音频、ping 等命令。

官方定位是作为独立程序提供语音能力（FlightGear 内建语音支持当时尚未实现）。

## 源码组成

| 可执行程序 | 说明 |
| --- | --- |
| `fgcomd` | 主守护进程：管理用户与会议哈希表、监听 UDP 端口、分发网络报文 |
| `lighthouse` | 测试工具：模拟一个持续发送音频的客户端（信标） |
| `listener` | 测试工具：接收并播放服务器返回的音频（依赖 OpenAL） |

`Makefile` 中另有 `stress` 目标依赖 `stress.c`，但该源文件在上游 `trunk` 中并不存在，因此实际不可构建。

主要模块：

| 文件 | 职责 |
| --- | --- |
| `fgcomd.c` / `fgcomd.h` | 入口、信号处理、全局状态、版本与协议常量 |
| `config.c` / `config.h` | 命令行选项解析（`-v` / `-h` / `-p`） |
| `net.c` / `net.h` | UDP 套接字与网络分发线程 |
| `user.c` / `user.h` | 用户管理、位置更新、超时清理 |
| `conference.c` / `conference.h` | 会议（频道）管理与音频混合 |
| `packet.c` / `packet.h` | 报文结构与序列化 |
| `audio.c` / `audio.h` | AGC、底噪、混音等音频处理 |
| `distance.c` / `distance.h` | 用户间距离矩阵计算 |
| `data/audiodata.h` | 预生成的音频采样数据（约 6 MB） |

## 构建

### 依赖

- 编译工具链（GCC / Make）
- **GLib 2.x**（`glib-2.0`）
- **GNet 2.x**（`gnet-2.0`）
- **OpenAL + libalut**（仅 `listener` 需要）

Debian / Ubuntu 示例：

```bash
sudo apt install build-essential libglib2.0-dev libgnet2.0-dev libopenal-dev libalut-dev
```

### 编译

```bash
make            # 生成 fgcomd、lighthouse、listener
make clean      # 清理 *.o 与可执行文件
```

> `Makefile` 的 `CFLAGS` 中硬编码了 `/usr/lib/glib-2.0/include` 与 `/usr/lib/gnet-2.0/include` 路径，在发行版常见的 `/usr/include/glib-2.0` 布局下可能需要按本地环境调整。
>
> `Makefile` 通过 `svnversion` 注入版本号，在非 SVN 工作副本下 `SVN_REV` 会为空（编译仍可完成）。

## 运行

```bash
./fgcomd -h 0.0.0.0 -p 6463 -v
```

| 选项 | 含义 | 默认值 |
| --- | --- | --- |
| `-v` | 输出更详细日志 | 关闭 |
| `-h` | 监听地址 | `localhost` |
| `-p` | 监听端口 | `6463` |

启动时会以 `/etc/fgcomd.conf` 作为配置文件路径传入，但**当前实现中仅实际解析命令行选项，配置文件内容并未被读取**，如需自定义参数请使用命令行。

## 许可与版权

- 许可证：**GNU General Public License v3.0（GPLv3）**，与上游项目声明一致。
- 版权：`Copyright (C) 2006-2011 Holger Wirtz <dcoredump@gmail.com>`。
- 本镜像仓库中的全部源码均来自上游，未作功能修改，仅补充了本 README 与 Git 忽略规则。

## 关于镜像的说明

- 本仓库使用 Git 管理，上游使用 SVN，二者历史不互通；本仓库只有一个导入提交，不代表上游的完整提交历史（上游完整历史可在上游代码浏览页查看）。
- 若上游恢复更新，请以**上游仓库**为准，并将改动同步回本镜像。
- 镜像不接收功能开发需求；如需修改，请先在上游（或你 fork 的副本）进行，再同步至此。
