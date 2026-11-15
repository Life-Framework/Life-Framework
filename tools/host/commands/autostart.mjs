// `tools\cli host autostart install|remove|status [--dry-run]` (#1504). The logic is tools/host/autostart.mjs.
//
// Registers a Windows scheduled task that starts `host watch --flagship` at logon of this user,
// hidden, so the PC host returns after a reboot (the watchdog alone covers only a crashed server).
import { isMainCheckout } from "../../wt.mjs";
import { currentUser, installTask, queryTask, realRunner, removeTask } from "../autostart.mjs";

export const help = "autostart install|remove|status [--dry-run]   a Windows scheduled task runs `host watch --flagship` hidden at logon for this user, so the host returns after a reboot; status says whether it is registered and when it last ran; --dry-run prints the schtasks arguments and changes nothing; refuses in the main checkout";

const say = (m) => console.log(`[LFDebug:HostAutostart] ${m}`);

// --dry-run: reads as "not registered", prints what a real run would send, never starts schtasks.
function dryRunner() {
  return (args) => {
    if (args[0] === "/Query") return { status: 1, stdout: "", stderr: "" };
    say(`dry-run: schtasks ${args.map((a) => (/\s/.test(a) ? `"${a}"` : a)).join(" ")}`);
    return { status: 0, stdout: "", stderr: "" };
  };
}

// ctx.runner, ctx.isMain, ctx.nodePath and ctx.user are injected by the selftest.
export async function main(args, ctx) {
  const sub = args[0];
  if (!["install", "remove", "status"].includes(sub)) {
    say("usage: host autostart install|remove|status [--dry-run]");
    return 1;
  }
  const dry = args.includes("--dry-run");
  const isMain = ctx.isMain ?? isMainCheckout(ctx.root);
  if (isMain && sub !== "status") {
    say("REFUSE: this is the main checkout, the world-editor copy; the task would point at it. Run from a worktree (tools\\cli wt new <slug>).");
    return 1;
  }
  if (!ctx.runner && !dry && process.platform !== "win32") {
    say("autostart needs Windows (schtasks)");
    return 1;
  }
  const run = dry ? dryRunner() : ctx.runner ?? realRunner;
  const nodePath = ctx.nodePath ?? process.execPath;
  const user = ctx.user ?? currentUser();

  if (sub === "install") {
    const r = installTask({ run, root: ctx.root, nodePath, user });
    say(dry ? `dry-run, nothing registered: ${r.message}` : r.message);
    return r.ok ? 0 : 1;
  }
  if (sub === "remove") {
    const r = removeTask({ run });
    say(r.message);
    return r.ok ? 0 : 1;
  }
  const q = queryTask(run);
  if (q.error) {
    say(q.error);
    return 1;
  }
  const i = q.info;
  if (!i.registered) {
    say("not registered; `host autostart install` registers it");
    return 1;
  }
  say(`registered: runs at logon as ${i.user || "?"}, status ${i.status || "?"}`);
  say(i.ran ? `last run ${i.lastRun}, result ${i.lastResult}` : "never ran since it was registered");
  say(`runs: ${i.command}`);
  return 0;
}
