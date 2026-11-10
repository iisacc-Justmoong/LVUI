#!/usr/bin/env python3
"""Exercise the native image transport with a bounded loopback fixture."""

from http.server import BaseHTTPRequestHandler, HTTPServer
import gzip
import subprocess
import sys
import threading


SVG = b'<svg xmlns="http://www.w3.org/2000/svg" width="8" height="4"><rect width="8" height="4" fill="red"/></svg>'


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/image")
            self.end_headers()
            return
        if self.path == "/loop":
            self.send_response(302)
            self.send_header("Location", "/loop")
            self.end_headers()
            return
        if self.path == "/missing":
            self.send_error(404)
            return
        body = {"/image": SVG, "/empty": b"", "/large": b"x" * 2048,
                "/gzip": gzip.compress(SVG), "/gzip-large": gzip.compress(b"x" * 2048),
                "/http-gzip": gzip.compress(SVG), "/double-gzip": gzip.compress(gzip.compress(SVG)),
                "/invalid-gzip": b"not gzip"}.get(self.path, b"bad")
        self.send_response(200)
        if self.path in ("/http-gzip", "/double-gzip", "/invalid-gzip"):
            self.send_header("Content-Encoding", "gzip")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, *args):
        pass


def main():
    executable, mode = sys.argv[1:]
    paths = {"success": "/image", "redirect": "/redirect", "status": "/missing",
             "empty": "/empty", "limit": "/large", "loop": "/loop",
             "gzip_async": "/gzip", "gzip_limit": "/gzip-large", "gzip_http": "/http-gzip",
             "gzip_double": "/double-gzip", "gzip_invalid": "/invalid-gzip"}
    with HTTPServer(("127.0.0.1", 0), Handler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}{paths[mode]}"
            # Startup on an external SDK volume can be slower than the bounded
            # transport itself; the executable still enforces its own deadline.
            result = subprocess.run([executable, "http", mode, url], timeout=60, check=False)
        finally:
            server.shutdown()
            thread.join()
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
