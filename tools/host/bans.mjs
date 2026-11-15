// Ban record reader (`tools\cli host bans`, #1609). Reads the host's
// $profile:LifeFramework/ban_record.txt, which LF_BanRecord.c (#1285) appends to, one row per vanilla
// `#ban create` (BAN) and `#ban remove` (UNBAN):
//   V1|ACTION|targetUid|unix|staffUid|durationSeconds|incidentIds|reason
// The file is append-only, so file order is time order. Pure text in, plain objects out.

export const ROW_VERSION = "V1";
export const ROW_FIELDS = 8;
const ACTIONS = new Set(["BAN", "UNBAN"]);

// One line as a row, or { error } with why it is skipped (the game skips the same rows).
export function parseBanLine(line) {
  const parts = line.split("|");
  if (parts[0] !== ROW_VERSION) return { error: `unknown version '${parts[0].slice(0, 12)}' (this reader knows ${ROW_VERSION})` };
  if (parts.length !== ROW_FIELDS) return { error: `${parts.length} fields, want ${ROW_FIELDS}` };
  if (!ACTIONS.has(parts[1])) return { error: `unknown action '${parts[1]}'` };
  if (!parts[2]) return { error: "empty target uid" };
  if (!/^-?\d+$/.test(parts[3])) return { error: `time '${parts[3]}' is not a number` };
  if (!/^-?\d+$/.test(parts[5])) return { error: `duration '${parts[5]}' is not a number` };
  const incidents = parts[6]
    .split(",")
    .map((s) => Number.parseInt(s, 10))
    .filter((n) => n > 0);
  return {
    row: {
      action: parts[1],
      uid: parts[2],
      time: Number(parts[3]),
      staff: parts[4],
      duration: Number(parts[5]),
      incidents,
      reason: parts[7],
    },
  };
}

// The whole file: { rows, skipped: [{ line, reason, text }] }. A row carries its 1-based `line`.
// Blank lines are not errors.
export function parseBanRecord(text) {
  const rows = [];
  const skipped = [];
  text.split(/\r?\n/).forEach((text, i) => {
    if (!text.trim()) return;
    const r = parseBanLine(text);
    if (r.error) skipped.push({ line: i + 1, reason: r.error, text });
    else rows.push({ ...r.row, line: i + 1 });
  });
  return { rows, skipped };
}

// One entry per row, in file order: { row, status, closedBy }.
//   BAN   -> "open", or "closed" with `closedBy` = the first later UNBAN of that uid
//   UNBAN -> "lifted"
// opts.player keeps one uid's rows (status is still worked out from all rows); opts.open keeps only
// the bans no later unban closed.
export function listBans(rows, opts = {}) {
  const entries = rows.map((row) => ({ row, status: row.action === "UNBAN" ? "lifted" : "open", closedBy: null }));
  const openByUid = new Map();
  for (const e of entries) {
    if (e.row.action === "BAN") {
      if (!openByUid.has(e.row.uid)) openByUid.set(e.row.uid, []);
      openByUid.get(e.row.uid).push(e);
    } else {
      for (const b of openByUid.get(e.row.uid) ?? []) {
        b.status = "closed";
        b.closedBy = e.row;
      }
      openByUid.delete(e.row.uid);
    }
  }
  return entries.filter((e) => (!opts.player || e.row.uid === opts.player) && (!opts.open || e.status === "open"));
}

export function formatDuration(seconds) {
  if (seconds <= 0) return "permanent";
  if (seconds % 86400 === 0) return `${seconds / 86400}d`;
  if (seconds % 3600 === 0) return `${seconds / 3600}h`;
  if (seconds % 60 === 0) return `${seconds / 60}m`;
  return `${seconds}s`;
}

export function formatTime(unix) {
  return new Date(unix * 1000).toISOString().replace("T", " ").replace(/\.\d+Z$/, "Z");
}

// One printed line for an entry.
export function formatEntry(e) {
  const r = e.row;
  const ids = r.incidents.length ? r.incidents.join(",") : "-";
  const reason = r.reason || "-";
  if (r.action === "UNBAN") {
    return `${formatTime(r.time)}  UNBAN  ${r.uid}  by ${r.staff}  incidents ${ids}  "${reason}"`;
  }
  const state = e.status === "closed" ? `closed ${formatTime(e.closedBy.time)} by ${e.closedBy.staff}` : "open";
  return `${formatTime(r.time)}  BAN    ${r.uid}  by ${r.staff}  ${formatDuration(r.duration)}  incidents ${ids}  [${state}]  "${reason}"`;
}