# tools/lib/wb-dialog-watcher.ps1 - answer Workbench's blocking modal dialogs on LF-Agents.
#
# RegisterResourceFile on an .fbx opens "Register Resource File(s)" on the
# Workbench main thread, and the first restricted script call (MakeScreenshot,
# RegisterResourceFile, Save) opens "Script Authorization Required". Both hang
# the NET API socket until someone clicks. On the hidden LF-Agents desktop
# nobody can, so this helper does.
#
# Isolation (the reason this file exists apart from any focus-stealing tool):
#   * It runs only when its own thread desktop is WinSta0\LF-Agents. It is
#     started by quiet-launch, whose CreateProcess sets lpDesktop to LF-Agents;
#     anywhere else (the user's desktop, LF_SHOW_WINDOWS=1, ssh) it exits 3
#     without enumerating a single window.
#   * It enumerates with EnumDesktopWindows on the LF-Agents handle, never
#     EnumWindows, and keeps only windows whose GetWindowThreadProcessId is the
#     exact -WorkbenchPid, whose title matches a known dialog, and which is not
#     a Workbench main window.
#   * It never moves the cursor or injects input (no SetCursorPos, mouse_event,
#     keybd_event, SendInput: those act on the input desktop, which is the
#     user's). Every click and key is a message posted to that one HWND.
#
# Answer order, per the maintainer spec (wb-tooling-spec item 5):
#   1. a Win32 "Button" child with an affirmative caption -> BM_CLICK
#   2. the keyboard mnemonic (Alt+A for "Yes to &All") posted to the dialog
#   3. a click at a fixed offset from the dialog's bottom-right corner, for a
#      custom-drawn Qt dialog with no child HWNDs (the button row is
#      right-aligned, so the offset holds while the width grows with the path)
# Each step is tried only if the dialog is still there after the previous one.
#
# Every dialog seen and every answer goes to -Log as one JSON line.
# The watcher exits when the Workbench process exits, or when -StopFile exists.
#
#   powershell -File tools/lib/wb-dialog-watcher.ps1 -WorkbenchPid <pid> -Log <file> [-SnapDir <dir>] [-Probe]
# -Probe logs the windows of that pid on LF-Agents (class, title, rect,
# children) once and exits; it never clicks.
param(
  [Parameter(Mandatory)][int]$WorkbenchPid,
  [Parameter(Mandatory)][string]$Log,
  [string]$StopFile = "",
  [string]$SnapDir = "",
  [int]$IntervalMs = 300,
  [switch]$Probe
)

$ErrorActionPreference = "Stop"

if (-not ("LfDialogWatch" -as [type])) {
  Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public static class LfDialogWatch {
  public const string DesktopName = "LF-Agents";

  [StructLayout(LayoutKind.Sequential)]
  public struct RECT { public int Left, Top, Right, Bottom; }

  delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern IntPtr OpenDesktop(string name, int flags, bool inherit, uint access);
  [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr d);
  [DllImport("user32.dll")] static extern IntPtr GetThreadDesktop(uint tid);
  [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
  [DllImport("user32.dll", SetLastError = true)] static extern bool SetThreadDesktop(IntPtr d);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)]
  static extern bool GetUserObjectInformation(IntPtr obj, int index, StringBuilder info, int len, out int needed);
  [DllImport("user32.dll")] static extern bool EnumDesktopWindows(IntPtr d, EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool ScreenToClient(IntPtr h, ref Point p);
  [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] static extern IntPtr SendMessageTimeout(IntPtr h, uint msg, IntPtr w, IntPtr l, uint flags, uint timeout, out IntPtr result);
  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);

  static string Text(IntPtr h) { var sb = new StringBuilder(512); GetWindowText(h, sb, sb.Capacity); return sb.ToString(); }
  static string Class(IntPtr h) { var sb = new StringBuilder(256); GetClassName(h, sb, sb.Capacity); return sb.ToString(); }

  static string ObjectName(IntPtr obj) {
    var sb = new StringBuilder(256); int needed;
    if (!GetUserObjectInformation(obj, 2 /*UOI_NAME*/, sb, sb.Capacity * 2, out needed)) return "";
    return sb.ToString();
  }

  // Attach this thread to LF-Agents and return the desktop name the thread is
  // really on afterwards. The caller refuses to run unless it is LF-Agents.
  // A process started by quiet-launch is already there; SetThreadDesktop only
  // makes the intent explicit and fails harmlessly if the thread owns windows.
  static IntPtr desk = IntPtr.Zero;
  public static string Attach() {
    desk = OpenDesktop(DesktopName, 0, false, 0x0040 | 0x0001 /*DESKTOP_ENUMERATE|DESKTOP_READOBJECTS; never SWITCHDESKTOP*/);
    if (desk == IntPtr.Zero) return "";
    SetThreadDesktop(desk);
    return ObjectName(GetThreadDesktop(GetCurrentThreadId()));
  }

  public class Win {
    public IntPtr Hwnd; public string Title; public string Cls; public RECT Rect; public bool Visible;
    public List<Win> Children = new List<Win>();
  }

  static Win Describe(IntPtr h) {
    var w = new Win(); w.Hwnd = h; w.Title = Text(h); w.Cls = Class(h); w.Visible = IsWindowVisible(h);
    GetWindowRect(h, out w.Rect); return w;
  }

  static uint Owner(IntPtr h) { uint pid; GetWindowThreadProcessId(h, out pid); return pid; }

  // Top-level windows on LF-Agents owned by exactly this pid. Nothing else is
  // ever returned, so nothing else can be clicked.
  public static List<Win> Windows(int pid, bool withChildren) {
    var list = new List<Win>();
    if (desk == IntPtr.Zero) return list;
    EnumDesktopWindows(desk, (h, l) => {
      if (Owner(h) != (uint)pid) return true;
      var w = Describe(h);
      if (withChildren) {
        EnumChildWindows(h, (c, cl) => { if (Owner(c) == (uint)pid) w.Children.Add(Describe(c)); return true; }, IntPtr.Zero);
      }
      list.Add(w);
      return true;
    }, IntPtr.Zero);
    return list;
  }

  public static bool Alive(IntPtr h, int pid) { return IsWindow(h) && IsWindowVisible(h) && Owner(h) == (uint)pid; }

  // Step 1: BM_CLICK on a "Button" child whose caption (mnemonic & removed)
  // matches the first affirmative caption present. Returns the caption or "".
  public static string ClickButton(IntPtr dialog, int pid, string[] captions) {
    var buttons = new List<KeyValuePair<IntPtr, string>>();
    EnumChildWindows(dialog, (c, l) => {
      if (Owner(c) == (uint)pid && Class(c) == "Button" && IsWindowVisible(c))
        buttons.Add(new KeyValuePair<IntPtr, string>(c, Text(c).Replace("&", "")));
      return true;
    }, IntPtr.Zero);
    foreach (var want in captions)
      foreach (var b in buttons)
        if (string.Equals(b.Value.Trim(), want, StringComparison.OrdinalIgnoreCase)) {
          IntPtr r;
          SendMessageTimeout(b.Key, 0x00F5 /*BM_CLICK*/, IntPtr.Zero, IntPtr.Zero, 0x0002 /*SMTO_ABORTIFHUNG*/, 2000, out r);
          return b.Value;
        }
    return "";
  }

  // Step 2: Alt+<key> posted to the dialog (WM_SYSKEYDOWN/WM_SYSCHAR/WM_SYSKEYUP
  // with the context bit set). No input is injected into any desktop.
  public static void PostMnemonic(IntPtr dialog, char key) {
    int vk = char.ToUpperInvariant(key);
    IntPtr down = (IntPtr)(1 | (1 << 29));
    IntPtr up = (IntPtr)(1 | (1 << 29) | (1 << 30) | (1 << 31));
    PostMessage(dialog, 0x0104 /*WM_SYSKEYDOWN*/, (IntPtr)0x12, (IntPtr)(1 | (1 << 29)));
    PostMessage(dialog, 0x0104, (IntPtr)vk, down);
    PostMessage(dialog, 0x0106 /*WM_SYSCHAR*/, (IntPtr)char.ToLowerInvariant(key), down);
    PostMessage(dialog, 0x0105 /*WM_SYSKEYUP*/, (IntPtr)vk, up);
    PostMessage(dialog, 0x0101 /*WM_KEYUP*/, (IntPtr)0x12, (IntPtr)(1 | (1 << 30) | (1 << 31)));
  }

  // Step 3: a left click at (right - dx, bottom - dy) of the dialog's window
  // rect, posted as client-coordinate mouse messages to that HWND.
  public static string PostAnchorClick(IntPtr dialog, int dx, int dy) {
    RECT r; GetWindowRect(dialog, out r);
    var p = new Point(r.Right - dx, r.Bottom - dy);
    ScreenToClient(dialog, ref p);
    IntPtr lp = (IntPtr)((p.Y << 16) | (p.X & 0xFFFF));
    PostMessage(dialog, 0x0200 /*WM_MOUSEMOVE*/, IntPtr.Zero, lp);
    PostMessage(dialog, 0x0201 /*WM_LBUTTONDOWN*/, (IntPtr)0x0001, lp);
    PostMessage(dialog, 0x0202 /*WM_LBUTTONUP*/, IntPtr.Zero, lp);
    return p.X + "," + p.Y;
  }

  // PNG of the dialog, for the log (what was on it when it was answered).
  public static bool Snap(IntPtr h, string path) {
    RECT r; GetWindowRect(h, out r);
    int w = r.Right - r.Left, ht = r.Bottom - r.Top;
    if (w <= 0 || ht <= 0 || w > 8000 || ht > 8000) return false;
    using (var bmp = new Bitmap(w, ht, PixelFormat.Format24bppRgb))
    using (var g = Graphics.FromImage(bmp)) {
      IntPtr hdc = g.GetHdc();
      bool ok = PrintWindow(h, hdc, 2 /*PW_RENDERFULLCONTENT*/) || PrintWindow(h, hdc, 0);
      g.ReleaseHdc(hdc);
      if (ok) bmp.Save(path, ImageFormat.Png);
      return ok;
    }
  }
}
'@
}

# The dialogs this watcher answers. Captions are tried in order. dx/dy is the
# centre of the affirmative button measured from the dialog's bottom-right
# window corner, for a custom-drawn Qt dialog with no Button children.
#   register: the FBX prompt is "as Model | as Animation | Cancel" in a 516x167
#     window (Qt 6.8.3, snapshot 2026-11-14); "as Model" centre is (-260,-31).
#     Its buttons have no Alt+A mnemonic: the posted Alt+A was a no-op there.
#   authorize: not yet seen on LF-Agents, so no anchor. A blind click could
#     hit "Deny"; it is answered by button or mnemonic, or logged "unanswered"
#     with a snapshot so the anchor can be measured.
$Dialogs = @(
  @{ name = "register"; match = "Register Resource File"; captions = @("as Model", "Yes to All", "Yes", "OK"); mnemonic = "a"; anchors = @(@{ dx = 260; dy = 31; button = "as Model" }) },
  @{ name = "authorize"; match = "Script Authorization"; captions = @("Yes to All", "Yes", "Allow", "Always Allow", "OK"); mnemonic = "a"; anchors = @() }
)
$MainTitles = "World Editor|Resource Manager|Enfusion Workbench|Script Editor"

function Write-WatchLog([hashtable]$Entry) {
  $line = [ordered]@{ time = (Get-Date).ToString("o"); workbenchPid = $WorkbenchPid }
  foreach ($k in $Entry.Keys) { $line[$k] = $Entry[$k] }
  # UTF-8 without a BOM, so every line of the .jsonl parses.
  [IO.File]::AppendAllText($Log, ($line | ConvertTo-Json -Compress -Depth 6) + "`n", (New-Object Text.UTF8Encoding $false))
}

$logDir = Split-Path -Parent $Log
if ($logDir -and -not (Test-Path $logDir)) { New-Item -ItemType Directory -Force $logDir | Out-Null }

$onDesktop = [LfDialogWatch]::Attach()
if ($onDesktop -ne [LfDialogWatch]::DesktopName) {
  Write-WatchLog @{ event = "refused"; reason = "thread desktop is '$onDesktop', not LF-Agents; the watcher only acts on the hidden desktop" }
  exit 3
}
Write-WatchLog @{ event = "start"; desktop = $onDesktop; watcherPid = $PID; probe = [bool]$Probe }

function Rect([object]$r) { "$($r.Left),$($r.Top),$($r.Right),$($r.Bottom)" }

if ($Probe) {
  foreach ($w in [LfDialogWatch]::Windows($WorkbenchPid, $true)) {
    if (-not $w.Visible) { continue }
    $kids = @($w.Children | Where-Object Visible | ForEach-Object { @{ cls = $_.Cls; title = $_.Title; rect = (Rect $_.Rect) } })
    Write-WatchLog @{ event = "probe"; hwnd = [int64]$w.Hwnd; cls = $w.Cls; title = $w.Title; rect = (Rect $w.Rect); children = $kids }
  }
  exit 0
}

function Gone([IntPtr]$h) {
  # Up to ~1.5 s for the dialog to close after an answer.
  for ($i = 0; $i -lt 10; $i++) {
    if (-not [LfDialogWatch]::Alive($h, $WorkbenchPid)) { return $true }
    Start-Sleep -Milliseconds 150
  }
  return $false
}

function Answer($w, $d) {
  $h = $w.Hwnd
  $base = @{ dialog = $d.name; title = $w.Title; hwnd = [int64]$h; cls = $w.Cls; rect = (Rect $w.Rect) }
  if ($SnapDir) {
    if (-not (Test-Path $SnapDir)) { New-Item -ItemType Directory -Force $SnapDir | Out-Null }
    $png = Join-Path $SnapDir ("{0}-{1}-{2}.png" -f $d.name, $h, (Get-Date -Format "HHmmssfff"))
    if ([LfDialogWatch]::Snap($h, $png)) { $base.snap = $png }
  }
  $caption = [LfDialogWatch]::ClickButton($h, $WorkbenchPid, [string[]]$d.captions)
  if ($caption) {
    $ok = Gone $h
    Write-WatchLog ($base + @{ event = "answered"; method = "button"; button = $caption; closed = $ok })
    if ($ok) { return }
  }
  if ($d.mnemonic) {
    [LfDialogWatch]::PostMnemonic($h, [char]$d.mnemonic)
    $ok = Gone $h
    Write-WatchLog ($base + @{ event = "answered"; method = "mnemonic"; button = "Alt+$($d.mnemonic.ToUpper())"; closed = $ok })
    if ($ok) { return }
  }
  foreach ($a in $d.anchors) {
    if (-not [LfDialogWatch]::Alive($h, $WorkbenchPid)) { return }
    $at = [LfDialogWatch]::PostAnchorClick($h, $a.dx, $a.dy)
    $ok = Gone $h
    Write-WatchLog ($base + @{ event = "answered"; method = "anchor"; button = "$($a.button) at bottom-right -$($a.dx),-$($a.dy) (client $at)"; closed = $ok })
    if ($ok) { return }
  }
  Write-WatchLog ($base + @{ event = "unanswered"; reason = "no answer closed the dialog" })
}

$tries = @{}
while ($true) {
  $wb = Get-Process -Id $WorkbenchPid -ErrorAction SilentlyContinue
  if (-not $wb) { Write-WatchLog @{ event = "stop"; reason = "workbench exited" }; exit 0 }
  if ($StopFile -and (Test-Path -LiteralPath $StopFile)) { Write-WatchLog @{ event = "stop"; reason = "stop file" }; exit 0 }
  foreach ($w in [LfDialogWatch]::Windows($WorkbenchPid, $false)) {
    if (-not $w.Visible -or -not $w.Title -or $w.Title -match $MainTitles) { continue }
    $d = $Dialogs | Where-Object { $w.Title -like "*$($_.match)*" } | Select-Object -First 1
    if (-not $d) { continue }
    # A dialog that survives every answer is retried a few times, then left alone and logged once.
    $key = [int64]$w.Hwnd
    $tries[$key] = 1 + [int]$tries[$key]
    if ($tries[$key] -le 3) { Answer $w $d }
    elseif ($tries[$key] -eq 4) { Write-WatchLog @{ event = "gave-up"; dialog = $d.name; title = $w.Title; hwnd = $key } }
  }
  Start-Sleep -Milliseconds $IntervalMs
}
