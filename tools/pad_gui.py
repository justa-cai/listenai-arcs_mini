#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ARCS 手柄 GUI (Ubuntu 桌面端, Qt 版)
====================================
在 PC 上画一个游戏手柄, 鼠标点按 / 物理键盘 两套输入, 经 WebSocket/UDP
把按键发给 arcs-mini 上的 NES 游戏 (协议见 doc/gamepad-protocol.md)。

- 鼠标: 直接点画布上的十字键 / A / B / Select / Start; RESET 点击即复位
- 键盘: 方向键或 WASD -> 十字键, X/K -> A, Z/J -> B, Enter -> Start,
        Space -> Select, R -> 复位, Esc -> 退出程序
        (焦点在输入框 (IP/过滤) 时不拦截按键, 可正常输入)
- 连接: 不填 IP 直接启动会**自动扫描并连上**发现的设备; 也可手动输入 IP 点「连接」;
        点「扫描」用 UDP 广播重新发现 (支持多网卡/存在 VPN 的机器)
- 按键通道: 固件 welcome 带 udp_port 时自动走 UDP 全量位图 (低延迟 + 弱网自愈),
        否则回退 WS 差分事件; WS 始终保留 ROM 推送 / 状态 / 心跳 / 命令
- ROM 库 (左侧): 递归列出目录内 .nes 文件 (含子目录), 过滤框即时筛选,
        选中看信息 (大小/mapper/PRG/CHR), 双击或「推送」即热切换游戏 (协议 §4.2.1)。
        ROM 目录: 默认为项目自带 roms/, 可用按钮/第 2 个启动参数/PAD_ROM_DIR
        环境变量指向任意目录, 选择会记入 ~/.config/arcs_pad_gui.json。
        **只推送合法来源的 ROM** (自制/homebrew/已获授权)
- ROM HTTP API (只读): 启动即在后台拉起一个轻量 HTTP 服务 (默认 0.0.0.0:38202),
        供二次开发/网页端查询 ROM 文件信息并拉取 ROM 原始字节; 服务地址显示在
        ROM 卡片内 (可选中复制)。端口用 PAD_HTTP_PORT 覆盖, 监听地址用 PAD_HTTP_BIND。
        接口清单与调用示例见 doc/pad-http-api.md。

依赖: PyQt5 或 PySide2 (任一), websockets
用法:
    python3 tools/pad_gui.py [设备IP] [ROM目录]
"""
import asyncio
import fcntl
import glob
import http.server
import json
import os
import queue
import socket
import struct
import select
import sys
import threading
import time
import urllib.parse
import zlib
from pathlib import Path

try:
    import websockets
except ImportError:
    sys.exit("缺少依赖: pip install websockets")

try:  # Qt 绑定自适应: 优先 PyQt5, 兼容 PySide2 (均为 Qt5 API)
    from PyQt5 import QtCore, QtGui, QtWidgets
except ImportError:
    from PySide2 import QtCore, QtGui, QtWidgets

WS_PORT = 38200
DISC_PORT = 38201
HTTP_PORT = 38202         # ROM 查询/下载 HTTP API 默认端口 (PAD_HTTP_PORT 可覆盖)
JSON_CT = "application/json; charset=utf-8"
PATH = "/gamepad"
ROM_CHUNK = 4096          # ROM 推送分块 (二进制 WS 帧)
ROM_MAX = 1024 * 1024     # 与设备端 nes_rom 分区对齐

# UDP 按键通道 (协议 §4.5.1): 全量位图帧, 20Hz 周期重发 (应用层弱网自愈)
UDP_MAGIC = 0xA5
UDP_VER = 1
UDP_REFRESH_S = 0.05      # 与设备端 GAMEPAD_UDP_REFRESH_MS 对齐
KEY_BITS = {"up": 1 << 11, "down": 1 << 10, "left": 1 << 9, "right": 1 << 8,
            "a": 1 << 15, "b": 1 << 14, "select": 1 << 13, "start": 1 << 12}

# 用户偏好持久化 (记住上次选择的 ROM 目录); 默认目录固定为项目自带 roms/,
# 不做其它硬编码 —— 指向哪个目录由使用者自己选择
CONFIG_PATH = Path.home() / ".config" / "arcs_pad_gui.json"


def _load_saved_rom_dir():
    try:
        return json.loads(CONFIG_PATH.read_text(encoding="utf-8")).get("rom_dir", "")
    except Exception:
        return ""


def _save_rom_dir(d):
    try:
        CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
        CONFIG_PATH.write_text(json.dumps({"rom_dir": d}), encoding="utf-8")
    except Exception:
        pass

# ---- 设计 token (单一来源; 布局遵循 4/8/12 间距节奏, 卡片 10px 圆角) ----
BG = "#0F1115"        # 窗口底
SURFACE = "#161A22"   # 卡片
SURFACE2 = "#1D2330"  # 卡片内控件 (输入框/列表)
BORDER = "rgba(255,255,255,26)"       # 8% 白描边
ACCENT = "#4F8CFF"    # 主强调 (唯一 primary)
ACCENT_HOVER = "#6FA3FF"
ACCENT_PRESS = "#3E78E6"
SUCCESS = "#5AD27A"
DANGER = "#E05F5F"
TXT = "#E8ECF4"       # 主文本 (对比度 ~13:1)
MUT = "#8A93A6"       # 次文本 (~5:1)
DIM = "#5C6474"       # 弱提示

# 手柄画布专用
DPAD = "#242B3A"
DPAD_ON = ACCENT
BTN_BG = "#242B3A"
BTN_ON = ACCENT
RESET_BG = "#4A3038"

STYLESHEET = f"""
QWidget {{ background: {BG}; color: {TXT}; font-family: 'Sans'; font-size: 13px; }}

QFrame#card {{ background: {SURFACE}; border: 1px solid {BORDER}; border-radius: 10px; }}
QFrame#subcard {{ background: {SURFACE2}; border: 1px solid {BORDER}; border-radius: 6px; }}

QLabel#h1 {{ font-size: 15px; font-weight: bold; }}
QLabel#muted {{ color: {MUT}; font-size: 12px; }}
QLabel#dim {{ color: {DIM}; font-size: 11px; }}
QLabel#dot {{ font-size: 11px; }}

QLineEdit {{
    background: {SURFACE2}; color: {TXT}; border: 1px solid {BORDER};
    border-radius: 6px; padding: 5px 10px; font-family: 'Mono'; font-size: 13px;
    selection-background-color: {ACCENT};
}}
QLineEdit:focus {{ border-color: {ACCENT}; }}

QListWidget {{
    background: {SURFACE2}; color: {TXT}; border: 1px solid {BORDER};
    border-radius: 6px; font-family: 'Mono'; font-size: 12px;
    outline: none;
}}
QListWidget::item {{ padding: 6px 8px; border-radius: 4px; }}
QListWidget::item:hover {{ background: {SURFACE}; }}
QListWidget::item:selected {{ background: {ACCENT}; color: white; }}

QScrollBar:vertical {{ background: transparent; width: 8px; margin: 2px; }}
QScrollBar::handle:vertical {{ background: {SURFACE}; border-radius: 4px; min-height: 24px; }}
QScrollBar::handle:vertical:hover {{ background: {DIM}; }}
QScrollBar::add-line, QScrollBar::sub-line {{ height: 0; }}
QScrollBar::add-page, QScrollBar::sub-page {{ background: transparent; }}

QPushButton {{
    background: transparent; color: {TXT}; border: 1px solid {BORDER};
    border-radius: 6px; padding: 6px 14px; min-height: 26px;
}}
QPushButton:hover {{ border-color: {ACCENT}; color: {ACCENT_HOVER}; }}
QPushButton:pressed {{ background: {SURFACE2}; }}

QPushButton#primary {{
    background: {ACCENT}; color: white; border: none; font-weight: bold;
}}
QPushButton#primary:hover {{ background: {ACCENT_HOVER}; }}
QPushButton#primary:pressed {{ background: {ACCENT_PRESS}; }}
QPushButton#primary:disabled {{ background: {DIM}; }}

QPushButton#conn_btn[danger="true"] {{ background: {DANGER}; border: none; }}
QPushButton#conn_btn[danger="true"]:hover {{ background: #E87878; }}
"""


# ======================================================================
# WebSocket + UDP 客户端 (asyncio 跑在后台线程, GUI 侧经事件队列汇入)
# ======================================================================
class PadClient:
    def __init__(self, on_event):
        self.on_event = on_event        # 回调(event, payload) -> 从后台线程调, GUI 侧需 marshal
        self.loop = None
        self.ws = None
        self._thread = None
        self._stop = threading.Event()
        self._down = {}                 # 参考计数: key -> 按下次数(鼠标+键盘可能同时按)
        self._lock = threading.Lock()
        self._out_queue = queue.Queue()
        self._ping_sent = {}
        # UDP 按键通道 (§4.5.1): welcome 协商 udp_port 后启用, 按键不再走 WS
        self._ip = ""
        self.udp_addr = None            # (ip, port), None = 走 WS 差分事件
        self._udp_sock = None
        self._mask = 0
        self._seq = 0
        self.sent_count = 0             # 累计发送帧数 (丢包率分母, GUI 读取)

    # ---- 生命周期 ----
    def connect(self, ip):
        self.disconnect()
        self._stop.clear()
        self._ip = ip
        self._thread = threading.Thread(target=self._thread_main, args=(ip,), daemon=True)
        self._thread.start()

    def disconnect(self):
        self._stop.set()
        if self.loop and self.ws:
            try:
                asyncio.run_coroutine_threadsafe(self.ws.close(), self.loop)
            except Exception:
                pass
        self.udp_addr = None
        self._thread = None
        self.loop = None
        self.ws = None

    @property
    def connected(self):
        return self.ws is not None

    # ---- 后台线程 ----
    def _thread_main(self, ip):
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        self.loop = loop
        try:
            loop.run_until_complete(self._session(ip))
        except Exception as e:
            self.on_event("st", {"text": f"连接失败: {e}", "ok": False})
        finally:
            try:
                loop.close()
            except Exception:
                pass
            self.ws = None
            self.loop = None

    async def _session(self, ip):
        url = f"ws://{ip}:{WS_PORT}{PATH}"
        self.on_event("st", {"text": f"连接中 {url} ...", "ok": False})
        async with websockets.connect(url, open_timeout=5, ping_interval=None) as ws:
            self.ws = ws
            self.on_event("st", {"text": "已连接", "ok": True})
            rx = asyncio.create_task(self._recv_loop(ws))
            tx = asyncio.create_task(self._send_loop(ws))
            pinger = asyncio.create_task(self._ping_loop(ws))
            refresher = asyncio.create_task(self._udp_refresh_loop())
            done, pending = await asyncio.wait(
                {rx, tx, pinger, refresher}, return_when=asyncio.FIRST_COMPLETED)
            for t in pending:
                t.cancel()
        self.udp_addr = None
        self.on_event("st", {"text": "连接已断开", "ok": False})

    async def _recv_loop(self, ws):
        async for raw in ws:
            try:
                msg = json.loads(raw)
            except (ValueError, TypeError):
                continue
            if msg.get("t") == "pong":
                t0 = self._ping_sent.pop(msg.get("ts"), None)
                if t0 is not None:
                    self.on_event("rtt", (time.time() - t0) * 1000.0)
            elif msg.get("t") == "welcome" and msg.get("udp_port"):
                # UDP 端口协商 (§4.5.1): 此后按键走 UDP 全量位图
                self._enable_udp(msg["udp_port"])
            self.on_event("rx", msg)

    def _enable_udp(self, port):
        if not self._ip:
            return
        if not self._udp_sock:
            self._udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._seq = 0
        self._mask = 0
        self.sent_count = 0
        self.udp_addr = (self._ip, int(port))
        self.on_event("st", {"text": f"按键走 UDP:{port} (WS 保留控制/ROM)", "ok": True})

    def _send_udp(self):
        """8 字节全量位图帧: magic/ver/seq(BE32)/mask(BE16); 发送失败静默(下个重发周期自愈)"""
        addr = self.udp_addr
        if not addr or not self._udp_sock:
            return
        with self._lock:
            self._seq += 1
            seq = self._seq
            mask = self._mask
        frame = bytes((UDP_MAGIC, UDP_VER,
                       (seq >> 24) & 0xFF, (seq >> 16) & 0xFF,
                       (seq >> 8) & 0xFF, seq & 0xFF,
                       (mask >> 8) & 0xFF, mask & 0xFF))
        self.sent_count += 1
        try:
            self._udp_sock.sendto(frame, addr)
        except OSError:
            pass

    async def _udp_refresh_loop(self):
        """20Hz 周期重发当前位图 (丢包自愈 + 隐式保活)"""
        while True:
            if self.udp_addr:
                self._send_udp()
            await asyncio.sleep(UDP_REFRESH_S)

    async def _send_loop(self, ws):
        q = self._out_queue
        while True:
            try:
                msg = q.get_nowait()
            except queue.Empty:
                await asyncio.sleep(0.005)
                continue
            if isinstance(msg, (bytes, bytearray, memoryview)):
                await ws.send(msg)          # bytes -> 二进制帧 (ROM 分块, §4.2.1)
            else:
                await ws.send(json.dumps(msg))

    async def _ping_loop(self, ws):
        while True:
            await asyncio.sleep(1.5)
            ts = int(time.time() * 1000)
            self._ping_sent[ts] = time.time()
            await ws.send(json.dumps({"t": "ping", "ts": ts}))

    # ---- GUI 线程调用 ----
    def _emit(self, msg):
        if not self.loop or not self.ws:
            return
        self._out_queue.put(msg)

    def set_key(self, key, pressed):
        """按下/抬起(参考计数)。返回 True 表示状态确实变了(需刷新 UI)。
        UDP 通道: 立即发全量位图; WS 通道(无 udp_port 的旧固件): 发差分事件。"""
        with self._lock:
            cnt = self._down.get(key, 0)
            if pressed:
                self._down[key] = cnt + 1
                changed = cnt == 0
                if changed:
                    self._mask |= KEY_BITS[key]
            else:
                if cnt <= 0:
                    return False
                self._down[key] = cnt - 1
                changed = self._down[key] == 0
                if changed:
                    self._mask &= ~KEY_BITS[key]
        if changed:
            if self.udp_addr:
                self._send_udp()
            else:
                self._emit({"t": "k", "k": key, "v": 1 if pressed else 0})
        return changed

    def release_all(self):
        with self._lock:
            pressed = [k for k, c in self._down.items() if c > 0]
            self._down.clear()
            self._mask = 0
        if self.udp_addr:
            self._send_udp()
        else:
            for k in pressed:
                self._emit({"t": "k", "k": k, "v": 0})

    def send_hello(self, name):
        self._emit({"t": "hello", "ver": 1, "client": "ubuntu-gui", "name": name, "token": ""})

    def send_cmd(self, c):
        """主机命令: reset (重载当前 ROM) / exit (退出游戏)"""
        self._emit({"t": "cmd", "c": c})

    def send_rom(self, data, name):
        """推送本地 ROM (协议 §4.2.1): rom_begin -> 4KB 二进制分块 -> rom_end。
        返回 (ok, msg); 队列异步发送, 结果由 rom_ack 回报。"""
        if not self.loop or not self.ws:
            return False, "未连接设备"
        if len(data) < 16 or len(data) > ROM_MAX:
            return False, f"文件大小非法: {len(data)} 字节 (需 16..{ROM_MAX})"
        if data[:4] != b"NES\x1a":
            return False, "不是 iNES/NES2.0 镜像 (文件头错误)"
        self._emit({"t": "rom_begin", "size": len(data),
                    "crc32": zlib.crc32(data) & 0xFFFFFFFF, "name": name})
        for off in range(0, len(data), ROM_CHUNK):
            self._out_queue.put(data[off:off + ROM_CHUNK])   # bytes -> 二进制帧
        self._emit({"t": "rom_end"})
        return True, f"推送中 {len(data) // 1024} KB ..."

    def cancel_rom(self):
        self._emit({"t": "rom_cancel"})


# ======================================================================
# 发现
# ======================================================================
_SIOCGIFADDR = 0x8915
_SIOCGIFNETMASK = 0x891b


def _iface_ipv4():
    """枚举本机 IPv4 接口: [(name, ip, netmask), ...] (Linux ioctl)"""
    out = []
    for _, name in socket.if_nameindex():
        try:
            ip = _ioctl_addr(name, _SIOCGIFADDR)
            mask = _ioctl_addr(name, _SIOCGIFNETMASK)
        except OSError:
            continue        # 无 IPv4 的接口
        if ip and mask:
            out.append((name, ip, mask))
    return out


def _ioctl_addr(name, cmd):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        req = struct.pack("256s", name.encode()[:15])
        res = fcntl.ioctl(s.fileno(), cmd, req)
        return socket.inet_ntoa(res[20:24])
    finally:
        s.close()


def _subnet_broadcast(ip, mask):
    """由 ip/掩码算定向广播地址, 如 192.168.31.205/24 -> 192.168.31.255"""
    i = struct.unpack("!I", socket.inet_aton(ip))[0]
    m = struct.unpack("!I", socket.inet_aton(mask))[0]
    return socket.inet_ntoa(struct.pack("!I", (i & m) | (~m & 0xFFFFFFFF)))


def discover(timeout=2.0, http_port=None):
    """在所有本机接口上做 UDP 广播探测, 返回 announce 列表。

    关键: 逐接口 bind 到该网卡 IP 再发定向广播 —— 多网卡/存在 VPN TUN
    (docker0 / Mihomo / anbox0 等) 时, 不绑定会让探测走错网卡, 应答回不来。

    探测里自报 client 身份与 ROM HTTP API 端口: 设备据此反向确认"ROM 库服务
    在哪台机器、哪个端口", 不必依赖 WebSocket 会话 (见 设备侧
    gamepad_get_server_addr)。身份串含 "gui" 才被设备当作桌面端。
    """
    probe_obj = {"t": "discover", "ver": 1, "client": "ubuntu-gui"}
    if http_port:
        probe_obj["http_port"] = int(http_port)
    probe = json.dumps(probe_obj).encode()
    socks = []
    for name, ip, mask in _iface_ipv4():
        if ip.startswith("127."):
            continue
        bcast = _subnet_broadcast(ip, mask)
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind((ip, 0))
            s.setblocking(False)
            for target in (bcast, "255.255.255.255"):
                try:
                    s.sendto(probe, (target, DISC_PORT))
                except OSError:
                    pass
            socks.append(s)
        except OSError:
            s.close()

    found = {}
    end = time.time() + timeout
    while socks and time.time() < end:
        r, _, _ = select.select(socks, [], [], max(0.0, end - time.time()))
        for s in r:
            try:
                data, _ = s.recvfrom(2048)
            except OSError:
                continue
            try:
                msg = json.loads(data)
            except ValueError:
                continue
            if msg.get("t") == "announce":
                found[msg.get("did") or msg.get("ip")] = msg
    for s in socks:
        s.close()
    return list(found.values())


# ======================================================================
# ROM 元数据解析 (GUI 与 HTTP API 共用)
# ======================================================================
def _parse_ines_meta(path):
    """解析 iNES / NES2.0 头, 返回结构化元数据 dict; 非 iNES 镜像返回 None。"""
    try:
        with open(path, "rb") as fp:
            hdr = fp.read(16)
    except OSError:
        return None
    if len(hdr) < 16 or hdr[:4] != b"NES\x1a":
        return None
    flags6, flags7 = hdr[6], hdr[7]
    prg_kb = hdr[4] * 16
    chr_kb = hdr[5] * 8
    mapper = (flags6 >> 4) | (flags7 & 0xF0)
    nes2 = (flags7 & 0x0C) == 0x08
    return {
        "format": "NES2.0" if nes2 else "iNES",
        "mapper": mapper,
        "prg_kb": prg_kb,
        "chr_kb": chr_kb,
        "chr_ram": chr_kb == 0,
        "mirroring": "vertical" if (flags6 & 0x01) else "horizontal",
        "battery": bool(flags6 & 0x02),
        "trainer": bool(flags6 & 0x04),
        "four_screen": bool(flags6 & 0x08),
    }


# ======================================================================
# ROM HTTP API (只读; 供二次开发 / 网页端查询文件信息并拉取 ROM)
# ======================================================================
class RomHttpApi:
    """ROM 查询/下载 HTTP 服务的只读后端。

    与 GUI 完全解耦: 只通过 3 个回调取当前快照, 不直接触碰 Qt 对象。
      get_roms()     -> [(rel_path, abs_path), ...]  当前 ROM 全量列表
      get_device()   -> dict                          设备连接快照
      get_rom_dir()  -> str                           当前 ROM 目录

    路由 (全部 GET, 幂等只读):
      GET /                          服务信息 + 端点清单
      GET /api/info                  同上 (稳定入口)
      GET /api/roms?q=&offset=&limit=&with_ines=   ROM 列表 (可过滤/分页)
      GET /api/roms/{id}             单个 ROM 详情 (id = 相对路径)
      GET /api/roms/{id}/download    下载 ROM 原始字节
      GET /api/device                设备连接快照
    """

    def __init__(self, get_roms, get_device, get_rom_dir,
                 bind="0.0.0.0", port=HTTP_PORT):
        self._get_roms = get_roms
        self._get_device = get_device
        self._get_rom_dir = get_rom_dir
        self.bind = bind
        self.port = port
        self.lan_ip = self._detect_lan_ip(bind)
        self._ines_cache = {}       # abs_path -> ((mtime, size), meta|None)
        self._httpd = None
        self._thread = None

    # ---- 生命周期 ----
    @staticmethod
    def _detect_lan_ip(bind):
        """给 UI 展示用: 把 0.0.0.0 解析成本机首个非回环 IPv4。"""
        if bind not in ("0.0.0.0", "", "::"):
            return bind
        for _name, ip, _mask in _iface_ipv4():
            if not ip.startswith("127."):
                return ip
        return "127.0.0.1"

    def start(self):
        api = self

        class _Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"   # 每个响应都带 Content-Length, 可长连接

            def do_GET(self):
                api._serve(self)

            def do_HEAD(self):
                api._serve(self, head=True)

            def do_OPTIONS(self):
                self.send_response(204)
                self.send_header("Access-Control-Allow-Origin", "*")
                self.send_header("Access-Control-Allow-Methods", "GET, HEAD, OPTIONS")
                self.send_header("Access-Control-Allow-Headers", "*")
                self.send_header("Content-Length", "0")
                self.end_headers()

            def log_message(self, _fmt, *args):
                pass    # 静音: 不向终端刷访问日志

        self._httpd = http.server.ThreadingHTTPServer((self.bind, self.port), _Handler)
        self._httpd.daemon_threads = True
        self._thread = threading.Thread(target=self._httpd.serve_forever,
                                        name="rom-http", daemon=True)
        self._thread.start()

    def stop(self):
        if self._httpd:
            try:
                self._httpd.shutdown()
                self._httpd.server_close()
            except Exception:
                pass
            self._httpd = None

    # ---- HTTP 收发 ----
    def _serve(self, h, head=False):
        try:
            parts = urllib.parse.urlsplit(h.path)
            path = urllib.parse.unquote(parts.path)
            query = urllib.parse.parse_qs(parts.query)
            status, ctype, body, extra = self._route(path, query, h)
        except (BrokenPipeError, ConnectionResetError):
            return
        except Exception as e:                       # 兜底: 任何异常都回 JSON 而不是断连
            status, ctype, extra = 500, JSON_CT, {}
            body = _json({"error": str(e), "status": 500})
        # HEAD: 仍按 GET 的实体长度回 Content-Length, 只是不写 body
        h.send_response(status)
        h.send_header("Content-Type", ctype)
        h.send_header("Content-Length", str(len(body)))
        h.send_header("Access-Control-Allow-Origin", "*")
        for k, v in extra.items():
            h.send_header(k, v)
        h.end_headers()
        if head:
            return
        try:
            h.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    # ---- 路由 ----
    def _route(self, path, q, h):
        base = self._base_url(h)
        if path in ("/", "/api", "/api/info"):
            return 200, JSON_CT, _json(self._index(base)), {}
        if path == "/api/device":
            return 200, JSON_CT, _json(self._get_device()), {}
        if path == "/api/roms":
            return self._list_roms(base, q)
        if path.startswith("/api/roms/"):
            rest = path[len("/api/roms/"):]
            if rest.endswith("/download"):
                return self._download(rest[:-len("/download")])
            return self._detail(rest, base)
        return 404, JSON_CT, _json({"error": f"未知端点: {path}", "status": 404}), {}

    def _base_url(self, h):
        """优先用请求的 Host 头拼绝对地址, 便于客户端直接复用返回的 url 字段。"""
        host = h.headers.get("Host")
        return f"http://{host}" if host else f"http://{self.lan_ip}:{self.port}"

    # ---- 各端点实现 ----
    def _index(self, base):
        roms = self._get_roms()
        return {
            "service": "ARCS Pad GUI ROM API",
            "version": 1,
            "base_url": base,
            "rom_dir": self._get_rom_dir(),
            "rom_count": len(roms),
            "device": self._get_device(),
            "endpoints": [
                {"method": "GET", "path": "/api/info",
                 "desc": "服务信息与端点清单"},
                {"method": "GET", "path": "/api/roms",
                 "desc": "ROM 列表", "query": {
                     "q": "名称/路径子串过滤 (大小写不敏感)",
                     "offset": "起始下标 (默认 0)",
                     "limit": "返回上限, 1..2000 (默认 200)",
                     "with_ines": "是否解析 iNES 头, 1/0 (默认 1)"}},
                {"method": "GET", "path": "/api/roms/{id}",
                 "desc": "单个 ROM 详情 (id = 相对路径, 可 URL 编码)"},
                {"method": "GET", "path": "/api/roms/{id}/download",
                 "desc": "下载 ROM 原始字节"},
                {"method": "GET", "path": "/api/device",
                 "desc": "设备连接快照"},
            ],
        }

    def _list_roms(self, base, q):
        items = self._get_roms()                       # 快照: [(rel, abs), ...]
        total = len(items)
        qtxt = (q.get("q", [""])[0] or "").strip().casefold()
        if qtxt:
            items = [(rel, p) for rel, p in items if qtxt in rel.casefold()]
        offset = _int_arg(q, "offset", 0, 0)
        limit = min(_int_arg(q, "limit", 200, 1), 2000)
        with_ines = (q.get("with_ines", ["1"])[0] or "1") not in ("0", "false", "no")
        page = items[offset:offset + limit]
        return 200, JSON_CT, _json({
            "total": total,
            "matched": len(items),
            "offset": offset,
            "limit": limit,
            "q": qtxt,
            "roms": [self._entry(rel, p, base, with_ines) for rel, p in page],
        }), {}

    def _detail(self, rid, base):
        abs_path = self._rom_map().get(rid)
        if not abs_path:
            return 404, JSON_CT, _json({"error": f"未找到 ROM: {rid}", "status": 404}), {}
        return 200, JSON_CT, _json(self._entry(rid, abs_path, base, True)), {}

    def _download(self, rid):
        abs_path = self._rom_map().get(rid)
        if not abs_path:
            return 404, JSON_CT, _json({"error": f"未找到 ROM: {rid}", "status": 404}), {}
        try:
            with open(abs_path, "rb") as fp:
                data = fp.read()
        except OSError as e:
            return 500, JSON_CT, _json({"error": str(e), "status": 500}), {}
        fname = os.path.basename(rid)
        ascii_fname = fname.encode("ascii", "replace").decode("ascii")
        disp = (f"attachment; filename=\"{ascii_fname}\"; "
                f"filename*=UTF-8''{urllib.parse.quote(fname)}")
        return 200, "application/octet-stream", data, {"Content-Disposition": disp}

    # ---- 工具 ----
    def _rom_map(self):
        """当前列表的 {rel_path: abs_path}; 只服务已知文件, 天然免疫路径穿越。"""
        return {rel: p for rel, p in self._get_roms()}

    def _entry(self, rel, abs_path, base, with_ines):
        try:
            st = os.stat(abs_path)
            size, mtime = st.st_size, int(st.st_mtime)
        except OSError:
            size, mtime = 0, 0
        qid = urllib.parse.quote(rel)
        entry = {
            "id": rel,
            "name": os.path.basename(rel),
            "dir": os.path.dirname(rel),
            "rel_path": rel,
            "ext": os.path.splitext(rel)[1].lstrip(".").lower(),
            "size_bytes": size,
            "size_kb": size // 1024,
            "mtime": mtime,
            "url": f"{base}/api/roms/{qid}",
            "download_url": f"{base}/api/roms/{qid}/download",
        }
        if with_ines:
            entry["ines"] = self._ines(abs_path)
        return entry

    def _ines(self, abs_path):
        """按 (mtime, size) 缓存 iNES 解析结果, 避免重复读盘。"""
        try:
            st = os.stat(abs_path)
            key = (st.st_mtime, st.st_size)
        except OSError:
            return None
        cached = self._ines_cache.get(abs_path)
        if cached and cached[0] == key:
            return cached[1]
        meta = _parse_ines_meta(abs_path)
        self._ines_cache[abs_path] = (key, meta)
        return meta


def _json(obj):
    return json.dumps(obj, ensure_ascii=False, indent=2).encode("utf-8")


def _int_arg(q, name, default, minimum):
    try:
        return max(minimum, int(q.get(name, [default])[0]))
    except (ValueError, TypeError):
        return default


# ======================================================================
# 服务端发现 (设备主动找 GUI)
# ======================================================================
class RomDiscoveryServer:
    """监听 UDP 38201 (与设备发现服务同端口), 应答设备发来的
    {"t":"discover_server"}。

    方向与 `discover()` 相反: 那个是 GUI 广播找设备, 这个是**设备广播找 GUI**
    —— 设备据此拿到本机 ROM HTTP API 的地址与端口, 不必先由 GUI 扫描过设备
    (设备的 MCP rom_search / rom_load 会主动探测)。

    只应答, 不主动发包; 收到别的东西一律忽略 (例如自己的 discover 广播)。
    """

    def __init__(self, get_port, get_rom_count, bind="0.0.0.0", port=DISC_PORT):
        self._get_port = get_port
        self._get_rom_count = get_rom_count
        self.bind = bind
        self.port = port
        self._sock = None
        self._thread = None

    def start(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        s.bind((self.bind, self.port))
        self._sock = s
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        while True:
            try:
                self._sock.settimeout(0.5)
                data, addr = self._sock.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                return          # socket 被关闭 / 出错, 线程退出
            try:
                msg = json.loads(data)
            except ValueError:
                continue
            if not isinstance(msg, dict) or msg.get("t") != "discover_server":
                continue
            reply = {
                "t": "server",
                "ver": 1,
                "client": "ubuntu-gui",
                "ip": self._local_ip(addr),
                "http_port": self._get_port(),
                "rom_count": self._get_rom_count(),
            }
            try:
                self._sock.sendto(json.dumps(reply).encode(), addr)
            except OSError:
                pass

    def _local_ip(self, addr):
        """本机与请求方同网段的接口 IP (供设备交叉核对, 设备实际以应答源地址为准)。"""
        try:
            probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            try:
                probe.connect((addr[0], self.port))
                return probe.getsockname()[0]
            finally:
                probe.close()
        except OSError:
            return ""

    def stop(self):
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None


# ======================================================================
# GUI (Qt)
# ======================================================================
DPAD_KEYS = ("up", "down", "left", "right")

# 手柄画布几何 (与 tkinter 版一致): (key, cx, cy) 或矩形区域
PAD_SIZE = (620, 320)
DPAD_CX, DPAD_CY, DPAD_ARM, DPAD_THICK = 130, 160, 34, 44
BTN_B = ("b", 430, 200, 40)
BTN_A = ("a", 520, 140, 40)
SEL_RECT = ("select", 285, 250, 70, 30)
START_RECT = ("start", 375, 250, 70, 30)
RESET_RECT = QtCore.QRect(425, 44, 100, 28)


class PadCanvas(QtWidgets.QWidget):
    """自绘 NES 手柄: 十字键/A/B/SELECT/START 按住型, RESET 点击型"""

    reset_clicked = None   # 由 GUI 注入回调

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(*PAD_SIZE)
        self.pressed = set()
        self._mouse_key = None     # 本次鼠标按住的那个键 (释放跟它, 不跟位置)
        self.reset_clicked = lambda: None

    def set_pressed(self, key, on):
        if on:
            self.pressed.add(key)
        else:
            self.pressed.discard(key)
        self.update()

    # ---- 命中检测 ----
    def _key_at(self, pos):
        x, y = pos.x(), pos.y()
        half = DPAD_THICK // 2
        zones = (
            ("up", QtCore.QRect(DPAD_CX - half, DPAD_CY - DPAD_ARM - half,
                                DPAD_THICK, DPAD_ARM)),
            ("down", QtCore.QRect(DPAD_CX - half, DPAD_CY + half,
                                  DPAD_THICK, DPAD_ARM)),
            ("left", QtCore.QRect(DPAD_CX - DPAD_ARM - half, DPAD_CY - half,
                                  DPAD_ARM, DPAD_THICK)),
            ("right", QtCore.QRect(DPAD_CX + half, DPAD_CY - half,
                                   DPAD_ARM, DPAD_THICK)),
        )
        for k, r in zones:
            if r.contains(pos):
                return k
        for key, cx, cy, r in (BTN_B, BTN_A):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                return key
        for key, cx, cy, w, h in (SEL_RECT, START_RECT):
            if abs(x - cx) <= w / 2 and abs(y - cy) <= h / 2:
                return key
        return None

    def mousePressEvent(self, ev):
        if ev.button() != QtCore.Qt.LeftButton:
            return
        if RESET_RECT.contains(ev.pos()):
            self.reset_clicked()
            return
        key = self._key_at(ev.pos())
        if key:
            self._mouse_key = key
            self._emit_key(key, True)

    def mouseReleaseEvent(self, ev):
        if ev.button() != QtCore.Qt.LeftButton:
            return
        if self._mouse_key:
            self._emit_key(self._mouse_key, False)
            self._mouse_key = None

    def _emit_key(self, key, pressed):
        gui = self.window()
        if isinstance(gui, PadGUI):
            gui.key_input(key, pressed)

    def paintEvent(self, _ev):
        p = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.Antialiasing)
        p.setPen(QtCore.Qt.NoPen)

        # 画布底色与卡片内层一致
        p.setBrush(QtGui.QColor(SURFACE2))
        p.drawRect(self.rect())

        # 十字键
        half = DPAD_THICK // 2
        arms = (
            ("up", DPAD_CX - half, DPAD_CY - DPAD_ARM - half, DPAD_THICK, DPAD_ARM, "▲"),
            ("down", DPAD_CX - half, DPAD_CY + half, DPAD_THICK, DPAD_ARM, "▼"),
            ("left", DPAD_CX - DPAD_ARM - half, DPAD_CY - half, DPAD_ARM, DPAD_THICK, "◀"),
            ("right", DPAD_CX + half, DPAD_CY - half, DPAD_ARM, DPAD_THICK, "▶"),
        )
        for k, x, y, w, h, sym in arms:
            p.setBrush(QtGui.QColor(DPAD_ON if k in self.pressed else DPAD))
            p.drawRect(x, y, w, h)
            p.setPen(QtGui.QColor(TXT))
            p.setFont(QtGui.QFont("Sans", 12))
            p.drawText(QtCore.QRect(x, y, w, h), QtCore.Qt.AlignCenter, sym)
            p.setPen(QtCore.Qt.NoPen)

        # A / B
        for key, cx, cy, r in (BTN_B, BTN_A):
            p.setBrush(QtGui.QColor(BTN_ON if key in self.pressed else BTN_BG))
            p.drawEllipse(cx - r, cy - r, r * 2, r * 2)
            p.setPen(QtGui.QColor(TXT))
            p.setFont(QtGui.QFont("Sans", 16, QtGui.QFont.Bold))
            p.drawText(QtCore.QRect(cx - r, cy - r, r * 2, r * 2),
                       QtCore.Qt.AlignCenter, key.upper())
            p.setPen(QtCore.Qt.NoPen)

        # SELECT / START
        for key, cx, cy, w, h in (SEL_RECT, START_RECT):
            p.setBrush(QtGui.QColor(BTN_ON if key in self.pressed else BTN_BG))
            p.drawRect(int(cx - w / 2), int(cy - h / 2), w, h)
            p.setPen(QtGui.QColor(TXT))
            p.setFont(QtGui.QFont("Sans", 8, QtGui.QFont.Bold))
            p.drawText(QtCore.QRect(int(cx - w / 2), int(cy - h / 2), w, h),
                       QtCore.Qt.AlignCenter, key.upper())
            p.setPen(QtCore.Qt.NoPen)

        # RESET (点击型, 不高亮)
        p.setBrush(QtGui.QColor(RESET_BG))
        p.drawRect(RESET_RECT)
        p.setPen(QtGui.QColor(TXT))
        p.setFont(QtGui.QFont("Sans", 9, QtGui.QFont.Bold))
        p.drawText(RESET_RECT, QtCore.Qt.AlignCenter, "RESET")
        p.end()


class PadGUI(QtWidgets.QWidget):
    KB_MAP = {
        QtCore.Qt.Key_Up: "up", QtCore.Qt.Key_Down: "down",
        QtCore.Qt.Key_Left: "left", QtCore.Qt.Key_Right: "right",
        QtCore.Qt.Key_W: "up", QtCore.Qt.Key_S: "down",
        QtCore.Qt.Key_A: "left", QtCore.Qt.Key_D: "right",
        QtCore.Qt.Key_X: "a", QtCore.Qt.Key_K: "a",
        QtCore.Qt.Key_Z: "b", QtCore.Qt.Key_J: "b",
        QtCore.Qt.Key_Return: "start", QtCore.Qt.Key_Enter: "start",
        QtCore.Qt.Key_Space: "select", QtCore.Qt.Key_Tab: "select",
    }

    def __init__(self, ip, rom_dir=""):
        super().__init__()
        self.events = queue.Queue()
        self.client = PadClient(lambda ev, pl: self.events.put((ev, pl)))
        self.rtt_ms = None
        self.dev = None
        self.kb_held = set()         # 键盘已按下的 Qt key, 防自动重复
        self.auto_connect = not ip
        # UDP 丢包率统计基线 (收到 udp_stat 时按窗口差分计算)
        self._udp_base_rx = None
        self._udp_base_sent = 0

        # ROM 目录优先级: 启动参数 > 环境变量 > 上次记住的目录 > 项目自带 roms/
        self.rom_dir = rom_dir or os.environ.get("PAD_ROM_DIR", "") \
            or _load_saved_rom_dir() or self._default_rom_dir()
        self.rom_all = []            # [(rel_path, abs_path), ...] 全量扫描结果
        self.rom_shown = []          # 过滤后的当前列表

        self.setWindowTitle("ARCS 手柄")
        self.setStyleSheet(STYLESHEET)
        self.setMinimumSize(930, 480)
        self._build_ui(ip)

        # ROM 只读 HTTP API: 启动即拉起, 服务地址展示在 ROM 卡片内 (见 doc/pad-http-api.md)
        self.http_api = None
        # 服务端发现 (应答设备的 discover_server 广播), 随 HTTP API 一起起
        self.discovery = None
        self._start_http()

        # 后台事件汇入 (queue -> GUI 线程), 40ms 周期
        self._drain_timer = QtCore.QTimer(self)
        self._drain_timer.timeout.connect(self._drain_events)
        self._drain_timer.start(40)

        # 应用级按键捕获: 焦点在任意控件 (列表/按钮/文件对话框返回后) 都能玩;
        # 仅焦点在文本输入框时让位 (打字不触发手柄键)
        QtWidgets.QApplication.instance().installEventFilter(self)

        QtCore.QTimer.singleShot(300, self._startup)

    @staticmethod
    def _default_rom_dir():
        """默认 ROM 目录: 项目自带的合法 roms/ (apps-ui/apps/game/roms)"""
        d = Path(__file__).resolve().parent.parent / "apps-ui" / "apps" / "game" / "roms"
        return str(d) if d.is_dir() else str(Path.cwd())

    # ---------- ROM HTTP API (只读) ----------
    def _start_http(self):
        """启动只读 ROM HTTP API, 并把可访问地址显示到 ROM 卡片。
        端口/监听地址可用 PAD_HTTP_PORT / PAD_HTTP_BIND 覆盖。"""
        bind = os.environ.get("PAD_HTTP_BIND", "0.0.0.0")
        try:
            port = int(os.environ.get("PAD_HTTP_PORT", HTTP_PORT))
        except ValueError:
            port = HTTP_PORT
        self.http_api = RomHttpApi(
            get_roms=lambda: self.rom_all,
            get_device=self._device_snapshot,
            get_rom_dir=lambda: self.rom_dir,
            bind=bind, port=port)
        try:
            self.http_api.start()
        except OSError as e:
            self.http_api = None
            self.http_lbl.setStyleSheet(f"color: {DANGER};")
            self.http_lbl.setText(f"HTTP API 未启动 ({bind}:{port}): {e}")
            return
        self.http_lbl.setText(f"HTTP API: http://{self.http_api.lan_ip}:{port}/")
        self.http_lbl.setToolTip("只读 ROM 查询/下载接口, 同一局域网可直接访问; "
                                 "端点清单与示例见 doc/pad-http-api.md")
        self._start_discovery(port)

    def _start_discovery(self, http_port):
        """启动服务端发现监听: 设备广播 {"t":"discover_server"} 时回报本机
        地址与 ROM HTTP API 端口, 使设备无需先被 GUI 扫描过就能查到 ROM 库。"""
        self.discovery = RomDiscoveryServer(
            get_port=lambda: http_port,
            get_rom_count=lambda: len(self.rom_all))
        try:
            self.discovery.start()
        except OSError as e:
            # 端口被占用不致命: 设备仍可通过 GUI 扫描 (反向发现) 拿到地址
            self.discovery = None
            self._set_status(f"服务端发现未启动 (:38201 被占用? {e})", ok=False)
            return
        self._set_status(f"ROM API :{http_port} 就绪 (等待设备查找)", ok=True)

    def _device_snapshot(self):
        """HTTP API /api/device 的数据源 (从后台线程读取, 仅取原子快照)。"""
        dev = self.dev or {}
        return {
            "connected": bool(self.client.connected),
            "device": dev.get("dev"),
            "firmware": dev.get("fw"),
            "state": dev.get("state"),
            "fps": dev.get("fps"),
            "rtt_ms": self.rtt_ms,
            "udp_port": self.client.udp_addr[1] if self.client.udp_addr else None,
        }

    # ---------- UI 构建 ----------
    def _build_ui(self, ip=""):
        root = QtWidgets.QHBoxLayout(self)
        root.setContentsMargins(12, 12, 12, 12)
        root.setSpacing(12)
        root.addWidget(self._build_rom_card())
        root.addLayout(self._build_right_column(ip), 1)
        self._scan_rom_dir()

    # ---------- 左侧: ROM 库卡片 ----------
    def _build_rom_card(self):
        card = QtWidgets.QFrame()
        card.setObjectName("card")
        card.setFixedWidth(264)
        v = QtWidgets.QVBoxLayout(card)
        v.setContentsMargins(12, 12, 12, 12)
        v.setSpacing(10)

        # 标题行: ROM 库 + 计数徽标 | 目录按钮
        head = QtWidgets.QHBoxLayout()
        title = QtWidgets.QLabel("ROM 库")
        title.setObjectName("h1")
        head.addWidget(title)
        self.rom_count_lbl = QtWidgets.QLabel("")
        self.rom_count_lbl.setObjectName("muted")
        head.addWidget(self.rom_count_lbl)
        head.addStretch(1)
        dir_btn = QtWidgets.QPushButton("目录...")
        dir_btn.clicked.connect(self._choose_rom_dir)
        head.addWidget(dir_btn)
        v.addLayout(head)

        # 过滤框 (可见标签, 不只靠 placeholder)
        filt = QtWidgets.QHBoxLayout()
        lbl = QtWidgets.QLabel("过滤")
        lbl.setObjectName("muted")
        filt.addWidget(lbl)
        self.rom_filter = QtWidgets.QLineEdit()
        self.rom_filter.setPlaceholderText("名称片段, 即时筛选")
        self.rom_filter.setClearButtonEnabled(True)
        self.rom_filter.textChanged.connect(self._apply_rom_filter)
        filt.addWidget(self.rom_filter, 1)
        v.addLayout(filt)

        # 列表
        self.rom_list = QtWidgets.QListWidget()
        self.rom_list.itemDoubleClicked.connect(lambda _it: self._push_selected_rom())
        self.rom_list.itemClicked.connect(lambda _it: self._on_rom_select())
        v.addWidget(self.rom_list, 1)

        # 详情子卡片: 选中信息 + 当前目录
        details = QtWidgets.QFrame()
        details.setObjectName("subcard")
        dv = QtWidgets.QVBoxLayout(details)
        dv.setContentsMargins(8, 8, 8, 8)
        dv.setSpacing(4)
        self.rom_info = QtWidgets.QLabel("选择 ROM 查看信息")
        self.rom_info.setObjectName("muted")
        self.rom_info.setWordWrap(True)
        dv.addWidget(self.rom_info)
        self.rom_dir_lbl = QtWidgets.QLabel("")
        self.rom_dir_lbl.setObjectName("dim")
        self.rom_dir_lbl.setWordWrap(True)
        dv.addWidget(self.rom_dir_lbl)
        # ROM HTTP API 服务地址 (只读查询/下载; 可选中复制, 供二次开发接入)
        self.http_lbl = QtWidgets.QLabel("HTTP API: 启动中 ...")
        self.http_lbl.setObjectName("dim")
        self.http_lbl.setWordWrap(True)
        self.http_lbl.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
        dv.addWidget(self.http_lbl)
        v.addWidget(details)

        # 主操作 (本区唯一 primary)
        push_btn = QtWidgets.QPushButton("推送到设备 ▶")
        push_btn.setObjectName("primary")
        push_btn.clicked.connect(self._push_selected_rom)
        v.addWidget(push_btn)
        return card

    # ---------- 右侧: 工具栏 + 手柄 + 状态栏 ----------
    def _build_right_column(self, ip=""):
        right = QtWidgets.QVBoxLayout()
        right.setSpacing(12)

        # 工具栏卡片
        bar = QtWidgets.QFrame()
        bar.setObjectName("card")
        bh = QtWidgets.QHBoxLayout(bar)
        bh.setContentsMargins(10, 8, 10, 8)
        bh.setSpacing(8)
        ip_lbl = QtWidgets.QLabel("设备 IP")
        ip_lbl.setObjectName("muted")
        bh.addWidget(ip_lbl)
        self.ip_edit = QtWidgets.QLineEdit(ip if ip else "")
        self.ip_edit.setFixedWidth(140)
        bh.addWidget(self.ip_edit)
        scan_btn = QtWidgets.QPushButton("扫描")
        scan_btn.clicked.connect(self._do_scan)
        bh.addWidget(scan_btn)
        self.conn_btn = QtWidgets.QPushButton("连接")
        self.conn_btn.setObjectName("conn_btn")
        self.conn_btn.setMinimumWidth(72)      # 防 连接<->断开 文案切换时抖动
        self.conn_btn.clicked.connect(self._toggle_connect)
        bh.addWidget(self.conn_btn)
        rom_btn = QtWidgets.QPushButton("载入ROM")
        rom_btn.clicked.connect(self._do_load_rom)
        bh.addWidget(rom_btn)
        bh.addStretch(1)
        right.addWidget(bar)

        # 手柄卡片 (画布居中)
        pad_card = QtWidgets.QFrame()
        pad_card.setObjectName("card")
        pv = QtWidgets.QVBoxLayout(pad_card)
        pv.setContentsMargins(16, 16, 16, 16)
        self.canvas = PadCanvas()
        self.canvas.reset_clicked = self._send_reset
        pv.addWidget(self.canvas, 0, QtCore.Qt.AlignHCenter | QtCore.Qt.AlignVCenter)
        right.addWidget(pad_card, 1)

        # 状态栏: 状态点 + 文本 | UDP 丢包率 | 快捷键提示
        status = QtWidgets.QHBoxLayout()
        status.setSpacing(6)
        self.status_dot = QtWidgets.QLabel("●")
        self.status_dot.setObjectName("dot")
        self.status_dot.setStyleSheet(f"color: {DIM};")
        status.addWidget(self.status_dot)
        self.status_lbl = QtWidgets.QLabel("未连接")
        status.addWidget(self.status_lbl, 1)
        self.udp_loss_lbl = QtWidgets.QLabel("")
        self.udp_loss_lbl.setObjectName("muted")
        self.udp_loss_lbl.hide()
        status.addWidget(self.udp_loss_lbl, 0, QtCore.Qt.AlignRight)
        hint = QtWidgets.QLabel("↑↓←→/WASD 移动 · X=A · Z=B · Enter=Start · "
                                "Space=Select · R=复位 · 输入框内 Enter/Esc 返回 · "
                                "Esc=退出")
        hint.setObjectName("dim")
        status.addWidget(hint, 0, QtCore.Qt.AlignRight)
        right.addLayout(status)
        return right

    # ---------- 按键输入 (键盘 + 画布共用) ----------
    def key_input(self, key, pressed):
        if self.client.set_key(key, pressed):
            self.canvas.set_pressed(key, pressed)

    def _focus_in_entry(self):
        w = QtWidgets.QApplication.focusWidget()
        return isinstance(w, (QtWidgets.QLineEdit, QtWidgets.QPlainTextEdit,
                              QtWidgets.QTextEdit))

    def eventFilter(self, obj, ev):
        """应用级键盘捕获: 点击列表/按钮/文件对话框后焦点落在子控件上,
        事件不再冒泡到窗口 (列表会吃方向键, 按钮会吃 Space/Enter),
        因此在 QApplication 层拦截, 而不是依赖窗口自身的 keyPressEvent。
        另外处理"焦点陷阱": 点进输入框后给明确出口 —— Esc/Enter 交还焦点,
        点击任意非交互控件 (画布/卡片/标签) 也回到按键捕获态。
        返回 True 表示已消费 (不再传给焦点控件)。"""
        et = ev.type()
        if et == QtCore.QEvent.KeyPress:
            return self._handle_key(ev, True)
        if et == QtCore.QEvent.KeyRelease:
            return self._handle_key(ev, False)
        if et == QtCore.QEvent.MouseButtonPress and not isinstance(
                obj, (QtWidgets.QLineEdit, QtWidgets.QAbstractButton,
                      QtWidgets.QAbstractItemView, QtWidgets.QComboBox,
                      QtWidgets.QAbstractSlider, QtWidgets.QMenu,
                      QtWidgets.QMenuBar)):
            # 点在"背景"上: 交还焦点, 回到游戏按键捕获
            self.setFocus(QtCore.Qt.OtherFocusReason)
        return super().eventFilter(obj, ev)

    def _handle_key(self, ev, pressed):
        """返回 True = 消费。按下时焦点在输入框则让位 (打字);
        抬起不让位 —— 按住 A 的瞬间点进输入框, 松手也必须能解除, 否则粘键。"""
        if ev.key() == QtCore.Qt.Key_Escape:
            if pressed:
                if self._focus_in_entry():
                    # 输入框内 Esc = 交还焦点回到按键捕获 (再按才关窗口)
                    self.setFocus(QtCore.Qt.OtherFocusReason)
                else:
                    self.close()
            return True
        if pressed and self._focus_in_entry():
            if ev.key() in (QtCore.Qt.Key_Return, QtCore.Qt.Key_Enter):
                # 输入框内 Enter = 确认并回到按键捕获; IP 框则直接发起连接
                if QtWidgets.QApplication.focusWidget() is self.ip_edit:
                    self._do_connect()
                self.setFocus(QtCore.Qt.OtherFocusReason)
                return True
            return False
        if ev.isAutoRepeat():
            return True
        if ev.key() == QtCore.Qt.Key_R:
            if pressed and QtCore.Qt.Key_R not in self.kb_held:
                self.kb_held.add(QtCore.Qt.Key_R)
                self._send_reset()
            if not pressed:
                self.kb_held.discard(QtCore.Qt.Key_R)
            return True
        key = self.KB_MAP.get(ev.key())
        if key:
            if pressed:
                if ev.key() not in self.kb_held:
                    self.kb_held.add(ev.key())
                    self.key_input(key, True)
            else:
                if ev.key() in self.kb_held:
                    self.kb_held.discard(ev.key())
                    self.key_input(key, False)
            return True
        return False

    # ---------- ROM 库 ----------
    def _choose_rom_dir(self):
        d = QtWidgets.QFileDialog.getExistingDirectory(
            self, "选择 ROM 目录 (仅限合法来源)", self.rom_dir)
        if d:
            self.rom_dir = d
            _save_rom_dir(d)
            self._scan_rom_dir()

    def _scan_rom_dir(self):
        """递归扫描 ROM 目录 (roms 通常按子目录分册存放, 如 0001/xxx.nes)"""
        files = []
        for pat in ("*.nes", "*.NES"):
            files.extend(glob.glob(os.path.join(self.rom_dir, "**", pat),
                                   recursive=True))
        self.rom_all = sorted((os.path.relpath(p, self.rom_dir), p)
                              for p in set(files))
        self._apply_rom_filter()

    def _apply_rom_filter(self):
        """过滤框: 对相对路径做大小写不敏感的子串匹配"""
        text = self.rom_filter.text().strip().casefold()
        self.rom_shown = [(rel, p) for rel, p in self.rom_all
                          if text in rel.casefold()]
        self.rom_list.clear()
        self.rom_list.addItems([rel for rel, _ in self.rom_shown])
        self.rom_info.setText("选择 ROM 查看信息")
        shown = self.rom_dir if len(self.rom_dir) <= 34 else "..." + self.rom_dir[-31:]
        self.rom_count_lbl.setText(f"{len(self.rom_shown)}/{len(self.rom_all)}")
        self.rom_dir_lbl.setText(shown)

    @staticmethod
    def _parse_ines(path):
        """读 iNES/NES2.0 头, 返回展示用信息串; 解析失败返回 None"""
        meta = _parse_ines_meta(path)
        if not meta:
            return None
        try:
            size_kb = os.path.getsize(path) // 1024
        except OSError:
            size_kb = 0
        chr_txt = f"CHR {meta['chr_kb']}K" if meta["chr_kb"] else "CHR-RAM"
        return (f"{size_kb} KB · mapper {meta['mapper']}"
                f"{' (NES2.0)' if meta['format'] == 'NES2.0' else ''}\n"
                f"PRG {meta['prg_kb']}K · {chr_txt}")

    def _selected_rom(self):
        row = self.rom_list.currentRow()
        if row < 0 or row >= len(self.rom_shown):
            return None
        return self.rom_shown[row][1]

    def _on_rom_select(self):
        path = self._selected_rom()
        if not path:
            return
        info = self._parse_ines(path)
        self.rom_info.setText(info if info else os.path.basename(path) + "\n(非 iNES 镜像)")
        self.rom_info.setStyleSheet(f"color: {MUT if info else DANGER};")

    def _push_selected_rom(self):
        """把选中的 ROM 推送到设备 (协议 §4.2.1), 设备热切换游戏"""
        path = self._selected_rom()
        if not path:
            self._set_status("先在左侧列表选择一个 ROM", ok=None)
            return
        try:
            with open(path, "rb") as fp:
                data = fp.read()
        except OSError as e:
            self._set_status(f"读取失败: {e}", ok=False)
            return
        ok, msg = self.client.send_rom(data, os.path.basename(path))
        self._set_status(msg, ok=ok)

    # ---------- 连接动作 ----------
    def _startup(self):
        if self.auto_connect:
            self._do_scan()
        elif self.ip_edit.text().strip():
            self._do_connect()

    def _set_conn_danger(self, on):
        self.conn_btn.setProperty("danger", "true" if on else "")
        self.conn_btn.style().unpolish(self.conn_btn)
        self.conn_btn.style().polish(self.conn_btn)

    def _toggle_connect(self):
        if self.client.connected or self.client._thread:
            self.client.release_all()
            self.client.disconnect()
            self.conn_btn.setText("连接")
            self._set_conn_danger(False)
            self.dev = None
            self.rtt_ms = None
            self._udp_base_rx = None
            self.udp_loss_lbl.hide()
            self._set_status("已断开", ok=None)
        else:
            self._do_connect()

    def _do_connect(self):
        ip = self.ip_edit.text().strip()
        if not ip:
            self._set_status("请输入设备 IP", ok=None)
            return
        self.client.release_all()
        self.client.disconnect()
        self.client.connect(ip)
        self.conn_btn.setText("断开")
        self._set_conn_danger(True)

    def _do_scan(self):
        self._set_status("扫描中 ...", ok=None)
        # 探测里带上本机 ROM API 端口, 让设备知道去哪台机器查 ROM
        http_port = self.http_api.port if self.http_api else None

        def worker():
            res = discover(2.0, http_port)
            self.events.put(("scan", res))
        threading.Thread(target=worker, daemon=True).start()

    def _do_load_rom(self):
        """选择本地 .nes 推送到设备 (协议 §4.2.1)。只应推送合法来源的 ROM。"""
        path, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "选择 NES ROM (仅限合法来源)", self.rom_dir,
            "NES ROM (*.nes);;所有文件 (*)")
        if not path:
            return
        try:
            with open(path, "rb") as fp:
                data = fp.read()
        except OSError as e:
            self._set_status(f"读取失败: {e}", ok=False)
            return
        ok, msg = self.client.send_rom(data, os.path.basename(path))
        self._set_status(msg, ok=ok)

    def _send_reset(self):
        """主机复位: 设备重载当前 ROM (游戏回到开头)"""
        self.client.send_cmd("reset")
        self._set_status("已发送 RESET (游戏复位)", ok=True)

    def _set_status(self, text, ok):
        """状态栏: 圆点 + 文本双通道表达 (不只靠颜色), ok=None 表示中性"""
        self.status_lbl.setText(text)
        color = SUCCESS if ok else (DANGER if ok is not None else DIM)
        self.status_dot.setStyleSheet(f"color: {color};")

    # ---------- 后台事件汇入 GUI ----------
    def _drain_events(self):
        try:
            while True:
                ev, pl = self.events.get_nowait()
                self._handle(ev, pl)
        except queue.Empty:
            pass

    def _handle(self, ev, pl):
        if ev == "st":
            self._set_status(pl["text"], pl.get("ok", False))
        elif ev == "rtt":
            self.rtt_ms = pl
            if self.dev:
                self._render_dev_status()
        elif ev == "rx":
            t = pl.get("t")
            if t == "welcome":
                self.dev = pl
                self._set_status("已连接", ok=True)
                self._render_dev_status()
                self._udp_base_rx = None      # 新会话: 丢包统计重定基线
                self.udp_loss_lbl.hide()
                self.client.send_hello(socket.gethostname()[:20])
            elif t == "state":
                if self.dev is None:
                    self.dev = {}
                self.dev["state"] = "running" if pl.get("run") else "idle"
                self.dev["fps"] = pl.get("fps")
                self._render_dev_status()
            elif t == "udp_stat":
                self._update_udp_loss(pl)
            elif t == "rom_ack":
                ok = bool(pl.get("ok"))
                self._set_status(f"ROM: {pl.get('msg', '')}", ok=ok)
            elif t == "err":
                self._set_status(f"设备错误: {pl.get('msg')}", ok=False)
        elif ev == "scan":
            if not pl:
                self._set_status("未发现设备 (可手动输入 IP 后点连接)", ok=None)
                return
            first = pl[0]
            self.ip_edit.setText(first.get("ip", ""))
            names = ", ".join(f"{d.get('name')}@{d.get('ip')}" for d in pl)
            self._set_status(f"发现 {len(pl)} 台: {names}", ok=True)
            if self.auto_connect:
                self.auto_connect = False
                self._do_connect()

    def _update_udp_loss(self, pl):
        """UDP 丢包率: 设备上报会话累计收包数, 与本端发送计数按窗口差分。
        乱序被设备按 seq 丢弃也计入 (对全量位图流而言乱序帧即无效帧)。"""
        if not self.client.udp_addr:
            return
        dev_rx = pl.get("rx")
        if not isinstance(dev_rx, int):
            return
        sent_now = self.client.sent_count
        if self._udp_base_rx is None or dev_rx < self._udp_base_rx:
            # 首次上报 / 设备会话重置 (静默松键后重连): 重定基线, 不出读数
            self._udp_base_rx = dev_rx
            self._udp_base_sent = sent_now
            return
        sent_d = sent_now - self._udp_base_sent
        rx_d = dev_rx - self._udp_base_rx
        self._udp_base_rx = dev_rx
        self._udp_base_sent = sent_now
        if sent_d <= 0:
            return
        loss = max(0.0, (1.0 - rx_d / sent_d) * 100.0)
        self.udp_loss_lbl.setText(f"UDP 丢包 {loss:.1f}%")
        self.udp_loss_lbl.setStyleSheet(
            f"color: {SUCCESS if loss < 1 else ('#E0A85F' if loss < 5 else DANGER)};")
        self.udp_loss_lbl.show()

    def _render_dev_status(self):
        if not self.dev:
            return
        rtt = f" · RTT {self.rtt_ms:.0f}ms" if self.rtt_ms is not None else ""
        self._set_status(f"已连接 · {self.dev.get('dev', '?')} "
                         f"fw={self.dev.get('fw', '?')} "
                         f"state={self.dev.get('state', '?')} "
                         f"fps={self.dev.get('fps', '?')}{rtt}", ok=True)

    # ---------- 收尾 ----------
    def closeEvent(self, ev):
        try:
            self.client.release_all()
            self.client.disconnect()
        except Exception:
            pass
        try:
            if self.http_api:
                self.http_api.stop()
        except Exception:
            pass
        super().closeEvent(ev)


def main():
    ip = sys.argv[1] if len(sys.argv) > 1 else ""
    rom_dir = (sys.argv[2] if len(sys.argv) > 2
               else os.environ.get("PAD_ROM_DIR", ""))
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("ARCS 手柄")
    gui = PadGUI(ip, rom_dir)
    gui.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
