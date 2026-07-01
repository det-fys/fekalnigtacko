from http.server import SimpleHTTPRequestHandler, HTTPServer

class COOPCOEPHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

HTTPServer(("localhost", 8000), COOPCOEPHandler).serve_forever()
