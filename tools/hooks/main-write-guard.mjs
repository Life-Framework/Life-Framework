// PreToolUse hook (.claude/settings.json): refuses a Write/Edit/NotebookEdit into the lead's main checkout from an agent
// that works in a worktree (#2785). The rule is tools/lib/main-write-guard.mjs; this file only reads the payload from
// stdin and prints the deny. Any failure here allows the call: a broken guard must never stop work.
import { decideWrite } from "../lib/main-write-guard.mjs";

let raw = "";
process.stdin.setEncoding("utf8");
process.stdin.on("data", (c) => { raw += c; });
process.stdin.on("end", () => {
  try {
    const input = JSON.parse(raw || "{}");
    const d = decideWrite(input, { projectDir: process.env.CLAUDE_PROJECT_DIR });
    if (d.block) {
      process.stdout.write(JSON.stringify({
        hookSpecificOutput: { hookEventName: "PreToolUse", permissionDecision: "deny", permissionDecisionReason: d.reason },
      }));
    }
  } catch { /* allow */ }
  process.exit(0);
});
