#!/usr/bin/python

import socket
import os
from http.server import HTTPServer
from http.server import SimpleHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

PORT = 8080
DEFAULT_FILE_SIZE_MB = 5  # 默认文件大小（MB）
MAX_FILE_SIZE_MB = 100    # 最大允许文件大小（MB）

class RequestHandler(SimpleHTTPRequestHandler):
    length = 0

    def _set_headers(self, content_type='text/html', content_length=0):
        self.send_response(200)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(content_length))
        self.end_headers()

    def do_GET(self):
        # 解析 URL 和查询参数
        parsed_url = urlparse(self.path)
        path = parsed_url.path
        query_params = parse_qs(parsed_url.query)

        # 处理下载随机文件的请求
        if path == '/random' or path == '/random.bin':
            print(f"\n===== 收到 GET 请求 =====")
            print(f"路径: {self.path}")

            # 从查询参数获取文件大小（MB），默认为 DEFAULT_FILE_SIZE_MB
            size_mb = DEFAULT_FILE_SIZE_MB
            if 'size' in query_params:
                try:
                    size_mb = int(query_params['size'][0])
                    # 限制文件大小范围
                    if size_mb < 1:
                        size_mb = 1
                    elif size_mb > MAX_FILE_SIZE_MB:
                        size_mb = MAX_FILE_SIZE_MB
                        print(f"⚠️  请求的大小超过最大限制，使用最大值: {MAX_FILE_SIZE_MB}MB")
                except ValueError:
                    print(f"⚠️  无效的 size 参数，使用默认值: {DEFAULT_FILE_SIZE_MB}MB")

            file_size = size_mb * 1024 * 1024
            print(f"生成 {size_mb}MB ({file_size} bytes) 随机数据...")

            # 动态生成指定大小的随机数据
            random_data = os.urandom(file_size)

            print(f"发送 {size_mb}MB 随机数据文件")
            self._set_headers(
                content_type='application/octet-stream',
                content_length=len(random_data)
            )
            self.wfile.write(random_data)
            print("文件发送完成")
        else:
            # 返回简单的 HTML 页面
            payload = f"""<html>
<head><title>ARCS HTTP Server</title></head>
<body>
    <h1>ARCS HTTP Server</h1>
    <p>服务器运行正常</p>
    <h2>使用说明</h2>
    <ul>
        <li><a href="/random">下载随机文件（默认 {DEFAULT_FILE_SIZE_MB}MB）</a></li>
        <li><a href="/random?size=10">下载 10MB 随机文件</a></li>
        <li><a href="/random?size=50">下载 50MB 随机文件</a></li>
        <li><a href="/random?size=100">下载 100MB 随机文件</a></li>
    </ul>
    <p>使用方法: <code>/random?size=N</code> (N 为文件大小，单位 MB，最大 {MAX_FILE_SIZE_MB}MB)</p>
</body>
</html>"""
            self._set_headers(
                content_type='text/html',
                content_length=len(payload.encode())
            )
            self.wfile.write(payload.encode())

    def do_POST(self):
        # 1. 打印请求信息
        print("\n===== 收到 POST 请求 =====")
        print(f"路径: {self.path}")
        print("头部:")
        for header, value in self.headers.items():
            print(f"  {header}: {value}")

        # 2. 读取请求 body 并打印
        content_length = int(self.headers.get('Content-Length', 0))
        post_data = self.rfile.read(content_length)
        print("Body 数据:")
        try:
            print(post_data.decode('utf-8'))
        except UnicodeDecodeError:
            print(f"<二进制数据，{len(post_data)} 字节>")

        payload = "<html><p>Done</p></html>"
        self._set_headers(content_length=len(payload))
        self.wfile.write(payload.encode())

def main():
    # 创建 HTTP 服务器并设置端口重用
    httpd = HTTPServer(("0.0.0.0", PORT), RequestHandler)
    httpd.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    print(f"\n========================================")
    print(f"  ARCS HTTP 测试服务器")
    print(f"========================================")
    print(f"监听地址: http://0.0.0.0:{PORT}")
    print(f"默认文件大小: {DEFAULT_FILE_SIZE_MB}MB")
    print(f"最大文件大小: {MAX_FILE_SIZE_MB}MB")
    print(f"\n使用示例:")
    print(f"  下载默认大小: http://0.0.0.0:{PORT}/random")
    print(f"  下载 10MB:   http://0.0.0.0:{PORT}/random?size=10")
    print(f"  下载 50MB:   http://0.0.0.0:{PORT}/random?size=50")
    print(f"========================================\n")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n\n服务器已停止")
        httpd.shutdown()
        httpd.server_close()

if __name__ == '__main__':
    main()
