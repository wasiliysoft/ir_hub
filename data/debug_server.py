import os
from http.server import SimpleHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse
import urllib.request

HOST = 'localhost'
PORT = 8000
REMOTE_HOST = 'http://irhub.local'
DEFAULT_FILE = 'index.html'

class DebugHTTPRequestHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        self._handle_request('GET')
    
    def do_POST(self):
        self._handle_request('POST')
    
    def _handle_request(self, method):
        parsed_path = urlparse(self.path)
        path = parsed_path.path
        
        # Перенаправляем запрос к корню на index.html
        if path == '/':
            path = '/' + DEFAULT_FILE
        
        local_path = path.lstrip('/')
        
        # Для POST запросов всегда проксируем на удаленный сервер
        if method == 'POST' or not (os.path.exists(local_path) and os.path.isfile(local_path)):
            self._proxy_to_remote(method, path)
        else:
            # Используем оригинальный обработчик для GET запросов к локальным файлам
            self.path = path  # Устанавливаем модифицированный путь
            super().do_GET()
    
    def _proxy_to_remote(self, method, path):
        remote_url = f"{REMOTE_HOST}{path}"
        try:
            # Читаем тело запроса, если это POST
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length) if content_length else None
            
            # Создаем запрос к удаленному серверу
            req = urllib.request.Request(
                remote_url,
                data=post_data,
                method=method,
                headers={**self.headers}
            )
            
            # Отправляем запрос и получаем ответ
            with urllib.request.urlopen(req) as response:
                self.send_response(response.status)
                # Копируем заголовки
                for header, value in response.getheaders():
                    if header.lower() not in ['transfer-encoding', 'connection']:
                        self.send_header(header, value)
                self.end_headers()
                # Копируем содержимое
                self.wfile.write(response.read())
        except Exception as e:
            self.send_error(500, f"Error fetching from remote: {str(e)}")

def run_server():
    server_address = (HOST, PORT)
    httpd = HTTPServer(server_address, DebugHTTPRequestHandler)
    print(f"Starting debug server on http://{HOST}:{PORT}")
    print(f"Root requests will be redirected to {DEFAULT_FILE}")
    print(f"Local files will be served first, missing files will be fetched from {REMOTE_HOST}")
    httpd.serve_forever()

if __name__ == '__main__':
    run_server()