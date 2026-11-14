# tools/lib/wb-window-capture.ps1 - list and capture the Qt windows of ONE Workbench pid.
#
# System.MakeScreenshot only grabs the 3D framebuffer. This grabs the whole Qt
# window (Log Console, Hierarchy, Object Properties, menus) with Win32
# PrintWindow, so an agent can read panel text. Spec: wb-tooling-spec.md item 6.
#
# Safety (the contract of this file):
#  - Only windows whose owner pid is the exact -WbPid are listed or captured.
#  - Nothing is sent to a window except PrintWindow (WM_PRINT-style paint into
#    our own bitmap). No input, no SetForegroundWindow, no ShowWindow, no
#    screen grab: the user's desktop is never read.
#  - Windows are found on the private LF-Agents desktop (EnumDesktopWindows),
#    else on the calling desktop (LF_SHOW_WINDOWS=1 runs).
#
# Called by tools/lib/wb-window-capture.mjs:
#   powershell -File wb-window-capture.ps1 -Mode list|capture -WbPid N [-Title t] [-Hwnd h] -Raw out.bgr
# Prints one JSON object on stdout. The .mjs does BGR->RGB, downscale and PNG.
param(
  [Parameter(Mandatory)][ValidateSet("list", "capture")][string]$Mode,
  [Parameter(Mandatory)][int]$WbPid,
  [string]$Title = "",
  [long]$Hwnd = 0,
  [string]$Raw = ""
)

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

public static class LfWbCap {
  public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)]
  struct BITMAPINFOHEADER {
    public int biSize, biWidth, biHeight; public short biPlanes, biBitCount;
    public int biCompression, biSizeImage, biXPelsPerMeter, biYPelsPerMeter, biClrUsed, biClrImportant;
  }
  delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)] static extern IntPtr OpenDesktop(string n, int f, bool i, uint a);
  [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr d);
  [DllImport("user32.dll", SetLastError = true)] static extern bool SetThreadDesktop(IntPtr d);
  [DllImport("user32.dll")] static extern IntPtr GetThreadDesktop(uint tid);
  [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
  static IntPtr attached = IntPtr.Zero;
  // Make this thread a member of the named desktop (never switches the visible desktop).
  public static string Attach(string name) {
    attached = OpenDesktop(name, 0, false, 0x10000000);
    if (attached == IntPtr.Zero) return "OpenDesktop failed " + Marshal.GetLastWin32Error();
    if (!SetThreadDesktop(attached)) return "SetThreadDesktop failed " + Marshal.GetLastWin32Error();
    return "";
  }
  [DllImport("user32.dll")] static extern bool EnumDesktopWindows(IntPtr d, EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr h);
  [DllImport("user32.dll")] static extern int ReleaseDC(IntPtr h, IntPtr dc);
  [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr dc);
  [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleBitmap(IntPtr dc, int w, int h);
  [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc, IntPtr o);
  [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr o);
  [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr dc);
  [DllImport("gdi32.dll")] static extern int GetDIBits(IntPtr dc, IntPtr bmp, uint start, uint lines, byte[] bits, ref BITMAPINFOHEADER bi, uint usage);

  public class Win { public long Hwnd; public string Title, Class; public bool Visible, Minimized; public int W, H, X, Y; }

  // Windows owned by pid on the LF-Agents desktop (or, with desktop null, the calling one).
  public static List<Win> Find(int pid, string desktop) {
    var list = new List<Win>();
    EnumProc cb = (h, l) => {
      uint p; GetWindowThreadProcessId(h, out p);
      if (p != (uint)pid) return true;
      var t = new StringBuilder(256); GetWindowTextW(h, t, 256);
      var c = new StringBuilder(128); GetClassNameW(h, c, 128);
      RECT r; GetWindowRect(h, out r);
      list.Add(new Win { Hwnd = h.ToInt64(), Title = t.ToString(), Class = c.ToString(), Visible = IsWindowVisible(h),
        Minimized = IsIconic(h), X = r.L, Y = r.T, W = r.R - r.L, H = r.B - r.T });
      return true;
    };
    if (desktop != null) {
      IntPtr d = OpenDesktop(desktop, 0, false, 0x0040);
      if (d == IntPtr.Zero) return list;
      EnumDesktopWindows(d, cb, IntPtr.Zero);
      CloseDesktop(d);
    } else EnumWindows(cb, IntPtr.Zero);
    GC.KeepAlive(cb);
    return list;
  }

  // 1 = captured with PW_RENDERFULLCONTENT, 2 = captured with flag 0, 0 = failed.
  // Writes padded 24-bit BGR scanlines (top-down, stride = ((w*3+3)&~3)) to path.
  public static int Capture(long hwnd, int w, int h, string path, out string err) {
    err = "";
    IntPtr hw = new IntPtr(hwnd);
    IntPtr screen = GetDC(IntPtr.Zero);
    IntPtr mem = CreateCompatibleDC(screen);
    IntPtr bmp = CreateCompatibleBitmap(screen, w, h);
    if (mem == IntPtr.Zero || bmp == IntPtr.Zero) { err = "gdi alloc failed " + Marshal.GetLastWin32Error(); return 0; }
    IntPtr old = SelectObject(mem, bmp);
    int used = 0;
    if (PrintWindow(hw, mem, 0x2)) used = 1;
    else if (PrintWindow(hw, mem, 0)) used = 2;
    else err = "PrintWindow returned false (win32 error " + Marshal.GetLastWin32Error() + ")";
    if (used != 0) {
      var bi = new BITMAPINFOHEADER();
      bi.biSize = Marshal.SizeOf(typeof(BITMAPINFOHEADER)); bi.biWidth = w; bi.biHeight = -h;
      bi.biPlanes = 1; bi.biBitCount = 24; bi.biCompression = 0;
      var buf = new byte[((w * 3 + 3) & ~3) * h];
      SelectObject(mem, old);
      int got = GetDIBits(mem, bmp, 0, (uint)h, buf, ref bi, 0);
      if (got == 0) { err = "GetDIBits returned 0"; used = 0; }
      else File.WriteAllBytes(path, buf);
    } else SelectObject(mem, old);
    DeleteObject(bmp); DeleteDC(mem); ReleaseDC(IntPtr.Zero, screen);
    return used;
  }
}
'@

# LF-Agents when the pid owns a window there; otherwise the calling desktop.
$desktop = "LF-Agents"
$wins = [LfWbCap]::Find($WbPid, $desktop)
if ($wins.Count -eq 0) { $desktop = ""; $wins = [LfWbCap]::Find($WbPid, $null) }

$mainTitles = "World Editor|Resource Manager|Enfusion Workbench|Workbench"
$rows = @($wins | ForEach-Object {
  [pscustomobject]@{ hwnd = $_.Hwnd; title = $_.Title; class = $_.Class; visible = $_.Visible; minimized = $_.Minimized; x = $_.X; y = $_.Y; width = $_.W; height = $_.H }
})

if ($Mode -eq "list") {
  [pscustomobject]@{ pid = $WbPid; desktop = $desktop; windows = $rows } | ConvertTo-Json -Depth 4 -Compress
  exit 0
}

# capture: an explicit -Hwnd must still belong to the pid; else the biggest
# visible, non-minimised window whose title matches -Title (default: a module window).
$pick = $null
if ($Hwnd -ne 0) {
  $pick = $rows | Where-Object { $_.hwnd -eq $Hwnd } | Select-Object -First 1
} else {
  $pat = if ($Title) { [regex]::Escape($Title) } else { $mainTitles }
  $pick = $rows | Where-Object { $_.visible -and -not $_.minimized -and $_.width -gt 100 -and $_.height -gt 100 -and $_.title -match $pat } |
    Sort-Object { $_.width * $_.height } -Descending | Select-Object -First 1
}
if (-not $pick) {
  [pscustomobject]@{ ok = $false; error = "no visible, non-minimised window of pid $WbPid matches"; desktop = $desktop; windows = $rows } | ConvertTo-Json -Depth 4 -Compress
  exit 0
}
if ($pick.minimized -or $pick.width -le 0 -or $pick.height -le 0) {
  [pscustomobject]@{ ok = $false; error = "window is minimised or has no size"; window = $pick; desktop = $desktop } | ConvertTo-Json -Depth 4 -Compress
  exit 0
}
$err = ""
$attach = ""
if ($desktop -and $env:LF_WBCAP_NO_ATTACH -ne "1") { $attach = [LfWbCap]::Attach($desktop) }
$used = [LfWbCap]::Capture([long]$pick.hwnd, [int]$pick.width, [int]$pick.height, $Raw, [ref]$err)
[pscustomobject]@{
  ok = ($used -ne 0); error = $err; desktop = $desktop; attach = $attach; window = $pick
  flag = $(if ($used -eq 1) { "PW_RENDERFULLCONTENT" } elseif ($used -eq 2) { "0" } else { "" })
} | ConvertTo-Json -Depth 4 -Compress
