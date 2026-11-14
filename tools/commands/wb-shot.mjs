// tools\cli wb-shot: PNG of a Workbench Qt window (Log Console, Object Properties, menus) via PrintWindow.
//
//   tools\cli wb-shot --pid 81820 --title "World Editor" --out tmp/wb.png [--max-px 2400]
//   tools\cli wb-shot --pid 81820 --list           windows of that pid (hwnd, title, size, visible, minimised)
//   tools\cli wb-shot --pid 81820 --list --wait 240   block (bounded) until the pid shows a visible window
//   tools\cli wb-shot --find                      Workbench pids
//
// --pid is required: only that process's windows are listed or captured, nothing is clicked or focused,
// and the screen is never grabbed. It needs an active, rendered desktop and a non-minimised window
// (spec item 6); docs/setup.md "Workbench window capture" records what works on LF-Agents.
import { captureWorkbench, findWorkbenchPids, listWorkbenchWindows } from "../lib/wb-window-capture.mjs";

export const help = "wb-shot --pid N [--title t] [--out f.png] [--crop x,y,w,h] [--max-px N] | --list | --find   PNG of a Workbench Qt window (PrintWindow)";

function opt(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

export async function main(args, { root }) {
  if (args.includes("--find")) {
    console.log(findWorkbenchPids().join("\n") || "no Workbench process");
    return 0;
  }
  const pid = Number(opt(args, "--pid"));
  if (!Number.isInteger(pid) || pid <= 0) {
    console.log("usage: tools\\cli wb-shot --pid <Workbench pid> [--title \"World Editor\"] [--out file.png] [--max-px N] | --list | --find");
    return 1;
  }
  const waitS = Number(opt(args, "--wait")) || 0;
  if (args.includes("--list") || waitS) {
    let r = listWorkbenchWindows(pid);
    const until = Date.now() + waitS * 1000;
    while (waitS && Date.now() < until && !r.windows.some((x) => x.visible && x.width > 100)) {
      await new Promise((d) => setTimeout(d, 3000));
      r = listWorkbenchWindows(pid);
    }
    console.log(`pid ${r.pid} desktop '${r.desktop}'`);
    for (const w of r.windows.filter((x) => x.visible)) console.log(`  ${w.hwnd}  ${w.width}x${w.height}${w.minimized ? " minimised" : ""}  ${w.title || "(untitled)"}  [${w.class}]`);
    return 0;
  }
  const c = (opt(args, "--crop") || "").split(",").map(Number);
  const cropBox = c.length === 4 && c.every(Number.isFinite) ? { x: c[0], y: c[1], w: c[2], h: c[3] } : null;
  const out = opt(args, "--out") || `${root}/tmp/wb-shot-${pid}.png`;
  const r = await captureWorkbench({ pid, title: opt(args, "--title") || "", hwnd: Number(opt(args, "--hwnd")) || 0, out, maxPx: Number(opt(args, "--max-px")) || 0, cropBox });
  if (!r.ok) {
    console.log(`FAIL  ${r.error}`);
    return 1;
  }
  console.log(`${r.blank ? "WARN  blank frame (PrintWindow painted nothing): " : "OK    "}${r.out}  ${r.width}x${r.height}` +
    `${r.scaled ? ` (from ${r.fullWidth}x${r.fullHeight})` : ""}  ${r.window.title}  desktop '${r.desktop}'  flag ${r.flag}  colors ${r.stats.colors} stdev ${r.stats.stdev}`);
  return r.blank ? 1 : 0;
}
