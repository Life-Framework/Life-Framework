// WASAPI loopback of the default render device -> raw PCM on stdout.
// Pads silence (WASAPI sends no packets while nothing plays) so the stream stays wall-clock aligned.
// Usage: lf-loopback.ps1 --info            prints "<rate> <channels> <bits> <float|int>"
//        lf-loopback.ps1 --diag             prints the default output endpoint id, volume, mute
//        lf-loopback.ps1 --use <endpointId>  makes that endpoint the default output at 100%, unmuted
//        lf-loopback.ps1 [seconds]         streams PCM in the device mix format to stdout
using System; using System.IO; using System.Diagnostics; using System.Threading; using System.Runtime.InteropServices;

[ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumerator {}
[Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDeviceEnumerator { int EnumAudioEndpoints(int a, int b, out IntPtr c); int GetDefaultAudioEndpoint(int flow, int role, out IMMDevice dev); int GetDevice([MarshalAs(UnmanagedType.LPWStr)] string id, out IMMDevice dev); }
[ComImport, Guid("870af99c-171d-4f9e-af0d-e63df40c2bc9")] class PolicyConfigClient {}
[Guid("f8679f50-850a-41cf-9c72-430f290290c8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IPolicyConfig { int a1(); int a2(); int a3(); int a4(); int a5(); int a6(); int a7(); int a8(); int a9(); int a10(); int SetDefaultEndpoint([MarshalAs(UnmanagedType.LPWStr)] string id, int role); }
[Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDevice { int Activate(ref Guid iid, int ctx, IntPtr p, [MarshalAs(UnmanagedType.IUnknown)] out object o); int OpenPropertyStore(int a, out IntPtr p); int GetId([MarshalAs(UnmanagedType.LPWStr)] out string id); }
[Guid("5CDF2C82-841E-4546-9722-0CF74078229A"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioEndpointVolume { int R1(IntPtr a); int R2(IntPtr a); int GetChannelCount(out uint c); int S1(float l, IntPtr g); int S2(float l, IntPtr g); int GetMasterVolumeLevel(out float l); int GetMasterVolumeLevelScalar(out float l); int S3(uint c, float l, IntPtr g); int S4(uint c, float l, IntPtr g); int G1(uint c, out float l); int G2(uint c, out float l); int SetMute([MarshalAs(UnmanagedType.Bool)] bool m, IntPtr g); int GetMute([MarshalAs(UnmanagedType.Bool)] out bool m); }
[Guid("1CB9AD4C-DBFA-4c32-B178-C2F568A703B2"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioClient {
  int Initialize(int share, int flags, long dur, long period, IntPtr fmt, IntPtr session);
  int GetBufferSize(out uint n); int GetStreamLatency(out long l); int GetCurrentPadding(out uint p);
  int IsFormatSupported(int s, IntPtr f, out IntPtr c); int GetMixFormat(out IntPtr f); int GetDevicePeriod(out long d, out long m);
  int Start(); int Stop(); int Reset(); int SetEventHandle(IntPtr h); int GetService(ref Guid iid, [MarshalAs(UnmanagedType.IUnknown)] out object o);
}
[Guid("C8ADBD64-E71E-48a0-A4DE-185C395CD317"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioCaptureClient { int GetBuffer(out IntPtr data, out uint frames, out uint flags, out ulong pos, out ulong qpc); int ReleaseBuffer(uint n); int GetNextPacketSize(out uint n); }

public static class LfLoopback {
  public static int Main(string[] a) {
    var en = (IMMDeviceEnumerator)new MMDeviceEnumerator();
    IMMDevice dev; Marshal.ThrowExceptionForHR(en.GetDefaultAudioEndpoint(0, 0, out dev));
    Guid iac = typeof(IAudioClient).GUID; object o;
    Marshal.ThrowExceptionForHR(dev.Activate(ref iac, 23, IntPtr.Zero, out o));
    var ac = (IAudioClient)o; IntPtr fmt; Marshal.ThrowExceptionForHR(ac.GetMixFormat(out fmt));
    int tag = Marshal.ReadInt16(fmt, 0), ch = Marshal.ReadInt16(fmt, 2), rate = Marshal.ReadInt32(fmt, 4), align = Marshal.ReadInt16(fmt, 12), bits = Marshal.ReadInt16(fmt, 14);
    bool isFloat = tag == 3;
    if (tag == unchecked((short)0xFFFE)) { byte[] g = new byte[16]; Marshal.Copy(fmt + 24, g, 0, 16); isFloat = new Guid(g) == new Guid("00000003-0000-0010-8000-00aa00389b71"); }
    if (a.Length > 1 && a[0] == "--use") { // make endpoint a[1] the default output at full volume, unmuted
      var pc = (IPolicyConfig)new PolicyConfigClient(); for (int role = 0; role < 3; role++) Marshal.ThrowExceptionForHR(pc.SetDefaultEndpoint(a[1], role));
      IMMDevice d2; Marshal.ThrowExceptionForHR(en.GetDevice(a[1], out d2)); Guid iv = typeof(IAudioEndpointVolume).GUID; object vo2; d2.Activate(ref iv, 23, IntPtr.Zero, out vo2);
      var ev2 = (IAudioEndpointVolume)vo2; ev2.S2(1.0f, IntPtr.Zero); ev2.SetMute(false, IntPtr.Zero); dev = d2; a = new[] { "--diag" }; }
    if (a.Length > 0 && a[0] == "--diag") { string id; dev.GetId(out id); Guid iev = typeof(IAudioEndpointVolume).GUID; object vo; dev.Activate(ref iev, 23, IntPtr.Zero, out vo); var ev = (IAudioEndpointVolume)vo; float v; bool m; ev.GetMasterVolumeLevelScalar(out v); ev.GetMute(out m); Console.WriteLine("id={0} volume={1} mute={2}", id, v, m); return 0; }
    if (a.Length > 0 && a[0] == "--info") { Console.WriteLine("{0} {1} {2} {3}", rate, ch, bits, isFloat ? "float" : "int"); return 0; }
    double secs = a.Length > 0 ? double.Parse(a[0], System.Globalization.CultureInfo.InvariantCulture) : 0;
    Marshal.ThrowExceptionForHR(ac.Initialize(0, 0x00020000 /*LOOPBACK*/, 2000000, 0, fmt, IntPtr.Zero));
    Guid icc = typeof(IAudioCaptureClient).GUID; Marshal.ThrowExceptionForHR(ac.GetService(ref icc, out o));
    var cc = (IAudioCaptureClient)o; var outp = Console.OpenStandardOutput();
    byte[] buf = new byte[rate * align]; long written = 0, real = 0; var sw = Stopwatch.StartNew();
    Marshal.ThrowExceptionForHR(ac.Start());
    try {
      while (secs <= 0 || sw.Elapsed.TotalSeconds < secs) {
        uint n; cc.GetNextPacketSize(out n);
        while (n > 0) {
          IntPtr d; uint fr, fl; ulong p1, p2; Marshal.ThrowExceptionForHR(cc.GetBuffer(out d, out fr, out fl, out p1, out p2));
          int bytes = (int)fr * align; if (bytes > buf.Length) buf = new byte[bytes];
          if ((fl & 2) != 0) Array.Clear(buf, 0, bytes); else Marshal.Copy(d, buf, 0, bytes);
          outp.Write(buf, 0, bytes); written += fr; real += fr; cc.ReleaseBuffer(fr); cc.GetNextPacketSize(out n);
        }
        long expected = (long)(sw.Elapsed.TotalSeconds * rate);
        if (expected - written > rate / 20) { // >50 ms gap: nothing is playing, pad with silence
          long pad = expected - written; Array.Clear(buf, 0, buf.Length);
          while (pad > 0) { int f = (int)Math.Min(pad, buf.Length / align); outp.Write(buf, 0, f * align); pad -= f; written += f; }
        }
        outp.Flush(); Thread.Sleep(10);
      }
    } catch (IOException) { } // ffmpeg closed the pipe
    ac.Stop(); Console.Error.WriteLine("loopback: real frames {0}, total {1}", real, written); return 0;
  }
}
