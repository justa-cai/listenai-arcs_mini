#!/usr/bin/python

import socket
from http.server import HTTPServer
from http.server import SimpleHTTPRequestHandler

PORT = 8080

class RequestHandler(SimpleHTTPRequestHandler):
    length = 0

    def _set_headers(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.send_header('Content-Length', str(self.length))
        self.end_headers()

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
        print(post_data.decode('utf-8'))

        payload = "<html><p>Done</p></html>"
        self.length = len(payload)
        self._set_headers()
        self.wfile.write(payload.encode())

def main():
    httpd = HTTPServer(("0.0.0.0", PORT), RequestHandler)
    print(f"Serving at http://0.0.0.0:{PORT} (IPv4)")
    httpd.serve_forever()

if __name__ == '__main__':
    main()
