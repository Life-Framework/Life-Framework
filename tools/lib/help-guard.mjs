// tools/lib/help-guard.mjs - one argument guard for the heavy commands.
//
// `tools\cli e2e --help` used to start a real e2e run: only some commands knew the flag. tools/cli.mjs
// asks helpRequest() before it dispatches anything; a hit prints that command's usage and exits 0,
// so no lock, build, boot or pool ticket is ever taken for a question about flags.
// The usage text is cut from the CLI's own help (cmdHelpFull / wtHelpFull), so it cannot drift.

// Commands that build, boot a server or a client, take a lane or a Workbench.
export const HEAVY_HELP = ["build", "pack", "serve", "dev", "test", "ci", "e2e", "look", "session"];

// wt subcommands: every one, so `wt new --help` never creates a worktree named "--help".
export const WT_HELP = ["new", "warm", "list", "test", "e2e", "dev", "build", "gate", "ci", "logs", "sync", "sync-base", "ship", "prune", "open", "main"];

export function wantsHelp(args) {
  return (args ?? []).some((a) => a === "--help" || a === "-h");
}

// { cmd, sub } when this command line is a request for usage, else null.
export function helpRequest(cmd, rest) {
  if (!cmd || !wantsHelp(rest)) return null;
  if (cmd === "wt") {
    const sub = rest[0];
    return WT_HELP.includes(sub) ? { cmd, sub } : null; // `wt --help` and `wt help` already print wt's help
  }
  return HEAVY_HELP.includes(cmd) ? { cmd, sub: null } : null;
}

// The entries of a help text for one command. An entry starts at a line indented by exactly two
// spaces (`  test [--no-build] ...`) or, in the wt help, `  tools\cli wt gate <slug> ...`; the
// lines indented deeper that follow belong to it.
export function usageLines(helpText, cmd, sub = null) {
  const lines = String(helpText).split(/\r?\n/);
  const out = [];
  let keep = false;
  for (const line of lines) {
    const m = /^ {2}(\S+)(?: (\S+))?(?: (\S+))?/.exec(line);
    if (m && !/^ {3}/.test(line)) {
      if (cmd === "wt") {
        const isWt = /^tools.cli$/.test(m[1]) && m[2] === "wt";
        const name = isWt ? m[3] : null;
        keep = isWt && (name === sub || (sub === "ci" && name === "gate"));
      } else {
        keep = m[1] === cmd;
      }
    } else if (!/^ {3}/.test(line) && line.trim() !== "") {
      keep = false; // a flush-left line ends the entries
    }
    if (keep) out.push(line.replace(/^ {2}/, ""));
  }
  return out;
}

export function usageText(helpText, req) {
  const lines = usageLines(helpText, req.cmd, req.sub);
  while (lines.length && lines[lines.length - 1].trim() === "") lines.pop();
  const name = req.sub ? `tools\\cli ${req.cmd} ${req.sub}` : `tools\\cli ${req.cmd}`;
  if (!lines.length) return `usage: ${name} [flags]; no flag list recorded, see 'tools\\cli ${req.cmd === "wt" ? "wt help" : "help"} --verbose'. --help never runs the command.`;
  return `usage: ${name}  (--help never runs the command)\n${lines.join("\n")}`;
}
