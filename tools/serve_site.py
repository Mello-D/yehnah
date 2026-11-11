#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mello-D
"""Serve build/site for the local installer with caching off, so the browser always gets the latest build:
    python3 tools/serve_site.py [PORT]        (default 8000; open http://localhost:PORT/webapp/installer/)
Run ./build.sh first (it refreshes build/site). Stop with Ctrl+C."""
import functools, http.server, os, sys

site = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'site')


class NoCache(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store, max-age=0')
        super().end_headers()

    def do_GET(self):
        if self.path == '/favicon.ico':                 # (none: no 404 noise in the log)
            self.send_response(204)
            self.end_headers()
            return
        super().do_GET()


port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
handler = functools.partial(NoCache, directory=site)
print(f'serving {os.path.normpath(site)} on http://localhost:{port}/webapp/installer/ (no caching)')
http.server.ThreadingHTTPServer(('0.0.0.0', port), handler).serve_forever()
