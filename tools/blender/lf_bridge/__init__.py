"""Life Framework Bridge: lets the Life Framework agent CLI drive this Blender over localhost.

On enable it listens on 127.0.0.1 only, and answers only requests that carry the token in
~/.life-framework/blender-bridge-token. The sidebar (3D viewport, N panel, "LF Bridge" tab) and the
status bar show whether it is listening and have the Start/Stop button. Protocol: protocol.py;
what runs on the main thread: runner.py; pictures: shot.py. Documentation: docs/blender-bridge.md.
"""
import os
import time

import bpy
from bpy.props import BoolProperty, IntProperty
from bpy.types import AddonPreferences, Operator, Panel

from . import protocol, runner

_server = None
_error = ""  # why the last start failed, shown in the panel


def _prefs():
    try:
        return bpy.context.preferences.addons[__package__].preferences
    except Exception:
        return None


def _port():
    p = _prefs()
    return int(p.port) if p else protocol.DEFAULT_PORT


def is_listening():
    return _server is not None and _server.running


def _redraw():
    try:
        for window in bpy.context.window_manager.windows:
            for area in window.screen.areas:
                area.tag_redraw()
    except Exception:
        pass


def start():
    """Start listening. Returns True when it is listening afterwards."""
    global _server, _error
    if is_listening():
        return True
    try:
        token = protocol.ensure_token()
        server = protocol.BridgeServer(_port(), token, runner.dispatch)
        server.start()
    except OSError as e:
        _server, _error = None, "cannot listen on 127.0.0.1:%s (%s)" % (_port(), e.strerror or e)
        print("[LF Bridge] %s" % _error)
        _redraw()
        return False
    _server, _error = server, ""
    print("[LF Bridge] listening on %s:%d (token file %s)" % (protocol.HOST, server.port, protocol.token_path()))
    _redraw()
    return True


def stop():
    global _server
    if _server is not None:
        _server.stop()
        _server = None
        print("[LF Bridge] stopped")
    _redraw()


def _restart_on_port_change(self, context):
    if is_listening():
        stop()
        start()


class LFBRIDGE_Preferences(AddonPreferences):
    bl_idname = __package__

    port: IntProperty(name="Port", description="Localhost port the bridge listens on (the CLI reads LF_BRIDGE_PORT, default 9876)", default=protocol.DEFAULT_PORT, min=1024, max=65535, update=_restart_on_port_change)
    autostart: BoolProperty(name="Start when Blender starts", description="Listen as soon as the extension is enabled. Off: press Start in the LF Bridge panel", default=True)

    def draw(self, context):
        col = self.layout.column()
        col.prop(self, "port")
        col.prop(self, "autostart")
        col.label(text="Listens on 127.0.0.1 only. Requests need the token in %s" % protocol.token_path())


class LFBRIDGE_OT_toggle(Operator):
    bl_idname = "lf_bridge.toggle"
    bl_label = "Start / Stop the LF Bridge"
    bl_description = "Turn the Life Framework agent bridge on or off"

    def execute(self, context):
        if is_listening():
            stop()
            self.report({"INFO"}, "LF Bridge off")
        elif start():
            self.report({"INFO"}, "LF Bridge listening on 127.0.0.1:%d" % _server.port)
        else:
            self.report({"ERROR"}, _error)
        return {"FINISHED"}


def _ago(t):
    if not t:
        return "never"
    s = int(time.time() - t)
    return "%ds ago" % s if s < 120 else "%dm ago" % (s // 60)


class LFBRIDGE_PT_panel(Panel):
    bl_label = "LF Bridge"
    bl_idname = "LFBRIDGE_PT_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "LF Bridge"

    def draw(self, context):
        col = self.layout.column()
        if is_listening():
            col.label(text="LF Bridge: listening on 127.0.0.1:%d" % _server.port, icon="PLAY")
            col.operator("lf_bridge.toggle", text="Stop", icon="PAUSE")
        else:
            col.label(text="LF Bridge: off", icon="PAUSE")
            if _error:
                col.label(text=_error, icon="ERROR")
            col.operator("lf_bridge.toggle", text="Start", icon="PLAY")
        col.separator()
        col.label(text="Requests: %d (last: %s, %s)" % (runner.stats["count"], runner.stats["last_op"] or "none", _ago(runner.stats["last_at"])))
        col.label(text="Token: %s" % protocol.token_path())


def _draw_status(self, context):
    row = self.layout.row(align=True)
    if is_listening():
        row.operator("lf_bridge.toggle", text="LF Bridge: listening :%d" % _server.port, icon="PLAY", emboss=False)
    else:
        row.operator("lf_bridge.toggle", text="LF Bridge: off", icon="PAUSE", emboss=False)


def _autostart():
    p = _prefs()
    if (p is None or p.autostart) and not is_listening():
        start()
    return None


_classes = (LFBRIDGE_Preferences, LFBRIDGE_OT_toggle, LFBRIDGE_PT_panel)


def register():
    for c in _classes:
        bpy.utils.register_class(c)
    bpy.types.STATUSBAR_HT_header.append(_draw_status)
    if not bpy.app.timers.is_registered(runner.pump):
        bpy.app.timers.register(runner.pump, first_interval=runner.PUMP_SECONDS, persistent=True)
    # A headless Blender with the extension enabled does not listen unless LF_BRIDGE_FORCE=1.
    if not bpy.app.background or os.environ.get("LF_BRIDGE_FORCE") == "1":
        bpy.app.timers.register(_autostart, first_interval=0.5, persistent=True)


def unregister():
    stop()
    if bpy.app.timers.is_registered(runner.pump):
        bpy.app.timers.unregister(runner.pump)
    if bpy.app.timers.is_registered(_autostart):
        bpy.app.timers.unregister(_autostart)
    bpy.types.STATUSBAR_HT_header.remove(_draw_status)
    for c in reversed(_classes):
        bpy.utils.unregister_class(c)
