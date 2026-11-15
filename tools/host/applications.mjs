// The police application queue, read from the server's text record.
//
// Feature/Police/LF_PoliceApplications.c writes $profile:LifeFramework/police_applications.txt, one
// `STATUS|uid|name|why|availability|unix|rejectedAt|trialEndsAt` line each (STATUS is OPEN,
// APPROVED, REJECTED or EXPIRED). Older rows have six fields (no rejectedAt, no trialEndsAt) or
// seven (no trialEndsAt); the missing numbers read as 0. This module only reads: approving and
// rejecting stay in the game. `tools\cli host applications` prints the queue before a session.

export const STATUSES = ["OPEN", "APPROVED", "REJECTED", "EXPIRED"];
const MIN_FIELDS = 6;
const MAX_FIELDS = 8;

// Parse the file text. Returns { rows, warnings }: a row that is not a whole application is
// skipped with a warning that names its (1-based) line. Blank lines are ignored.
export function parseApplications(text) {
  const rows = [];
  const warnings = [];
  const lines = String(text).split("\n");
  lines.forEach((raw, i) => {
    const line = raw.replace(/\r$/, "");
    if (line.trim() === "") return;
    const at = `line ${i + 1}`;
    const parts = line.split("|");
    if (parts.length < MIN_FIELDS || parts.length > MAX_FIELDS) {
      warnings.push(`${at}: ${parts.length} fields, expected ${MIN_FIELDS} to ${MAX_FIELDS}, skipped`);
      return;
    }
    const status = parts[0];
    if (!STATUSES.includes(status)) {
      warnings.push(`${at}: unknown status "${status}", skipped`);
      return;
    }
    const nums = [];
    for (const k of [5, 6, 7]) {
      if (k >= parts.length) {
        nums.push(0);
        continue;
      }
      const s = parts[k].trim();
      if (!/^\d+$/.test(s)) {
        warnings.push(`${at}: field ${k + 1} "${parts[k]}" is not a number, skipped`);
        return;
      }
      nums.push(Number(s));
    }
    rows.push({
      status,
      uid: parts[1],
      name: parts[2],
      why: parts[3],
      availability: parts[4],
      time: nums[0],
      rejectedAt: nums[1],
      trialEndsAt: nums[2],
    });
  });
  return { rows, warnings };
}

// "6d 22h", "13h" or "40m": how long ago `unix` was at `now`.
export function formatAge(seconds) {
  const s = Math.max(0, Math.floor(seconds));
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  if (d > 0) return `${d}d ${h}h`;
  if (h > 0) return `${h}h`;
  return `${Math.floor(s / 60)}m`;
}

function isoDate(unix) {
  return new Date(unix * 1000).toISOString().slice(0, 10);
}

// Left-aligned columns, two spaces apart, indented two, no trailing spaces.
function table(head, body) {
  const all = [head, ...body];
  const w = head.map((_, i) => Math.max(...all.map((r) => r[i].length)));
  return all.map((r) => `  ${r.map((c, i) => c.padEnd(w[i])).join("  ")}`.trimEnd()).join("\n");
}

// The queue as text (ends with a newline). Open applications oldest first, then the officers on
// trial with the hours left; `all` adds the rejected, expired and trial-free approved rows.
export function formatQueue(rows, now, { all = false } = {}) {
  const open = rows.filter((r) => r.status === "OPEN").sort((a, b) => a.time - b.time);
  const trial = rows.filter((r) => r.status === "APPROVED" && r.trialEndsAt > 0).sort((a, b) => a.trialEndsAt - b.trialEndsAt);
  const out = [];

  if (open.length === 0) out.push("No open applications.");
  else {
    out.push(`Open applications (${open.length}), oldest first:`);
    out.push(table(["#", "Name", "Waiting", "Availability", "Why"], open.map((r, i) => [String(i + 1), r.name, formatAge(now - r.time), r.availability, r.why])));
  }
  out.push("");

  if (trial.length === 0) out.push("No officers on trial.");
  else {
    out.push(`Officers on trial (${trial.length}):`);
    out.push(table(["Name", "Hours left", "Availability", "Account"], trial.map((r) => [r.name, `${Math.max(0, Math.ceil((r.trialEndsAt - now) / 3600))}h`, r.availability, r.uid])));
  }

  if (all) {
    const closed = [
      ["Rejected", rows.filter((r) => r.status === "REJECTED"), (r) => r.rejectedAt || r.time],
      ["Expired", rows.filter((r) => r.status === "EXPIRED"), (r) => r.time],
      ["Approved, no trial running", rows.filter((r) => r.status === "APPROVED" && r.trialEndsAt <= 0), (r) => r.time],
    ];
    for (const [title, list, when] of closed) {
      if (list.length === 0) continue;
      out.push("");
      out.push(`${title} (${list.length}):`);
      out.push(table(["Name", "Date", "Why"], list.sort((a, b) => when(a) - when(b)).map((r) => [r.name, isoDate(when(r)), r.why])));
    }
  }
  return `${out.join("\n")}\n`;
}
