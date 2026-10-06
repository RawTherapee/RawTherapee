# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise libcurl against a loopback mock; no provider or real credential is used."""
import http.server
import os
import subprocess
import sys
import threading
import time


class Handler(http.server.BaseHTTPRequestHandler):
    code = 200
    delay = 0
    body = b'{}'
    received = []

    def log_message(self, *args):
        pass

    def do_POST(self):
        self.received.append((self.path, self.headers.get('Authorization'),
                              self.rfile.read(int(self.headers['Content-Length']))))
        code, delay, body = self.code, self.delay, self.body
        time.sleep(delay)
        self.send_response(code)
        if code == 302:
            self.send_header('Location', '/must-not-follow')
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
env = dict(os.environ, NO_PROXY='127.0.0.1', no_proxy='127.0.0.1')
env.pop('RT_AI_TEST_CREDENTIAL', None)


def request(mode='http', environment=env):
    return subprocess.check_output(
        [sys.argv[1], f'http://127.0.0.1:{server.server_port}', mode],
        text=True, env=environment, timeout=15,
    )


try:
    for code, expected in [(200, 'ok'), (401, 'AI_ERROR_AUTHENTICATION'),
                           (403, 'AI_ERROR_AUTHENTICATION'), (429, 'AI_ERROR_QUOTA'),
                           (400, 'AI_ERROR_REQUEST_REJECTED'),
                           (302, 'AI_ERROR_REQUEST_REJECTED'),
                           (500, 'AI_ERROR_REQUEST_REJECTED')]:
        Handler.code = code
        Handler.body = b'{"error":"sensitive provider detail must not leak"}'
        before = len(Handler.received)
        out = request()
        assert expected in out, out
        assert 'sensitive provider detail' not in out
        assert len(Handler.received) == before + 1, 'Unexpected retry or redirect'
        assert Handler.received[-1][0] == '/api/chat'
        assert Handler.received[-1][1] is None

    Handler.code = 200
    Handler.body = b'{}'
    assert 'ok' in request('http-auth', dict(env, RT_AI_TEST_CREDENTIAL='synthetic-test-value'))
    assert Handler.received[-1][1] == 'Bearer synthetic-test-value'
    assert b'synthetic-test-value' not in Handler.received[-1][2]
    before = len(Handler.received)
    assert 'AI_ERROR_CREDENTIAL_MISSING' in request('http-auth')
    assert 'AI_ERROR_CREDENTIAL_INVALID' in request('http-auth', dict(env, RT_AI_TEST_CREDENTIAL='bad\r\nheader'))
    assert len(Handler.received) == before, 'Invalid credentials must not send a request'

    Handler.body = b'x' * (1024 * 1024 + 1)
    assert 'AI_ERROR_CONNECTION' in request(), 'Oversize responses must be rejected'
    Handler.body = b'{}'
    Handler.delay = 3
    assert 'AI_ERROR_TIMEOUT' in request()
    started = time.monotonic()
    assert 'AI_ERROR_CANCELLED' in request('http-cancel')
    assert time.monotonic() - started < 3, 'Cancellation did not interrupt the request'
    print('HTTP status, credentials, redaction, size limit, timeout, cancellation and no retry/redirect checks passed')
finally:
    server.shutdown()
    server.server_close()
