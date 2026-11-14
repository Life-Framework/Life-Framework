// tools/lib/wb-window-capture.mjs - PNG of a Workbench Qt window (Win32 PrintWindow).
//
//   import { captureWorkbench, listWorkbenchWindows, findWorkbenchPids } from "./wb-window-capture.mjs";
//   const r = await captureWorkbench({ pid, title: "World Editor", out: "tmp/wb.png", maxPx: 2400 });
//
// The Win32 half is wb-window-capture.ps1 (PrintWindow, GetDIBits, 24-bit top-down BGR).
// This half converts BGR to RGB, optionally box-downscales, and encodes the PNG.
// Spec: wb-tooling-spec.md item 6. Safety: only the given pid's windows, no input,
// no foreground change, no screen grab (see the .ps1 header).
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync, existsSync, unlinkSync, rmdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { encodePng } from "../../agent-box/bmp.mjs";

const PS1 = join(dirname(fileURLToPath(import.meta.url)), "wb-window-capture.ps1");
const PWSH = join(process.env.SystemRoot || "C:\\Windows", "System32", "WindowsPowerShell", "v1.0", "powershell.exe");

function runPs(args) {
  const r = spawnSync(PWSH, ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", PS1, ...args],
    { encoding: "utf8", windowsHide: true, timeout: 60000, maxBuffer: 16 * 1024 * 1024 });
  if (r.error) throw new Error(`wb-window-capture.ps1: ${r.error.message}`);
  const text = (r.stdout || "").trim();
  try {
    return JSON.parse(text.slice(text.indexOf("{")));
  } catch {
    throw new Error(`wb-window-capture.ps1 gave no JSON (exit ${r.status}): ${(r.stderr || text).slice(0, 400)}`);
  }
}

// Workbench pids that own a window (any desktop the .ps1 can see: LF-Agents first).
export function findWorkbenchPids() {
  const r = spawnSync("tasklist", ["/FI", "IMAGENAME eq ArmaReforgerWorkbench*", "/FO", "CSV", "/NH"], { encoding: "utf8", windowsHide: true });
  return (r.stdout || "").split(/\r?\n/).map((l) => /^"[^"]+","(\d+)"/.exec(l)?.[1]).filter(Boolean).map(Number);
}

export function listWorkbenchWindows(pid) {
  return runPs(["-Mode", "list", "-WbPid", String(pid)]);
}

// Box-average downscale of tight RGB so the longest side is at most maxPx.
export function downscale(width, height, rgb, maxPx) {
  const long = Math.max(width, height);
  if (!maxPx || long <= maxPx) return { width, height, rgb };
  const f = long / maxPx;
  const w = Math.max(1, Math.round(width / f)), h = Math.max(1, Math.round(height / f));
  const out = Buffer.alloc(w * h * 3);
  for (let y = 0; y < h; y++) {
    const y0 = Math.floor((y * height) / h), y1 = Math.max(y0 + 1, Math.floor(((y + 1) * height) / h));
    for (let x = 0; x < w; x++) {
      const x0 = Math.floor((x * width) / w), x1 = Math.max(x0 + 1, Math.floor(((x + 1) * width) / w));
      let r = 0, g = 0, b = 0, n = 0;
      for (let yy = y0; yy < y1; yy++) for (let xx = x0; xx < x1; xx++) {
        const i = (yy * width + xx) * 3;
        r += rgb[i]; g += rgb[i + 1]; b += rgb[i + 2]; n++;
      }
      const o = (y * w + x) * 3;
      out[o] = Math.round(r / n); out[o + 1] = Math.round(g / n); out[o + 2] = Math.round(b / n);
    }
  }
  return { width: w, height: h, rgb: out };
}

// Padded top-down 24-bit BGR scanlines -> tight RGB.
export function bgrToRgb(width, height, bgr) {
  const stride = (width * 3 + 3) & ~3;
  const out = Buffer.alloc(width * height * 3);
  for (let y = 0; y < height; y++) {
    let s = y * stride, o = y * width * 3;
    for (let x = 0; x < width; x++, s += 3, o += 3) {
      out[o] = bgr[s + 2]; out[o + 1] = bgr[s + 1]; out[o + 2] = bgr[s];
    }
  }
  return out;
}

// Crop tight RGB to {x,y,w,h} (clamped), so one panel of an ultrawide Workbench stays legible.
export function crop(width, height, rgb, c) {
  const x = Math.max(0, Math.min(width - 1, c.x | 0)), y = Math.max(0, Math.min(height - 1, c.y | 0));
  const w = Math.max(1, Math.min(width - x, c.w | 0)), h = Math.max(1, Math.min(height - y, c.h | 0));
  const out = Buffer.alloc(w * h * 3);
  for (let r = 0; r < h; r++) rgb.copy(out, r * w * 3, ((y + r) * width + x) * 3, ((y + r) * width + x + w) * 3);
  return { width: w, height: h, rgb: out };
}

// A flat or all-black picture means PrintWindow painted nothing (not an active, rendered desktop).
export function colorStats(rgb) {
  const seen = new Set();
  let sum = 0, sumSq = 0, n = 0;
  for (let i = 0; i < rgb.length; i += 3 * 7) {
    const v = (rgb[i] + rgb[i + 1] + rgb[i + 2]) / 3;
    sum += v; sumSq += v * v; n++;
    if (seen.size < 5000) seen.add((rgb[i] << 16) | (rgb[i + 1] << 8) | rgb[i + 2]);
  }
  const mean = sum / n;
  return { colors: seen.size, mean: Math.round(mean * 10) / 10, stdev: Math.round(Math.sqrt(Math.max(0, sumSq / n - mean * mean)) * 10) / 10 };
}

// Capture one window of `pid`. Returns { ok, out, width, height, scaled, flag, desktop, window, stats, blank, error }.
export async function captureWorkbench({ pid, title = "", hwnd = 0, out, maxPx = 0, cropBox = null }) {
  if (!Number.isInteger(pid) || pid <= 0) throw new Error("captureWorkbench needs the exact Workbench pid");
  const dir = mkdtempSync(join(tmpdir(), "lf-wbcap-"));
  const raw = join(dir, "frame.bgr");
  try {
    const args = ["-Mode", "capture", "-WbPid", String(pid), "-Raw", raw];
    if (title) args.push("-Title", title);
    if (hwnd) args.push("-Hwnd", String(hwnd));
    const info = runPs(args);
    if (!info.ok) return { ...info, ok: false };
    const { width, height } = info.window;
    const rgb = bgrToRgb(width, height, readFileSync(raw));
    const stats = colorStats(rgb);
    const part = cropBox ? crop(width, height, rgb, cropBox) : { width, height, rgb };
    const small = downscale(part.width, part.height, part.rgb, maxPx);
    const outPath = resolve(out);
    mkdirSync(dirname(outPath), { recursive: true });
    writeFileSync(outPath, encodePng(small.width, small.height, small.rgb));
    // A real Qt UI has thousands of colours (text antialiasing); a blank frame has a handful.
    const blank = stats.colors < 16;
    return { ...info, out: outPath, width: small.width, height: small.height, fullWidth: width, fullHeight: height,
      scaled: small.width !== part.width, stats, blank };
  } finally {
    try { if (existsSync(raw)) unlinkSync(raw); rmdirSync(dir); } catch { /* best effort */ }
  }
}
