#!/usr/bin/env python3
"""LLM 网络键盘 —— ARCS-MINI 端侧 LLM 终端的 PC 输入端

设备侧对应固件模块: apps/arcs-mini/llmterm/net_kb.c
协议（UDP 端口 38001，前缀 LLMKB1:）:
  "LLMKB1:D"         PC 广播发现探测，设备单播应答 "LLMKB1:d arcs-mini <ip>"
  "LLMKB1:K" + 1字节  逐键实时发送（可打印原码 / '\n' 回车 / '\b' 退格）
  "LLMKB1:L" + 整行  整行输入（兼容保留，netcat 手测用）
  "LLMKB1:H"         心跳（设备 30s 无包判离线）

用法: python3 llm_keyboard.py    （依赖 PyQt5: pip install PyQt5）
"""

import socket
import sys
import time

from PyQt5.QtCore import Qt, QThread, pyqtSignal
from PyQt5.QtWidgets import (QApplication, QLabel, QMainWindow, QVBoxLayout,
                             QWidget)

PORT = 38001
MAGIC = b"LLMKB1:"
DISCOVER_INTERVAL = 2.0     # 发现广播周期（秒）
HEARTBEAT_INTERVAL = 5.0    # 心跳周期（秒）
PEER_TIMEOUT = 15.0         # 超过该时长无应答则视为离线，重回发现


class DiscoveryWorker(QThread):
    """后台线程：广播发现设备 → 锁定 IP → 心跳保活 + 探活"""

    state_changed = pyqtSignal(str)          # 状态栏文本
    device_found = pyqtSignal(str)           # 设备 IP

    def run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.settimeout(0.5)

        dev_ip = None
        last_seen = 0.0
        last_probe = 0.0
        last_beat = 0.0

        while True:
            now = time.time()

            if dev_ip is None:
                # 发现阶段：定时广播探测
                if now - last_probe >= DISCOVER_INTERVAL:
                    last_probe = now
                    try:
                        sock.sendto(MAGIC + b"D", ("255.255.255.255", PORT))
                        self.state_changed.emit("searching device ...")
                    except OSError as e:
                        self.state_changed.emit(f"broadcast error: {e}")
            else:
                # 已连接阶段：心跳 + 探活
                if now - last_beat >= HEARTBEAT_INTERVAL:
                    last_beat = now
                    try:
                        sock.sendto(MAGIC + b"H", (dev_ip, PORT))
                    except OSError:
                        pass
                if now - last_seen > PEER_TIMEOUT:
                    dev_ip = None
                    self.state_changed.emit("device lost, searching ...")
                    continue

            # 收应答（发现响应或任意设备包都刷新在线）
            try:
                data, addr = sock.recvfrom(256)
            except socket.timeout:
                continue
            if not data.startswith(MAGIC):
                continue
            body = data[len(MAGIC):]

            if body.startswith(b"d"):
                # 应答形如 "d arcs-mini 192.168.31.101"
                parts = body.decode(errors="replace").split()
                name = parts[1] if len(parts) > 1 else "unknown"
                if dev_ip is None:
                    dev_ip = addr[0]
                    self.device_found.emit(f"{name} @ {addr[0]}")
                    self.state_changed.emit(f"connected: {name} @ {addr[0]}")
            if dev_ip and addr[0] == dev_ip:
                last_seen = now


class KeyboardWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.dev_ip = None
        self.send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.echo = ""

        self.setWindowTitle("LLM Keyboard — ARCS-MINI")
        self.setMinimumSize(460, 200)
        self.setFocusPolicy(Qt.StrongFocus)

        root = QWidget()
        lay = QVBoxLayout(root)
        self.lbl_state = QLabel("starting ...")
        self.lbl_state.setStyleSheet("color:#666; font-weight:bold;")
        self.lbl_echo = QLabel(" ")
        self.lbl_echo.setStyleSheet(
            "font-family:monospace; font-size:16px; background:#111;"
            " color:#2f2; padding:8px; border-radius:4px;")
        self.lbl_hint = QLabel(
            "click this window and type — Enter sends the line to the LLM")
        self.lbl_hint.setStyleSheet("color:#999;")
        lay.addWidget(self.lbl_state)
        lay.addWidget(self.lbl_echo)
        lay.addWidget(self.lbl_hint)
        self.setCentralWidget(root)

        self.worker = DiscoveryWorker()
        self.worker.state_changed.connect(self.lbl_state.setText)
        self.worker.device_found.connect(self.on_device)
        self.worker.start()

    # ---- 设备连接 ----
    def on_device(self, info):
        self.dev_ip = info.split(" @ ")[-1]

    # ---- 逐键实时发送（每按一键立即发一包，设备屏上同步显示） ----
    def _send_key(self, ch: bytes):
        if not self.dev_ip:
            self.lbl_state.setText("no device — keys dropped")
            return
        try:
            self.send_sock.sendto(MAGIC + b"K" + ch, (self.dev_ip, PORT))
        except OSError as e:
            self.lbl_state.setText(f"send error: {e}")

    def _update_echo(self):
        show = self.echo[-40:]
        self.lbl_echo.setText(show + "_")

    def keyPressEvent(self, ev):
        # ↑↓ 滚动历史（ESC+'A'/'B'，设备侧终端回看）
        if ev.key() == Qt.Key_Up:
            self._send_key(b"\x1bA")
            return
        if ev.key() == Qt.Key_Down:
            self._send_key(b"\x1bB")
            return
        if not ev.text():
            return super().keyPressEvent(ev)
        ch = ev.text()
        if ch == "\r":
            self._send_key(b"\n")
            self.echo = ""
        elif ch == "\b":
            self._send_key(b"\b")
            self.echo = self.echo[:-1]
        elif " " <= ch <= "~":
            self._send_key(ch.encode("ascii"))
            self.echo += ch
        else:
            return super().keyPressEvent(ev)
        self._update_echo()


def main():
    app = QApplication(sys.argv)
    win = KeyboardWindow()
    win.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
