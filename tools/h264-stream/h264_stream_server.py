#!/usr/bin/env python3
"""
h264_stream_server.py - ARCS-MINI 在线视频播放器的 Ubuntu 侧流媒体服务器

两种工作模式：

1. 媒体目录模式（推荐，--media <dir>）
   把任意分辨率/编码的 mp4/mkv/mov/avi 文件拷进目录，无需重启。
   服务器按请求参数实时转码为设备友好的 H264 Baseline + AAC TS 流：

     GET /list                                    媒体目录文件清单(JSON)
     GET /play/<文件名>?w=240&h=240&fps=10&vb=300k
                          &ar=16000&ac=1&ab=32k&loop=1&speed=1
         w/h/fps/vb     视频分辨率/帧率/码率
         ar/ac/ab       音频采样率/声道数/码率（ac=1 单声道混音）
         loop=1         循环播放（默认 1）
         speed=1        实时节奏（默认 1，按目标帧率推流；0=尽快推）

2. 测试源模式（默认）
     GET /live.ts     testsrc2 动图 + 440Hz 正弦，240x240@10fps 实时流
     GET /vod.ts      同源一次性整段下发（测下载极限）

设备侧（adb shell）:
     vp play http://<本机IP>:8000/play/movie.mp4
     vp play "http://<本机IP>:8000/play/movie.mp4?w=240&h=240&fps=15&vb=500k"
     vp status / vp stop

依赖: ffmpeg（含 libx264、aac）。无需任何 Python 第三方包。
"""
import argparse
import http.server
import json
import re
import shutil
import socket
import subprocess
import urllib.parse
from pathlib import Path

MEDIA_EXTS = {".mp4", ".mkv", ".mov", ".avi", ".flv", ".ts", ".webm", ".m4v"}

# 设备软解友好：Baseline/CAVLC、无 B 帧、单参考帧、低延迟
ENC_TPL = (
    "ffmpeg -hide_banner -loglevel error "
    "{speed} "
    "{loop} "
    "-i {input} "
    "-vf scale={w}:{h}:force_original_aspect_ratio=decrease,"
    "pad={w}:{h}:(ow-iw)/2:(oh-ih)/2:color=black,fps={fps} "
    "-c:v libx264 -preset ultrafast -profile:v baseline -tune zerolatency "
    "-pix_fmt yuv420p -g {gop} -b:v {vb} -maxrate {vb} -bufsize 600k "
    "-x264-params ref=1 "
    "-c:a aac -b:a {ab} -ar {ar} -ac {ac} "
    "-f mpegts pipe:1"
)

ARGS = None


def parse_query(qs):
    q = {k: v[0] for k, v in urllib.parse.parse_qs(qs).items()}
    out = {
        "w": int(q.get("w", ARGS.width)),
        "h": int(q.get("h", ARGS.height)),
        "fps": int(q.get("fps", ARGS.fps)),
        "vb": q.get("vb", ARGS.bitrate),
        "ar": int(q.get("ar", 16000)),
        "ac": int(q.get("ac", 1)),
        "ab": q.get("ab", "32k"),
        "loop": q.get("loop", "1") not in ("0", "false"),
        "realtime": q.get("speed", "1") not in ("0", "false"),
    }
    # 合法性钳位（设备软解上限与防呆）
    out["w"] = max(16, min(out["w"], 640))
    out["h"] = max(16, min(out["h"], 640))
    out["fps"] = max(5, min(out["fps"], 30))
    out["ac"] = 1 if out["ac"] < 2 else 2
    if not re.fullmatch(r"\d+k", out["vb"]):
        out["vb"] = "300k"
    if not re.fullmatch(r"\d+k", out["ab"]):
        out["ab"] = "32k"
    return out


def build_encoder(input_arg, spec, loop, realtime):
    cmd = ENC_TPL.format(
        speed="-re" if realtime else "-nostats",
        input=input_arg,
        w=spec["w"], h=spec["h"], fps=spec["fps"], gop=spec["fps"] * 2,
        vb=spec["vb"], ab=spec["ab"], ar=spec["ar"], ac=spec["ac"],
        loop="-stream_loop -1" if loop else "",
    )
    print(f"[enc] {cmd}")
    return subprocess.Popen(cmd.split(), stdout=subprocess.PIPE,
                            bufsize=1024 * 1024)


def media_dir() -> Path:
    d = Path(ARGS.media).resolve()
    d.mkdir(parents=True, exist_ok=True)
    return d


def safe_media_path(name):
    """把 URL 里的文件名解析回媒体目录内路径，拒绝穿越/隐藏文件。"""
    base = media_dir()
    p = (base / name).resolve()
    if not str(p).startswith(str(base) + "/"):
        return None
    if not p.is_file():
        return None
    return p


def pump_to(wfile, enc, stop_ev):
    """把 ffmpeg stdout 原样搬到 HTTP 连接。返回是否对端正常全程。"""
    try:
        while True:
            data = enc.stdout.read(1024 * 64)
            if not data:
                return True
            wfile.write(data)
            wfile.flush()
    except (BrokenPipeError, ConnectionResetError, OSError):
        return False


class HandlerBase:
    protocol_version = "HTTP/1.1"

    def _stream(self, enc):
        self.send_response(200)
        self.send_header("Content-Type", "video/mp2t")
        self.send_header("Connection", "close")
        self.close_connection = True
        self.end_headers()
        try:
            pump_to(self.wfile, enc, None)
        finally:
            enc.stdout.close()
            enc.terminate()
            enc.wait()

    def log_message(self, fmt, *a):
        print(f"[http] {self.address_string()} {fmt % a}")


if ARGS is None:
    ARGS = argparse.Namespace(media="media", width=240, height=240, fps=10,
                              bitrate="300k", source="test")


def make_handler(args_ns):
    global ARGS
    ARGS = args_ns

    class Handler(http.server.BaseHTTPRequestHandler, HandlerBase):
        def do_GET(self):
            parsed = urllib.parse.urlparse(self.path)
            route = parsed.path

            if route == "/list":
                files = sorted(
                    p.name for p in media_dir().iterdir()
                    if p.is_file() and p.suffix.lower() in MEDIA_EXTS
                    and not p.name.startswith(".")
                )
                body = json.dumps(
                    {"files": files,
                     "example": f"/play/{files[0] if files else 'movie.mp4'}"
                                f"?w={ARGS.width}&h={ARGS.height}"
                                f"&fps={ARGS.fps}&vb={ARGS.bitrate}"},
                    ensure_ascii=False).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return

            m = re.fullmatch(r"/play/(.+)", route)
            if m:
                name = urllib.parse.unquote(m.group(1))
                path = safe_media_path(name)
                if not path:
                    self.send_error(404, f"not in media dir: {name}")
                    return
                spec = parse_query(parsed.query)
                print(f"[play] {name} -> {spec['w']}x{spec['h']}@{spec['fps']} "
                      f"vb={spec['vb']} aac {spec['ar']}Hz ch{spec['ac']}")
                enc = build_encoder(str(path), spec, spec["loop"],
                                    spec["realtime"])
                self._stream(enc)
                return

            if route == "/live.ts":
                spec = {"w": ARGS.width, "h": ARGS.height, "fps": ARGS.fps,
                        "vb": ARGS.bitrate, "ab": "32k", "ar": 16000, "ac": 1}
                src = (f"-f lavfi -i testsrc2=size={spec['w']}x{spec['h']}:"
                       f"rate={spec['fps']} -f lavfi -i sine=frequency=440")
                enc = build_encoder(src, spec, True, True)
                self._stream(enc)
                return

            if route == "/vod.ts":
                spec = {"w": ARGS.width, "h": ARGS.height, "fps": ARGS.fps,
                        "vb": ARGS.bitrate, "ab": "32k", "ar": 16000, "ac": 1}
                src = (f"-f lavfi -i testsrc2=size={spec['w']}x{spec['h']}:"
                       f"rate={spec['fps']} -f lavfi -i sine=frequency=440")
                enc = build_encoder(src, spec, False, False)
                self._stream(enc)
                return

            self.send_error(404)

    return Handler


def lan_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def main():
    ap = argparse.ArgumentParser(description="H264/AAC HTTP-TS 实时转码流媒体服务器")
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--media", default="media",
                    help="媒体目录（拷入任意 mp4 等文件即点即转）")
    ap.add_argument("--width", type=int, default=240)
    ap.add_argument("--height", type=int, default=240)
    ap.add_argument("--fps", type=int, default=10)
    ap.add_argument("--bitrate", default="300k")
    args = ap.parse_args()

    if not shutil.which("ffmpeg"):
        raise SystemExit("未找到 ffmpeg，请先安装（含 libx264）")

    handler = make_handler(args)
    ip = lan_ip()
    d = media_dir()
    print(f"media dir : {d}")
    print(f"listening on http://{ip}:{args.port}")
    print(f"清单      : curl http://{ip}:{args.port}/list")
    print(f"设备播放  : vp play 'http://{ip}:{args.port}/play/<文件名>'")
    print(f"测试源    : vp play http://{ip}:{args.port}/live.ts")
    http.server.ThreadingHTTPServer(("0.0.0.0", args.port), handler).serve_forever()


if __name__ == "__main__":
    main()
