// A validate check that fails without printing a word (killed, or crashed before it could write) used to leave a
// bare `FAIL  validate/x.mjs (5.8 s)` line in CI: three selftests failed that way on 2026-11-16/17 and the log held
// nothing to read. This is the one line `cli validate` prints under such a header. res: { status, signal, stdout,
// stderr, timedOut } as invokeScriptAsync returns it; "" when the check did say something.
export function silentFailureNote(name, res) {
  if (res.status === 0 || String(res.stdout ?? "").trim() || String(res.stderr ?? "").trim()) return "";
  const how = [`exit code ${res.status}`];
  if (res.signal) how.push(`signal ${res.signal}`);
  if (res.timedOut) how.push("killed at its time budget");
  return `  ${name} printed nothing and ended with ${how.join(", ")}`;
}
