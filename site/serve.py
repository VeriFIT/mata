#!/usr/bin/env python3
"""Serve a local versioned website with its custom 404 page."""

import sys
from functools import partial
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, test
from pathlib import Path


class WebsiteRequestHandler(SimpleHTTPRequestHandler):
    def send_error(self, code, message=None, explain=None):
        """Use the custom 404 page when present, retaining standard HTTP error handling."""
        if code == HTTPStatus.NOT_FOUND:
            try:
                not_found = (Path(self.directory) / "404.html").read_text(encoding="utf-8")
            except OSError:
                pass
            else:
                # Escape percent signs for BaseHTTPRequestHandler's error template interpolation.
                self.error_message_format = not_found.replace("%", "%%")
        super().send_error(code, message, explain)


if __name__ == "__main__":
    test(
        HandlerClass=partial(WebsiteRequestHandler, directory=sys.argv[1]),
        port=int(sys.argv[2]),
        bind="127.0.0.1",
    )
