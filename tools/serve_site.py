#!/usr/bin/env python3
"""Serve the built AURA-1 site, including its real 404 page, on localhost."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class SiteHandler(SimpleHTTPRequestHandler):
    def send_error(self, code, message=None, explain=None):
        missing = Path(self.directory) / "404.html"
        if code != 404 or not missing.is_file():
            return super().send_error(code, message, explain)
        page = missing.read_text(encoding="utf-8")
        import re
        page = re.sub(r'<base href="[^"]*">', '<base href="/">', page)
        body = page.encode("utf-8")
        self.send_response(404)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--directory", type=Path, default=Path(__file__).resolve().parents[1] / "app/build/pages")
    args = parser.parse_args()
    server = ThreadingHTTPServer((args.bind, args.port), partial(SiteHandler, directory=str(args.directory.resolve())))
    print(f"AURA-1 studio: http://{args.bind}:{args.port}/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
