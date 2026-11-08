# tools/lib/quiet-launch.ps1 - start a process on the private "LF-Agents" desktop.
#
# A window on another Win32 desktop is never drawn on the user's screen and can
# never take focus. Every non-rendering tool run (Workbench script validation,
# the headless build, the dedicated test server) starts here, so agents stop
# flashing Workbench and console windows over whatever the user is doing.
#
# A function library with no param block, so dot-sourcing it never touches the
# caller's $args or $PSBoundParameters. Dot-source it and call
# Start-LfQuietProcess; tools/lib/quiet-launch.mjs calls Invoke-LfQuietSpec.
#
# LF_SHOW_WINDOWS=1 starts the process on the user's desktop instead (debugging).
# An ssh session (SSH_CONNECTION set) does the same: it has no interactive
# window station, and creating LF-Agents there kills the server before it logs.
if (-not ("LfQuiet" -as [type])) {
  Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

public static class LfQuiet {
  public const string DesktopName = "LF-Agents";

  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  struct STARTUPINFO {
    public int cb; public string lpReserved; public string lpDesktop; public string lpTitle;
    public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
  }
  [StructLayout(LayoutKind.Sequential)]
  struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }
  [StructLayout(LayoutKind.Sequential)]
  struct BASIC_LIMIT {
    public long PerProcessUserTimeLimit, PerJobUserTimeLimit; public uint LimitFlags;
    public UIntPtr MinimumWorkingSetSize, MaximumWorkingSetSize; public uint ActiveProcessLimit;
    public UIntPtr Affinity; public uint PriorityClass, SchedulingClass;
  }
  [StructLayout(LayoutKind.Sequential)]
  struct IO_COUNTERS { public ulong a, b, c, d, e, f; }
  [StructLayout(LayoutKind.Sequential)]
  struct EXTENDED_LIMIT {
    public BASIC_LIMIT Basic; public IO_COUNTERS Io;
    public UIntPtr ProcessMemoryLimit, JobMemoryLimit, PeakProcessMemoryUsed, PeakJobMemoryUsed;
  }

  [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern IntPtr CreateDesktop(string name, IntPtr device, IntPtr devmode, int flags, uint access, IntPtr sa);
  [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern bool CreateProcess(string app, StringBuilder cmd, IntPtr pa, IntPtr ta, bool inherit,
    uint flags, IntPtr env, string cwd, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
  [DllImport("kernel32.dll", SetLastError = true)]
  static extern IntPtr CreateJobObject(IntPtr sa, string name);
  [DllImport("kernel32.dll", SetLastError = true)]
  static extern bool SetInformationJobObject(IntPtr job, int cls, ref EXTENDED_LIMIT info, int len);
  [DllImport("kernel32.dll", SetLastError = true)]
  static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
  [DllImport("kernel32.dll", SetLastError = true)]
  static extern uint ResumeThread(IntPtr thread);
  [DllImport("kernel32.dll", SetLastError = true)]
  static extern bool CloseHandle(IntPtr h);
  [DllImport("kernel32.dll", SetLastError = true)]
  static extern bool SetHandleInformation(IntPtr h, int mask, int flags);
  [DllImport("kernel32.dll", SetLastError = true)]
  static extern bool TerminateProcess(IntPtr process, uint code);
  delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern IntPtr OpenDesktop(string name, int flags, bool inherit, uint access);
  [DllImport("user32.dll")]
  static extern bool CloseDesktop(IntPtr desktop);
  [DllImport("user32.dll")]
  static extern bool EnumDesktopWindows(IntPtr desktop, EnumProc cb, IntPtr l);
  [DllImport("user32.dll")]
  static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);

  // Pids that own a window on LF-Agents (empty when the desktop does not exist).
  public static int[] WindowPids() {
    var pids = new HashSet<int>();
    IntPtr d = OpenDesktop(DesktopName, 0, false, 0x0040 /*DESKTOP_ENUMERATE*/);
    if (d == IntPtr.Zero) return new int[0];
    EnumDesktopWindows(d, (h, l) => { uint pid; GetWindowThreadProcessId(h, out pid); pids.Add((int)pid); return true; }, IntPtr.Zero);
    CloseDesktop(d);
    var list = new List<int>(pids);
    return list.ToArray();
  }

  // Held for the life of this PowerShell process: the desktop must outlive the
  // launch call, and closing the job kills everything in it.
  static IntPtr desktop = IntPtr.Zero;
  static readonly List<IntPtr> keep = new List<IntPtr>();

  // MSVCRT argv quoting, the rules Workbench and the server parse with.
  public static string Quote(string a) {
    if (a.Length > 0 && a.IndexOfAny(new[] { ' ', '\t', '"' }) < 0) return a;
    var sb = new StringBuilder("\"");
    int slashes = 0;
    foreach (char c in a) {
      if (c == '\\') { slashes++; continue; }
      if (c == '"') sb.Append('\\', slashes * 2 + 1);
      else sb.Append('\\', slashes);
      slashes = 0;
      sb.Append(c);
    }
    sb.Append('\\', slashes * 2);
    return sb.Append('"').ToString();
  }

  static IntPtr Inheritable(string path) {
    var fs = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.ReadWrite | FileShare.Delete);
    IntPtr h = fs.SafeFileHandle.DangerousGetHandle();
    SetHandleInformation(h, 1, 1);
    keep.Add(h);
    GC.SuppressFinalize(fs);
    return h;
  }

  // Starts exe on WinSta0\LF-Agents. killOnClose puts it in a job that dies
  // with this process. Returns the pid; the process handle stays open so the
  // exit code is readable after the child exits.
  public static int Start(string exe, string[] args, string cwd, string stdout, string stderr, bool killOnClose) {
    if (desktop == IntPtr.Zero) {
      desktop = CreateDesktop(DesktopName, IntPtr.Zero, IntPtr.Zero, 0, 0x10000000 /*GENERIC_ALL*/, IntPtr.Zero);
      if (desktop == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateDesktop " + DesktopName);
    }
    var cmd = new StringBuilder(Quote(exe));
    foreach (var a in args) cmd.Append(' ').Append(Quote(a));
    var si = new STARTUPINFO();
    si.cb = Marshal.SizeOf(typeof(STARTUPINFO));
    si.lpDesktop = "WinSta0\\" + DesktopName;
    si.dwFlags = 0x1; // STARTF_USESHOWWINDOW
    si.wShowWindow = 4; // SW_SHOWNOACTIVATE
    bool inherit = false;
    if (!string.IsNullOrEmpty(stdout) || !string.IsNullOrEmpty(stderr)) {
      inherit = true;
      si.dwFlags |= 0x100; // STARTF_USESTDHANDLES
      si.hStdOutput = string.IsNullOrEmpty(stdout) ? IntPtr.Zero : Inheritable(stdout);
      si.hStdError = string.IsNullOrEmpty(stderr) ? si.hStdOutput : Inheritable(stderr);
    }
    // CREATE_SUSPENDED | CREATE_NO_WINDOW (no console for console-subsystem exes)
    uint flags = 0x4 | 0x08000000;
    PROCESS_INFORMATION pi;
    if (!CreateProcess(null, cmd, IntPtr.Zero, IntPtr.Zero, inherit, flags, IntPtr.Zero,
        string.IsNullOrEmpty(cwd) ? null : cwd, ref si, out pi))
      throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateProcess " + exe);
    if (killOnClose) {
      IntPtr job = CreateJobObject(IntPtr.Zero, null);
      var info = new EXTENDED_LIMIT();
      info.Basic.LimitFlags = 0x2000; // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
      if (job == IntPtr.Zero
          || !SetInformationJobObject(job, 9, ref info, Marshal.SizeOf(typeof(EXTENDED_LIMIT)))
          || !AssignProcessToJobObject(job, pi.hProcess)) {
        int err = Marshal.GetLastWin32Error();
        TerminateProcess(pi.hProcess, 1);
        throw new Win32Exception(err, "job object for " + exe);
      }
      keep.Add(job);
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    keep.Add(pi.hProcess);
    return pi.dwProcessId;
  }
}
'@
}

# Returns a [System.Diagnostics.Process] for the child (HasExited, ExitCode,
# WaitForExit, Kill all work). -KillOnClose ties the child to this PowerShell.
function Start-LfQuietProcess {
  param(
    [Parameter(Mandatory)][string]$FilePath,
    [string[]]$ArgumentList = @(),
    [string]$WorkingDirectory,
    [string]$RedirectStandardOutput,
    [string]$RedirectStandardError,
    [switch]$KillOnClose
  )
  if ($env:LF_SHOW_WINDOWS -eq "1" -or $env:SSH_CONNECTION) {
    $sp = @{ FilePath = $FilePath; PassThru = $true }
    if ($ArgumentList.Count) { $sp.ArgumentList = @($ArgumentList | ForEach-Object { [LfQuiet]::Quote($_) }) }
    if ($WorkingDirectory) { $sp.WorkingDirectory = $WorkingDirectory }
    if ($RedirectStandardOutput) { $sp.RedirectStandardOutput = $RedirectStandardOutput }
    if ($RedirectStandardError) { $sp.RedirectStandardError = $RedirectStandardError }
    $p = Start-Process @sp
    $null = $p.Handle
    return $p
  }
  $id = [LfQuiet]::Start($FilePath, [string[]]$ArgumentList, $WorkingDirectory,
    $RedirectStandardOutput, $RedirectStandardError, [bool]$KillOnClose)
  $p = [System.Diagnostics.Process]::GetProcessById($id)
  $null = $p.Handle # open now so ExitCode survives the child's exit
  return $p
}

# Node entry: Spec is base64 JSON {exe, args[], cwd, stdout?, stderr?}.
#   wait:   the child is in a kill-on-close job; PowerShell exits with its code,
#           and killing PowerShell kills the child.
#   detach: prints the child pid and returns; the child keeps running.
function Invoke-LfQuietSpec([string]$Spec, [string]$Mode) {
  $s = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($Spec)) | ConvertFrom-Json
  $launch = @{ FilePath = $s.exe; ArgumentList = @($s.args); KillOnClose = ($Mode -eq "wait") }
  if ($s.cwd) { $launch.WorkingDirectory = $s.cwd }
  if ($s.stdout) { $launch.RedirectStandardOutput = $s.stdout }
  if ($s.stderr) { $launch.RedirectStandardError = $s.stderr }
  try {
    $p = Start-LfQuietProcess @launch
  } catch {
    [Console]::Error.WriteLine("quiet-launch: $($_.Exception.Message)")
    exit 127
  }
  if ($Mode -eq "detach") { Write-Output $p.Id; exit 0 }
  $p.WaitForExit()
  exit $p.ExitCode
}

# Processes that own a window on LF-Agents: what `tools\cli hidden` lists.
# A console tool started with no window (the test server) is not listed; it
# dies with the run that started it.
function Get-LfQuietProcesses {
  foreach ($id in [LfQuiet]::WindowPids()) {
    $p = Get-Process -Id $id -ErrorAction SilentlyContinue
    if ($p) { [pscustomobject]@{ pid = $id; name = $p.ProcessName; started = $p.StartTime.ToString("s") } }
  }
}
