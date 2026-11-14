"""Wire protocol and socket server of the Life Framework Blender bridge.

No `bpy` import here, on purpose: the framing, the token check and the loopback-only server are
tested with any Python 3 (tools/validation/blender-bridge-selftest.mjs). The Blender side
(`runner.py`) supplies the handler that does the work on Blender's main thread.

Protocol: one JSON object per line (UTF-8, "\\n" ends it) in each direction.
  request   {"token": "...", "id": 1, "op": "ping" | "exec" | "shot", ...op fields}
  response  {"id": 1, "ok": true | false, "error": "..." | null, ...op fields}
Every request carries the token from ~/.life-framework/blender-bridge-token. A request without the
right token gets {"ok": false, "error": "unauthorized"} and reaches no handler.
"""
import hmac
import json
import os
import secrets
import socket
import socketserver
import threading

PROTOCOL = 1
HOST = "127.0.0.1"  # loopback only; there is deliberately no setting for it
DEFAULT_PORT = 9876
MAX_LINE = 8 * 1024 * 1024  # a request line (script source included) over 8 MB is refused
IDLE_SECONDS = 30  # a connection that sends nothing for this long is dropped
OPS = ("ping", "exec", "shot")
DEFAULT_TIMEOUT = {"ping": 5.0, "exec": 60.0, "shot": 120.0}
MAX_TIMEOUT = 900.0


def token_path():
    """~/.life-framework/blender-bridge-token (LF_BRIDGE_TOKEN_FILE overrides it, for tests)."""
    override = os.environ.get("LF_BRIDGE_TOKEN_FILE")
    if override:
        return override
    return os.path.join(os.path.expanduser("~"), ".life-framework", "blender-bridge-token")


def read_token(path=None):
    """The token, or None when there is no usable file."""
    try:
        with open(path or token_path(), "r", encoding="utf-8") as f:
            token = f.read().strip()
    except OSError:
        return None
    return token or None


def ensure_token(path=None):
    """The token; creates the file with 32 random bytes (hex) on first use. Never prints it."""
    path = path or token_path()
    token = read_token(path)
    if token:
        return token
    os.makedirs(os.path.dirname(path), exist_ok=True)
    token = secrets.token_hex(32)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write(token + "\n")
    return token


def encode(obj):
    """One response line. Values JSON cannot hold become their repr, so a reply never fails to send."""
    return (json.dumps(obj, separators=(",", ":"), default=repr) + "\n").encode("utf-8")


def clamp_timeout(req):
    op = req.get("op")
    value = req.get("timeout", DEFAULT_TIMEOUT.get(op, 60.0))
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        value = DEFAULT_TIMEOUT.get(op, 60.0)
    return max(1.0, min(float(value), MAX_TIMEOUT))


def handle_line(line, token, handler):
    """One request line to one response dict: framing, then token, then op, then the handler.

    Nothing reaches `handler` before the token matched. The token is compared in constant time and
    never appears in a response.
    """
    if len(line) > MAX_LINE:
        return {"ok": False, "error": "request line too long"}
    try:
        req = json.loads(line.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        return {"ok": False, "error": "bad request: not a JSON line"}
    if not isinstance(req, dict):
        return {"ok": False, "error": "bad request: not a JSON object"}
    rid = req.get("id")
    sent = req.get("token")
    expected = token or ""
    if not expected or not isinstance(sent, str) or not hmac.compare_digest(sent.encode("utf-8"), expected.encode("utf-8")):
        return {"id": rid, "ok": False, "error": "unauthorized"}
    op = req.get("op")
    if op not in OPS:
        return {"id": rid, "ok": False, "error": "unknown op %r (ops: %s)" % (op, ", ".join(OPS))}
    req.pop("token", None)
    req["timeout"] = clamp_timeout(req)
    try:
        res = handler(req)
    except Exception as e:  # a handler bug must not take the server down
        return {"id": rid, "ok": False, "error": "handler failed: %s: %s" % (type(e).__name__, e)}
    if not isinstance(res, dict):
        res = {"ok": False, "error": "handler returned no object"}
    res = dict(res)
    res["id"] = rid
    res.setdefault("ok", False)
    return res


class _Handler(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(IDLE_SECONDS)
        while True:
            try:
                line = self.rfile.readline(MAX_LINE + 1)
            except (OSError, socket.timeout):
                return
            if not line:
                return
            res = handle_line(line, self.server.lf_token, self.server.lf_handler)
            try:
                self.wfile.write(encode(res))
                self.wfile.flush()
            except OSError:
                return
            if res.get("error") in ("unauthorized", "request line too long") or str(res.get("error", "")).startswith("bad request"):
                return  # a stranger or a broken client gets one answer, then the connection closes


class _Server(socketserver.ThreadingTCPServer):
    daemon_threads = True
    allow_reuse_address = False  # on Windows reuse would let another process share the port

    def server_bind(self):
        excl = getattr(socket, "SO_EXCLUSIVEADDRUSE", None)
        if excl is not None:
            self.socket.setsockopt(socket.SOL_SOCKET, excl, 1)
        super().server_bind()


class BridgeServer:
    """Listens on 127.0.0.1:<port>; `handler(req) -> dict` runs per request in a connection thread."""

    def __init__(self, port, token, handler):
        if not token:
            raise ValueError("a bridge server needs a token")
        self.port = port
        self._token = token
        self._handler = handler
        self._server = None
        self._thread = None

    @property
    def running(self):
        return self._server is not None

    def start(self):
        if self._server:
            return
        server = _Server((HOST, int(self.port)), _Handler)
        server.lf_token = self._token
        server.lf_handler = self._handler
        self.port = server.server_address[1]  # port 0 picks a free one (tests)
        self._server = server
        self._thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.2}, name="lf-bridge-server", daemon=True)
        self._thread.start()

    def stop(self):
        server, self._server = self._server, None
        if not server:
            return
        server.shutdown()
        server.server_close()
        if self._thread:
            self._thread.join(timeout=3)
        self._thread = None
