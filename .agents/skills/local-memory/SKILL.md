---
name: local-memory
description: 管理项目本地记忆时使用。用户提到 /local-memory、记住到本地、保存本地记忆、查看本地记忆、删除本地记忆时触发。
---

# 项目本地记忆

## 兼容约定

- 本技能同时适用于 Claude Code 和 Codex；不要依赖单一客户端专属工具名。
- 下文 `/xxx` 是触发示例；客户端不支持 slash 命令时，用对应自然语言请求即可。
- 命令默认从仓库根目录执行；不要硬编码个人机器上的绝对路径。

## 记忆文件

- 路径：`.agents/local/AGENTS.local.md`
- 用途：保存本机/本用户偏好，不提交仓库。
- 如果用户要求写入本地记忆且文件不存在，自动创建目录和文件。

## 写入规则

1. 只有用户明确要求“记住”“保存到本地记忆”时才写入。
2. 写入前先读取已有内容，避免重复和矛盾。
3. 不写入 token、密码、密钥等敏感信息。
4. 优先保存短小、稳定、可复用的信息，例如提交签名、默认 app、常用设备/串口。
5. 记录格式保持 Markdown，便于人工审查。

## 建议格式

```markdown
# Local Agent Notes

> 本文件为项目本地记忆，不提交仓库。

## Commit
- Co-Authored-By: tfzou-bot <tfzou@listenai.com>

## Build
- default_app: arcs-mini

## Devices
- mini_serial: /dev/ttyACM0
```
