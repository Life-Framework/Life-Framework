"""Offline proof of the bridge protocol (no Blender): run by tools/validation/blender-bridge-selftest.mjs.

Loads lf_bridge/protocol.py by path (it has no bpy import) and drives a real server on an
ephemeral loopback port. Prints one "ok ..." or "FAIL ..." line per check and exits 1 on any FAIL.
"""
import importlib.util
import json
import os
import socket
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("lf_bridge_protocol", os.path.join(here, "lf_bridge", "protocol.py"))
protocol = importlib.util.module_from_spec(spec)
spec.loader.exec_module(protocol)

failed = 0


def check(cond, what):
    global failed
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        failed += 1


def talk(port, lines, read=1):
    """Send raw bytes, read `read` response lines (or until the server closes)."""
    out = []
    with socket.create_connection(("127.0.0.1", port), timeout=5) as s:
        s.sendall(lines)
        f = s.makefile("rb")
        for _ in range(read):
            line = f.readline()
            if not line:
                break
            out.append(json.loads(line))
    return out


with tempfile.TemporaryDirectory() as tmp:
    tokfile = os.path.join(tmp, "sub", "blender-bridge-token")
    t1 = protocol.ensure_token(tokfile)
    check(len(t1) == 64 and all(c in "0123456789abcdef" for c in t1), "ensure_token creates a 64-hex-character random token")
    check(protocol.ensure_token(tokfile) == t1 and protocol.read_token(tokfile) == t1, "ensure_token keeps the token it created")
    t2 = protocol.ensure_token(os.path.join(tmp, "other"))
    check(t2 != t1, "two token files get different tokens")
    check(protocol.read_token(os.path.join(tmp, "missing")) is None, "a missing token file reads as None")
    os.environ["LF_BRIDGE_TOKEN_FILE"] = tokfile
    check(protocol.token_path() == tokfile, "LF_BRIDGE_TOKEN_FILE overrides the token path")
    del os.environ["LF_BRIDGE_TOKEN_FILE"]
    check(protocol.token_path().replace("\\", "/").endswith("/.life-framework/blender-bridge-token"), "the default token path is ~/.life-framework/blender-bridge-token")

    calls = []

    def handler(req):
        calls.append(req)
        if req["op"] == "exec" and req.get("code") == "boom":
            raise RuntimeError("handler bug")
        return {"ok": True, "echo": req.get("code"), "timeout": req["timeout"]}

    srv = protocol.BridgeServer(0, t1, handler)
    srv.start()
    port = srv.port
    try:
        check(srv._server.server_address[0] == "127.0.0.1", "the server is bound to 127.0.0.1")
        check(protocol.HOST == "127.0.0.1", "the bind address is a constant, not a setting")
        try:
            protocol.BridgeServer(0, "", handler)
            check(False, "a server without a token is refused")
        except ValueError:
            check(True, "a server without a token is refused")

        req = lambda **k: (json.dumps(k) + "\n").encode()
        [r] = talk(port, req(id=1, op="ping"))
        check(r == {"id": 1, "ok": False, "error": "unauthorized"} and not calls, "no token: unauthorized, and the handler never ran")
        [r] = talk(port, req(id=2, op="ping", token="x" * 64))
        check(r["error"] == "unauthorized" and not calls, "wrong token: unauthorized, and the handler never ran")
        [r] = talk(port, req(id=3, op="exec", code="print(1)", token=t2))
        check(r["error"] == "unauthorized" and not calls, "another install's token: unauthorized")
        [r] = talk(port, req(id=4, op="ping", token=12345))
        check(r["error"] == "unauthorized" and not calls, "a non-string token: unauthorized")
        check(t1 not in json.dumps(r), "a response never contains the token")

        [r] = talk(port, req(id=5, op="exec", code="x = 1", token=t1))
        check(r["ok"] and r["id"] == 5 and r["echo"] == "x = 1" and len(calls) == 1, "the right token reaches the handler and gets its answer")
        check("token" not in calls[0], "the handler never sees the token")
        check(r["timeout"] == 60.0, "exec defaults to a 60 s timeout")
        [r] = talk(port, req(id=6, op="ping", token=t1, timeout=99999))
        check(r["timeout"] == protocol.MAX_TIMEOUT, "a huge timeout is clamped")
        [r] = talk(port, req(id=7, op="ping", token=t1, timeout="soon"))
        check(r["timeout"] == 5.0, "a non-numeric timeout falls back to the op's default")

        [r] = talk(port, req(id=8, op="rm_rf", token=t1))
        check(not r["ok"] and "unknown op" in r["error"], "an unknown op is refused")
        [r] = talk(port, req(id=9, op="exec", code="boom", token=t1))
        check(not r["ok"] and "handler failed" in r["error"], "a handler exception becomes an error response")
        [r] = talk(port, req(id=10, op="ping", token=t1))
        check(r["ok"], "the server still serves after a handler exception")

        [r] = talk(port, b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        check(not r["ok"] and r["error"].startswith("bad request"), "an HTTP request (a browser page) is not a request")
        [r] = talk(port, b"[1, 2]\n")
        check(not r["ok"] and r["error"].startswith("bad request"), "a JSON array is not a request")
        [r] = talk(port, b"\xff\xfe\n")
        check(not r["ok"] and r["error"].startswith("bad request"), "bytes that are not UTF-8 are not a request")

        two = req(id=11, op="ping", token=t1) + req(id=12, op="exec", code="y", token=t1)
        rs = talk(port, two, read=2)
        check([x["id"] for x in rs] == [11, 12] and all(x["ok"] for x in rs), "one connection serves several lines in order")

        before = len(calls)
        big = req(id=13, op="exec", token=t1, code="a" * (protocol.MAX_LINE + 10))
        try:
            [r] = talk(port, big)
            check(not r["ok"] and len(calls) == before, "an oversize line is refused before the handler")
        except (OSError, ValueError):
            check(len(calls) == before, "an oversize line is refused before the handler (connection dropped)")
    finally:
        srv.stop()
    check(not srv.running, "stop() stops the server")
    try:
        socket.create_connection(("127.0.0.1", port), timeout=2).close()
        check(False, "nothing listens after stop()")
    except OSError:
        check(True, "nothing listens after stop()")

    # exclusive bind: a second server on a held port is refused, not shared
    a = protocol.BridgeServer(0, t1, handler)
    a.start()
    try:
        b = protocol.BridgeServer(a.port, t1, handler)
        try:
            b.start()
            check(False, "a second server cannot take a held port")
            b.stop()
        except OSError:
            check(True, "a second server cannot take a held port")
    finally:
        a.stop()

sys.exit(1 if failed else 0)
