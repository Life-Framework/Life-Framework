# Installs the Virtual Display Driver (VirtualDrivers/Virtual-Display-Driver, MIT, catalog signed by
# SignPath Foundation) as a root-enumerated display adapter with one 1920x1080 monitor. No reboot.
# Run elevated on the box (an admin ssh session is). Idempotent. docs/worktrees.md "Desktop on the box".
param([string]$Version = '25.7.23', [string]$Work = 'C:\LF\desktop\vdd')
$ErrorActionPreference = 'Stop'
$zip = Join-Path $Work 'vdd.zip'; $drv = Join-Path $Work 'drv\VirtualDisplayDriver'
New-Item -ItemType Directory -Force $Work | Out-Null
if (-not (Test-Path (Join-Path $drv 'MttVDD.inf'))) {
  Invoke-WebRequest -UseBasicParsing "https://github.com/VirtualDrivers/Virtual-Display-Driver/releases/download/$Version/VirtualDisplayDriver-x86.Driver.Only.zip" -OutFile $zip
  Expand-Archive $zip -DestinationPath (Join-Path $Work 'drv') -Force
}
$sig = Get-AuthenticodeSignature (Join-Path $drv 'mttvdd.cat')
if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch 'SignPath Foundation') { throw "driver catalog signature: $($sig.Status) $($sig.SignerCertificate.Subject)" }
# The driver reads its monitors and modes from here.
New-Item -ItemType Directory -Force C:\VirtualDisplayDriver | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'vdd_settings.xml') C:\VirtualDisplayDriver\vdd_settings.xml -Force
# Trust the catalog signer so the install does not stop at a "trust this publisher" prompt.
$store = New-Object System.Security.Cryptography.X509Certificates.X509Store('TrustedPublisher', 'LocalMachine')
$store.Open('ReadWrite'); $store.Add($sig.SignerCertificate); $store.Close()
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class LfVdd {
  [StructLayout(LayoutKind.Sequential)] public struct SP_DEVINFO_DATA { public int cbSize; public Guid ClassGuid; public int DevInst; public IntPtr Reserved; }
  [DllImport("setupapi.dll", SetLastError=true)] static extern IntPtr SetupDiCreateDeviceInfoList(ref Guid g, IntPtr h);
  [DllImport("setupapi.dll", SetLastError=true, CharSet=CharSet.Unicode)] static extern bool SetupDiCreateDeviceInfoW(IntPtr s, string name, ref Guid g, string desc, IntPtr h, int flags, ref SP_DEVINFO_DATA d);
  [DllImport("setupapi.dll", SetLastError=true, CharSet=CharSet.Unicode)] static extern bool SetupDiSetDeviceRegistryPropertyW(IntPtr s, ref SP_DEVINFO_DATA d, int prop, byte[] buf, int size);
  [DllImport("setupapi.dll", SetLastError=true)] static extern bool SetupDiCallClassInstaller(int fn, IntPtr s, ref SP_DEVINFO_DATA d);
  [DllImport("setupapi.dll", SetLastError=true)] static extern bool SetupDiDestroyDeviceInfoList(IntPtr s);
  [DllImport("newdev.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern bool UpdateDriverForPlugAndPlayDevicesW(IntPtr hwnd, string hwid, string inf, int flags, out bool reboot);
  public static string CreateNode(string hwid) { // devcon install, step 1: a root device node of class Display
    Guid g = new Guid("4d36e968-e325-11ce-bfc1-08002be10318");
    IntPtr s = SetupDiCreateDeviceInfoList(ref g, IntPtr.Zero);
    var d = new SP_DEVINFO_DATA(); d.cbSize = Marshal.SizeOf(d);
    if (!SetupDiCreateDeviceInfoW(s, "Display", ref g, null, IntPtr.Zero, 1, ref d)) return "create error " + Marshal.GetLastWin32Error();
    byte[] b = System.Text.Encoding.Unicode.GetBytes(hwid + "\0\0");
    if (!SetupDiSetDeviceRegistryPropertyW(s, ref d, 1 /*SPDRP_HARDWAREID*/, b, b.Length)) return "hwid error " + Marshal.GetLastWin32Error();
    if (!SetupDiCallClassInstaller(0x19 /*DIF_REGISTERDEVICE*/, s, ref d)) return "register error " + Marshal.GetLastWin32Error();
    SetupDiDestroyDeviceInfoList(s); return "created";
  }
}
'@
if (Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object { $_.HardwareID -contains 'Root\MttVDD' }) { 'device node: present' }
else { 'device node: ' + [LfVdd]::CreateNode('Root\MttVDD') }
$reboot = $false
$ok = [LfVdd]::UpdateDriverForPlugAndPlayDevicesW([IntPtr]::Zero, 'Root\MttVDD', (Join-Path $drv 'MttVDD.inf'), 1, [ref]$reboot)
"driver: ok=$ok error=$([Runtime.InteropServices.Marshal]::GetLastWin32Error()) reboot=$reboot"
Get-PnpDevice -Class Display, Monitor | Format-Table -AutoSize Status, FriendlyName
