# ASR 自动停止配置

## 概述

在 `INTER_CONTINUE`（全双工）交互模式下，控制 TTS 应答完成后是否自动停止录音并进入待唤醒状态。

## 功能说明

### 问题背景

在全双工模式下，`recognizer_recognize_once_end()` 原本只释放音频焦点，没有停止录音和设置状态为 IDLE，导致 ASR 一直进行，无法正确进入待唤醒状态。

### 解决方案

通过全局变量 `g_auto_stop_record_on_tts_end` 控制是否在 TTS 结束后自动停止录音。

## API 接口

```c
#include "recognizer.h"

/**
 * @brief 设置 TTS 结束后是否自动停止录音
 * @param enable  true: 自动停止(默认), false: 保持录音
 */
void recognizer_set_auto_stop_record(bool enable);

/**
 * @brief 获取 TTS 结束后是否自动停止录音的设置
 * @return true: 自动停止, false: 保持录音
 */
bool recognizer_get_auto_stop_record(void);
```

## 使用示例

### 启用自动停止（默认行为）

```c
recognizer_set_auto_stop_record(true);
```

TTS 应答完成后：
- 停止发送音频到云端
- 释放音频焦点
- 状态设置为 IDLE
- 进入待唤醒状态

### 禁用自动停止

```c
recognizer_set_auto_stop_record(false);
```

TTS 应答完成后：
- 只释放音频焦点
- 保持录音状态
- 可继续进行语音交互

## 默认值

默认值为 `true`（启用自动停止）。

## 交互模式对比

| 交互模式 | 配置值 | TTS 结束后行为 |
|---------|-------|---------------|
| `INTER_ONESHOT` | - | 总是停止录音 |
| `INTER_CONTINUE` | `true` | 停止录音，进入待唤醒 |
| `INTER_CONTINUE` | `false` | 保持录音状态 |
| `INTER_BUTTON` | - | 由按键控制 |

## 相关文件

| 文件 | 说明 |
|-----|------|
| `src/cloud/recognizer/recognizer.h` | API 声明 |
| `src/cloud/recognizer/recognizer.c` | 实现 |
| `src/cloud/proc/proc_mgr.c` | 调用 `recognizer_recognize_once_end()` |
