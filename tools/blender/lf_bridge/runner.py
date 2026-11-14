"""Runs bridge requests on Blender's MAIN thread.

The socket threads never touch bpy: `dispatch` (called from a connection thread) puts a Job on a
queue and waits; `pump` (a persistent bpy.app.timers callback) pops jobs and runs them on the main
thread. A request has a hard timeout: a job that never started is dropped, and one that is running
gets a LFBridgeTimeout raised in the main thread (it lands at the next bytecode, so a busy Python
loop stops; a long native call such as a render stops when it returns).
"""
import contextlib
import ctypes
import io
import json
import os
import queue
import sys
import threading
import time
import traceback

import bpy

from . import protocol

_queue = queue.Queue()
stats = {"count": 0, "last_op": None, "last_at": None}  # shown in the sidebar panel
_main_thread_id = threading.main_thread().ident
PUMP_SECONDS = 0.05


class LFBridgeTimeout(BaseException):
    """Raised in the main thread when a request outlives its timeout (BaseException so a script's
    `except Exception` cannot swallow it)."""


class Job:
    def __init__(self, req):
        self.req = req
        self.done = threading.Event()
        self.lock = threading.Lock()
        self.state = "queued"  # queued -> running -> done | cancelled
        self.stdout = io.StringIO()
        self.stderr = io.StringIO()
        self.response = None


def dispatch(req):
    """Connection-thread side: queue the job, wait up to the request's timeout, answer."""
    job = Job(req)
    timeout = req["timeout"]
    started = time.monotonic()
    _queue.put(job)
    if not job.done.wait(timeout):
        with job.lock:
            was = job.state
            if was == "queued":
                job.state = "cancelled"
            elif was == "running":
                _raise_in_main_thread()  # under the lock: pump marks the job done under the same lock
        if was == "queued":
            return {"ok": False, "timed_out": True, "error": "timeout after %gs: the request never started (Blender is busy, in a modal operator or a dialog)" % timeout}
        job.done.wait(5)
        if was == "running":
            why = "timeout after %gs while running" % timeout
            if not job.done.is_set():
                why += "; the script is still inside a native call and stops when that returns"
            return {"ok": False, "timed_out": True, "error": why, "stdout": job.stdout.getvalue(), "stderr": job.stderr.getvalue()}
        # was "done": it finished in the instant the wait ran out; answer normally
    res = dict(job.response or {"ok": False, "error": "no response"})
    res["secs"] = round(time.monotonic() - started, 3)
    return res


def _raise_in_main_thread():
    ctypes.pythonapi.PyThreadState_SetAsyncExc(ctypes.c_ulong(_main_thread_id), ctypes.py_object(LFBridgeTimeout))


def _clear_async_exc():
    ctypes.pythonapi.PyThreadState_SetAsyncExc(ctypes.c_ulong(_main_thread_id), None)


def pump():
    """bpy.app.timers callback (main thread): run at most one queued job per tick."""
    try:
        job = _queue.get_nowait()
    except queue.Empty:
        return PUMP_SECONDS
    try:
        with job.lock:
            if job.state == "cancelled":
                return 0.0
            job.state = "running"
        stats["count"] += 1
        stats["last_op"], stats["last_at"] = job.req.get("op"), time.time()
        try:
            job.response = _run(job)
        except LFBridgeTimeout:
            job.response = {"ok": False, "error": "interrupted by the request timeout"}
        finally:
            with job.lock:
                job.state = "done"
        job.done.set()
    except LFBridgeTimeout:
        # raised after the job finished, while the timer was already past it: nothing to do
        if job.response is None:
            job.response = {"ok": False, "error": "interrupted by the request timeout"}
        job.done.set()
    return 0.0 if not _queue.empty() else PUMP_SECONDS


def _run(job):
    op = job.req["op"]
    try:
        if op == "ping":
            return ping()
        if op == "exec":
            return run_exec(job)
        if op == "shot":
            from . import shot

            return shot.run(job.req)
    except LFBridgeTimeout:
        raise
    except Exception:
        return {"ok": False, "error": _short_traceback(sys.exc_info())}
    return {"ok": False, "error": "unknown op"}


def ping():
    addons = []
    try:
        addons = sorted(bpy.context.preferences.addons.keys())
    except Exception:
        pass
    return {
        "ok": True,
        "protocol": protocol.PROTOCOL,
        "blender": bpy.app.version_string,
        "file": bpy.data.filepath,
        "dirty": bool(bpy.data.is_dirty),
        "background": bool(bpy.app.background),
        "pid": os.getpid(),
        "addons": addons,
    }


def _short_traceback(exc_info):
    """A traceback without the bridge's own frames."""
    etype, value, tb = exc_info
    while tb is not None and os.path.dirname(tb.tb_frame.f_code.co_filename) == os.path.dirname(__file__):
        tb = tb.tb_next
    return "".join(traceback.format_exception(etype, value, tb)).rstrip()


# Data-block collections emptied by new_empty_scene. Not the user's text blocks, screens or workspaces.
_SCENE_DATA = ("objects", "meshes", "materials", "images", "textures", "cameras", "lights", "collections", "armatures", "actions",
               "curves", "lattices", "metaballs", "node_groups", "worlds", "particles", "speakers", "lightprobes", "volumes",
               "pointclouds", "hair_curves", "grease_pencils", "shape_keys", "cache_files", "movieclips", "masks")


def new_empty_scene():
    """File > New with an empty scene, done in place. A script written for `blender --background`
    starts with bpy.ops.wm.read_factory_settings(use_empty=True); in a live session that would reset
    the user's add-ons (this bridge included) and the operators that follow lose their window
    context, so the live run swaps that one call for this (see liveSource in tools/lib/blender.mjs)."""
    obj = bpy.context.object
    if obj is not None and obj.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    for name in _SCENE_DATA:
        coll = getattr(bpy.data, name, None)
        if coll is not None and len(coll):
            bpy.data.batch_remove(list(coll))
    scene = bpy.context.scene
    scene.world, scene.camera = None, None
    scene.unit_settings.system, scene.unit_settings.scale_length = "METRIC", 1.0
    bpy.context.view_layer.update()


def run_exec(job):
    req = job.req
    code = req.get("code")
    if not isinstance(code, str):
        return {"ok": False, "error": "exec needs a string 'code'"}
    file = req.get("file") if isinstance(req.get("file"), str) else None
    args = [str(a) for a in req.get("args") or []]
    ns = {"__name__": "__main__", "bpy": bpy, "result": None, "args": args, "lf_new_scene": new_empty_scene}
    if file:
        ns["__file__"] = file
    saved_argv = sys.argv
    sys.argv = ["blender", "--"] + args  # scripts written for `blender --python x.py -- args` find them after "--"
    error = None
    try:
        with contextlib.redirect_stdout(job.stdout), contextlib.redirect_stderr(job.stderr):
            try:
                exec(compile(code, file or "<lf-bridge>", "exec"), ns)
            except SystemExit as e:
                if e.code not in (None, 0):
                    error = "exit %s" % (e.code,)
            except LFBridgeTimeout:
                raise
            except BaseException:
                error = _short_traceback(sys.exc_info())
    finally:
        sys.argv = saved_argv
        _clear_async_exc()
    result = None
    try:
        result = json.loads(json.dumps(ns.get("result"), default=repr))
    except (TypeError, ValueError):
        result = repr(ns.get("result"))
    out = {"ok": error is None, "stdout": job.stdout.getvalue(), "stderr": job.stderr.getvalue(), "result": result}
    if error:
        out["error"] = error
    return out
