// Process loopback: records only what ONE process (and its children) plays, as a 48 kHz stereo float WAV.
// Windows 10 20348+ (AUDCLNT_PROCESS_LOOPBACK). Other host sounds do not matter.
// Usage: lf-procaudio.ps1 --pid <n> --out <file.wav> [--seconds s] [--stop-file <path>]
//        lf-procaudio.ps1 --endpoint     prints "id=.. volume=.. mute=.." of the default output (the guard reads it)
// Prints "ready epochMs=<unix ms of the audio clock origin>" on stdout once capturing, and
// "done frames=<n> real=<n> peak=<dBFS> epochMs=<n>" at the end. Gaps (nothing playing) are padded with silence
// at the packet's own QPC time, so sample N of the WAV is exactly N/48000 s after epochMs.
using System; using System.IO; using System.Diagnostics; using System.Threading; using System.Runtime.InteropServices;

[ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumerator2 {}
[Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDeviceEnumerator2 { int EnumAudioEndpoints(int a, int b, out IntPtr c); int GetDefaultAudioEndpoint(int flow, int role, out IMMDevice2 dev); }
[Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDevice2 { int Activate(ref Guid iid, int ctx, IntPtr p, [MarshalAs(UnmanagedType.IUnknown)] out object o); int OpenPropertyStore(int a, out IntPtr p); int GetId([MarshalAs(UnmanagedType.LPWStr)] out string id); }
[Guid("5CDF2C82-841E-4546-9722-0CF74078229A"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioEndpointVolume2 { int R1(IntPtr a); int R2(IntPtr a); int GetChannelCount(out uint c); int S1(float l, IntPtr g); int S2(float l, IntPtr g); int GetMasterVolumeLevel(out float l); int GetMasterVolumeLevelScalar(out float l); int S3(uint c, float l, IntPtr g); int S4(uint c, float l, IntPtr g); int G1(uint c, out float l); int G2(uint c, out float l); int SetMute([MarshalAs(UnmanagedType.Bool)] bool m, IntPtr g); int GetMute([MarshalAs(UnmanagedType.Bool)] out bool m); }
[Guid("1CB9AD4C-DBFA-4c32-B178-C2F568A703B2"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioClient2 {
  int Initialize(int share, int flags, long dur, long period, IntPtr fmt, IntPtr session);
  int GetBufferSize(out uint n); int GetStreamLatency(out long l); int GetCurrentPadding(out uint p);
  int IsFormatSupported(int s, IntPtr f, out IntPtr c); int GetMixFormat(out IntPtr f); int GetDevicePeriod(out long d, out long m);
  int Start(); int Stop(); int Reset(); int SetEventHandle(IntPtr h); int GetService(ref Guid iid, [MarshalAs(UnmanagedType.IUnknown)] out object o);
}
[Guid("C8ADBD64-E71E-48a0-A4DE-185C395CD317"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioCaptureClient2 { int GetBuffer(out IntPtr data, out uint frames, out uint flags, out ulong pos, out ulong qpc); int ReleaseBuffer(uint n); int GetNextPacketSize(out uint n); }
[ComImport, Guid("72A22D78-CDE4-431D-B8CC-843A71199B6D"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IActivateAudioInterfaceAsyncOperation { [PreserveSig] int GetActivateResult(out int hr, [MarshalAs(UnmanagedType.IUnknown)] out object iface); }
[ComImport, Guid("41D949AB-9862-444A-80F6-C261334DA5EB"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IActivateAudioInterfaceCompletionHandler { [PreserveSig] int ActivateCompleted(IActivateAudioInterfaceAsyncOperation op); }
[ComImport, Guid("94ea2b94-e9cc-49e0-c0ff-ee64ca8f5b90"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IAgileObject {}

public class ProcActivateHandler : IActivateAudioInterfaceCompletionHandler, IAgileObject {
  public ManualResetEvent Done = new ManualResetEvent(false);
  public int Hr = -1; public object Iface;
  public int ActivateCompleted(IActivateAudioInterfaceAsyncOperation op) { int hr; object o; op.GetActivateResult(out hr, out o); Hr = hr; Iface = o; Done.Set(); return 0; }
}

public static class LfProcAudio {
  [DllImport("Mmdevapi.dll", ExactSpelling = true, PreserveSig = false)]
  static extern void ActivateAudioInterfaceAsync([MarshalAs(UnmanagedType.LPWStr)] string path, ref Guid riid, IntPtr prm, IActivateAudioInterfaceCompletionHandler h, out IActivateAudioInterfaceAsyncOperation op);

  static string Arg(string[] a, string k) { for (int i = 0; i < a.Length - 1; i++) if (a[i] == k) return a[i + 1]; return null; }

  static void WriteHeader(Stream f, long dataBytes) {
    var w = new BinaryWriter(f); f.Seek(0, SeekOrigin.Begin);
    w.Write(new[] { 'R', 'I', 'F', 'F' }); w.Write((int)(36 + dataBytes)); w.Write(new[] { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
    w.Write(16); w.Write((short)3); w.Write((short)2); w.Write(48000); w.Write(48000 * 8); w.Write((short)8); w.Write((short)32);
    w.Write(new[] { 'd', 'a', 't', 'a' }); w.Write((int)dataBytes); w.Flush();
  }

  // PowerShell's thread is STA; the activation result lives in the MTA, and IAudioClient has no proxy to cross apartments.
  public static int Main(string[] a) {
    int code = 1; var t = new Thread(() => { code = Run(a); }); t.SetApartmentState(ApartmentState.MTA); t.Start(); t.Join(); return code;
  }

  static int Run(string[] a) {
    if (a.Length > 0 && a[0] == "--endpoint") {
      var en = (IMMDeviceEnumerator2)new MMDeviceEnumerator2(); IMMDevice2 dev; Marshal.ThrowExceptionForHR(en.GetDefaultAudioEndpoint(0, 0, out dev));
      string id; dev.GetId(out id); Guid iev = typeof(IAudioEndpointVolume2).GUID; object vo; dev.Activate(ref iev, 23, IntPtr.Zero, out vo);
      var evv = (IAudioEndpointVolume2)vo; float v; bool m; evv.GetMasterVolumeLevelScalar(out v); evv.GetMute(out m);
      Console.WriteLine("id={0} volume={1} mute={2}", id, v.ToString(System.Globalization.CultureInfo.InvariantCulture), m); return 0;
    }
    string pidS = Arg(a, "--pid"), outPath = Arg(a, "--out"), secS = Arg(a, "--seconds"), stopFile = Arg(a, "--stop-file");
    if (pidS == null || outPath == null) { Console.Error.WriteLine("usage: --pid <n> --out <file.wav> [--seconds s] [--stop-file path]"); return 2; }
    double secs = secS == null ? 0 : double.Parse(secS, System.Globalization.CultureInfo.InvariantCulture);

    // AUDIOCLIENT_ACTIVATION_PARAMS { type=PROCESS_LOOPBACK(1), pid, mode=INCLUDE_TARGET_PROCESS_TREE(0) } inside a VT_BLOB PROPVARIANT
    IntPtr prm = Marshal.AllocHGlobal(12); Marshal.WriteInt32(prm, 0, 1); Marshal.WriteInt32(prm, 4, int.Parse(pidS)); Marshal.WriteInt32(prm, 8, 0);
    IntPtr pv = Marshal.AllocHGlobal(24); for (int i = 0; i < 24; i++) Marshal.WriteByte(pv, i, 0);
    Marshal.WriteInt16(pv, 0, 65); Marshal.WriteInt32(pv, 8, 12); Marshal.WriteIntPtr(pv, 16, prm);
    var h = new ProcActivateHandler(); Guid iac = typeof(IAudioClient2).GUID; IActivateAudioInterfaceAsyncOperation op;
    ActivateAudioInterfaceAsync("VAD\\Process_Loopback", ref iac, pv, h, out op);
    if (!h.Done.WaitOne(10000)) { Console.Error.WriteLine("error: activation timed out"); return 3; }
    if (h.Hr != 0) { Console.Error.WriteLine("error: activation failed hr=0x" + h.Hr.ToString("X8") + " (process loopback needs Windows 10 build 20348 or newer and a live pid)"); return 3; }
    var ac = (IAudioClient2)h.Iface;
    IntPtr fmt = Marshal.AllocHGlobal(18);
    Marshal.WriteInt16(fmt, 0, 3); Marshal.WriteInt16(fmt, 2, 2); Marshal.WriteInt32(fmt, 4, 48000); Marshal.WriteInt32(fmt, 8, 48000 * 8); Marshal.WriteInt16(fmt, 12, 8); Marshal.WriteInt16(fmt, 14, 32); Marshal.WriteInt16(fmt, 16, 0);
    const int LOOPBACK = 0x00020000, EVENTCB = 0x00040000, AUTOPCM = unchecked((int)0x80000000), SRCQ = 0x08000000;
    Marshal.ThrowExceptionForHR(ac.Initialize(0, LOOPBACK | EVENTCB | AUTOPCM | SRCQ, 0, 0, fmt, IntPtr.Zero));
    var evt = new AutoResetEvent(false); Marshal.ThrowExceptionForHR(ac.SetEventHandle(evt.SafeWaitHandle.DangerousGetHandle()));
    Guid icc = typeof(IAudioCaptureClient2).GUID; object o; Marshal.ThrowExceptionForHR(ac.GetService(ref icc, out o));
    var cc = (IAudioCaptureClient2)o;

    const int ALIGN = 8; const int RATE = 48000;
    byte[] buf = new byte[RATE * ALIGN / 10]; long written = 0, real = 0; double peak = 0;
    using (var f = new FileStream(outPath, FileMode.Create, FileAccess.ReadWrite)) {
      WriteHeader(f, 0); f.Seek(44, SeekOrigin.Begin);
      long startTs = Stopwatch.GetTimestamp(); long epochMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
      Marshal.ThrowExceptionForHR(ac.Start());
      Console.WriteLine("ready epochMs=" + epochMs); Console.Out.Flush();
      var sw = Stopwatch.StartNew();
      while (true) {
        evt.WaitOne(20);
        uint n; cc.GetNextPacketSize(out n);
        while (n > 0) {
          IntPtr d; uint fr, fl; ulong p1, qpc; Marshal.ThrowExceptionForHR(cc.GetBuffer(out d, out fr, out fl, out p1, out qpc));
          // qpc is in 100 ns units on the QPC clock; place the packet at its own time so gaps become silence.
          long at = written;
          if (qpc != 0) { double rel = ((double)qpc / 1e7) - ((double)startTs / Stopwatch.Frequency); at = (long)Math.Round(rel * RATE); }
          if (at > written + 480) Pad(f, ref buf, at - written, ref written);
          int bytes = (int)fr * ALIGN; if (bytes > buf.Length) buf = new byte[bytes];
          if ((fl & 2) != 0) Array.Clear(buf, 0, bytes); else {
            Marshal.Copy(d, buf, 0, bytes);
            for (int i = 0; i < bytes; i += 4) { double v = Math.Abs(BitConverter.ToSingle(buf, i)); if (v > peak) peak = v; }
            real += fr;
          }
          f.Write(buf, 0, bytes); written += fr; cc.ReleaseBuffer(fr); cc.GetNextPacketSize(out n);
        }
        double now = sw.Elapsed.TotalSeconds;
        if (secs > 0 && now >= secs) break;
        if (stopFile != null && File.Exists(stopFile)) break;
        long expected = (long)(now * RATE);
        if (expected - written > RATE / 4) Pad(f, ref buf, expected - written - RATE / 20, ref written); // nothing playing: keep the length near wall time
      }
      ac.Stop();
      WriteHeader(f, written * ALIGN);
      Console.WriteLine("done frames={0} real={1} peak={2:F1} epochMs={3}", written, real, peak > 0 ? 20 * Math.Log10(peak) : -200.0, epochMs);
    }
    return 0;
  }

  static void Pad(Stream f, ref byte[] buf, long frames, ref long written) {
    Array.Clear(buf, 0, buf.Length);
    while (frames > 0) { int n = (int)Math.Min(frames, buf.Length / 8); f.Write(buf, 0, n * 8); frames -= n; written += n; }
  }
}
