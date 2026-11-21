// Is this open issue already done under another number? (efficiency pass 3, 2026-11-18)
//
// On 2026-11-18 the lead dispatched about nine agents to open issues that merged work had already
// fixed: #2493 (by #2531/#2514), #640 (#2068, #2078), #1068 (#2438), #1756 (#1959), #2087 (#2110,
// #2151), #2462 (#2498), #1754 (mostly done), #2494 and #2418 (partly). Each agent found it in 1-6
// minutes, but each cost a brief, a worktree, a lane and a lead check-in. `tools\cli issue-check
// <N...>` asks GitHub and git first and prints one verdict per issue:
//   LIKELY DONE  the issue carries a merged-work label, a merged PR is FOR #N (its title ends in
//                "(#N)" or its body has "Refs #N" / "Closes #N" / "Fixes #N" as its own line), a main
//                commit subject ends in "(#N)", a comment says it is fixed or a duplicate, or the skip
//                list names it
//   CHECK        a PR or commit that merely mentions #N (a design note, a sibling card, "can reuse #N",
//                "builds on #N": 2026-11-19 it called #3260, #3263, #3208, #3274 and #3287 LIKELY DONE
//                for that), merged PRs with the same title words, or sub-issues it names that are done
//   CLEAR        nothing found
// The gathering is in tools/commands/issue-check.mjs; this file is pure so the selftest needs no gh.

export const DONE_LABELS = ["in-dev", "needs-playtest", "verified"];

// A PR that only plans or lists work (roadmap, outline) cites issues without doing them; a parent
// chapter or epic is closed or labelled long before its last card. Neither says the issue is done.
export const PLANNING_PR = /^(roadmap|m[0-9]+ (outline|plan|roadmap)|planner|slice plan)\b|\boutline\b/i;
export const PARENT_ISSUE = /\b(epic|chapter|milestone|outline)\b/i;

const STOP = new Set(
  "the and for with from that this when into onto than then they them their there what which while where after before about over under only also each every have has had does doing done not but its it's are was were will would should could must can cannot a an of to in on at by or as is be no yes one two new old own more less most some any all per via one's".split(" "),
);

// The 3-4 most distinctive words of a title, for a GitHub search: no area prefix punctuation,
// no stop words, no numbers, longest first.
export function keywords(title, max = 4) {
  const words = String(title || "")
    .toLowerCase()
    .replace(/\(.*?\)/g, " ")
    .split(/[^a-z0-9'-]+/)
    .map((w) => w.replace(/^'+|'+$/g, ""))
    .filter((w) => w.length >= 4 && !STOP.has(w) && !/^[0-9]+$/.test(w));
  const seen = [];
  for (const w of words) if (!seen.includes(w)) seen.push(w);
  return seen.sort((a, b) => b.length - a.length).slice(0, max);
}

export const citesIssue = (text, n) => new RegExp(`#${n}(?![0-9])`).test(String(text || ""));

// A PR or commit is FOR #N, not just about it: the title (subject) ends in "(#N)" (also "(#M, #N)"), or the
// body has a closing keyword line: "Refs #N", "Closes #N", "Fixes #N" (also "Refs #M, #N").
export function titleIsFor(title, n) {
  return new RegExp(`\\((?:#[0-9]+,\\s*)*#${n}(?:,\\s*#[0-9]+)*\\)\\s*$`).test(String(title || "").trim());
}
export function bodyIsFor(body, n) {
  const list = `(?:#[0-9]+\\s*(?:,|and|&)\\s*)*#${n}(?![0-9])(?:\\s*(?:,|and|&)\\s*#[0-9]+)*`;
  const re = new RegExp(`^\\s*(?:refs?|closes?|fix(?:es)?|resolves?)\\s*:?\\s+${list}\\s*$`, "i");
  return String(body || "").split("\n").some((l) => re.test(l.replace(/\r$/, "")));
}
export const prIsFor = (p, n) => titleIsFor(p.title, n) || bodyIsFor(p.body, n);

// Issue numbers a body names (sub-issue lists, "after #X", "see #Y"), without the issue itself.
export function namedIssues(body, self) {
  const out = [];
  for (const m of String(body || "").matchAll(/#([0-9]{2,5})(?![0-9])/g)) {
    const n = Number(m[1]);
    if (n !== self && !out.includes(n)) out.push(n);
  }
  return out;
}

const FIXED_WORDS = /\b(already (fixed|done|implemented|on main|landed|shipped)|fixed (by|in) #[0-9]+|duplicate of #[0-9]+|superseded by #[0-9]+|done (by|in) #[0-9]+)/i;

// Skip-list text (the lead's stale list): every issue number on a line that says ALREADY-FIXED or DONE.
export function skipListNumbers(text) {
  const out = new Set();
  for (const line of String(text || "").split("\n")) {
    if (!/ALREADY-FIXED|ALREADY DONE|DO NOT DISPATCH/i.test(line)) continue;
    // The numbers right after the colon, up to the first word: "ALREADY-FIXED (date): 640 rejoin (PRs 2068)" is 640.
    const body = line.replace(/\(.*?\)/g, " ").split(":").slice(1).join(":") || "";
    for (const tok of body.trim().split(/[\s,]+/)) {
      if (!/^#?[0-9]{2,5}$/.test(tok)) break;
      out.add(Number(tok.replace("#", "")));
    }
  }
  return out;
}

// facts: { issue: {number,title,labels,state,comments:[{body}]}, prs: [{number,title,body,mergedAt}],
//          commits: [{sha,subject}], similar: [{number,title,mergedAt}], named: [{number,title,state,labels}],
//          skipList: Set<number> }
export function verdict(facts) {
  const n = facts.issue.number;
  const labels = (facts.issue.labels || []).map((l) => (typeof l === "string" ? l : l.name));
  const strong = [];
  const weak = [];
  if (String(facts.issue.state || "").toLowerCase() === "closed") strong.push("the issue is closed");
  for (const l of labels) if (DONE_LABELS.includes(l)) strong.push(`label ${l}: a merged PR already refs it`);
  for (const p of facts.prs || []) {
    if (!citesIssue(`${p.title}\n${p.body}`, n)) continue;
    if (prIsFor(p, n) && !PLANNING_PR.test(p.title)) strong.push(`merged PR #${p.number} is for #${n}: ${p.title}`);
    else weak.push(`merged PR #${p.number} only mentions #${n} (read it: a design note or a sibling card?): ${p.title}`);
  }
  for (const c of facts.commits || []) {
    if (titleIsFor(c.subject, n)) strong.push(`main commit ${c.sha.slice(0, 9)} is for #${n}: ${c.subject}`);
    else weak.push(`main commit ${c.sha.slice(0, 9)} only mentions #${n}: ${c.subject}`);
  }
  for (const c of facts.issue.comments || []) {
    const m = FIXED_WORDS.exec(c.body || "");
    if (m) strong.push(`a comment says "${m[0]}"`);
  }
  if (facts.skipList?.has(n)) strong.push("the stale-issue skip list names it");
  const citing = new Set((facts.prs || []).filter((p) => citesIssue(`${p.title}\n${p.body}`, n)).map((p) => p.number));
  for (const p of facts.similar || []) if (!citing.has(p.number)) weak.push(`similar merged PR #${p.number}${p.mergedAt ? ` (${p.mergedAt.slice(0, 10)})` : ""}: ${p.title}`);
  for (const i of facts.named || []) {
    if (PARENT_ISSUE.test(i.title || "")) continue;
    const ls = (i.labels || []).map((l) => (typeof l === "string" ? l : l.name));
    const done = String(i.state || "").toLowerCase() === "closed" || ls.some((l) => DONE_LABELS.includes(l));
    if (done) weak.push(`named issue #${i.number} is done (${String(i.state).toLowerCase() === "closed" ? "closed" : ls.filter((l) => DONE_LABELS.includes(l)).join(",")}): ${i.title}`);
  }
  const level = strong.length ? "LIKELY DONE" : weak.length ? "CHECK" : "CLEAR";
  return { level, reasons: [...strong, ...weak] };
}

export function render(n, title, v) {
  const head = `#${n} ${v.level}: ${title}`;
  if (v.level === "CLEAR") return `${head}\n  nothing merged cites it or matches its title; safe to dispatch`;
  const tail =
    v.level === "LIKELY DONE"
      ? "  -> read the citing PRs before dispatching; if the issue is covered, add it to the stale list instead"
      : "  -> skim these first; dispatch with 'check whether <PR> already covers this' in the brief if unsure";
  return [head, ...v.reasons.slice(0, 8).map((r) => `  - ${r}`), tail].join("\n");
}
