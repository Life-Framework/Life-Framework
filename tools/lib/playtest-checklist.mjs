// tools/lib/playtest-checklist.mjs - one playtest session that clears many needs-playtest issues (#402, #403).
//
// Pure logic, no gh and no file access: tools/commands/playtest-checklist.mjs feeds it issues and merged
// PRs and writes the result; tools/validation/playtest-checklist-selftest.mjs feeds it fixtures.
//
//   buildItems(issues, prs)    one item per open issue: its de-duplicated manual steps, tags, place, minutes
//   selectItems(items, opts)   the "top hour" cut (chapter work, then player flows, then the rest, polish last), or everything
//   renderChecklist(...)       markdown: smoke list, then solo walk by place, two players, restart, desk
//   parseVerdicts(text)        read the ticked checklist back: [{ id, verdict: pass|fail|open, note }]
//   verdictCommands(...)       the gh commands the maintainer would run (never run here)
import { manualSteps, parseRefs } from "./release-gates.mjs";

// ------------------------------------------------------------------ places

// The walking route through the playtest town, west to east then out to the edges (coordinates are the
// anchors of docs/playtest/verify-m2.md). `words` match the issue title and steps; the first place whose
// words match wins, so list the specific ones first. Coordinates in the text fall back to the nearest anchor.
export const PLACES = [
  { id: "spawn", name: "Spawn square (119, 243)", at: [119, 243], words: /spawn square|travel globe|town clerk|registration form|\bform r-\d+|\bid card|respawn|launcher|sign up for the town|arrival|\bsky\b|weather/i },
  { id: "farm", name: "Farm (148, 299)", at: [148, 299], words: /\bfarm\b|cabbage|plot\b|harvest/i },
  { id: "bank", name: "Bank and bank street (177, 258)", at: [177, 258], words: /\bbank\b|teller|vault|\batm\b|cash van|drill/i },
  { id: "store", name: "General store, market and car lot (179, 244)", at: [190, 245], words: /general store|shopkeeper|\bmarket\b|juic|\bregister\b|till counter|car lot|dealership|price (list|board)|chalkboard|post counter|courier|parcel/i },
  { id: "fuel", name: "Fuel station (270, 240)", at: [270, 240], words: /fuel|petrol|gas station|pump|kiosk|attendant/i },
  { id: "police", name: "Police station, jail and hospital (240, 242)", at: [245, 244], words: /police|\bjail\b|\bcell\b|armory|evidence|duty desk|dispatch|wanted|handcuff|arrest|officer|hospital|medic|revive|downed/i },
  { id: "registry", name: "Registry desk, hideout and barn (302, 288)", at: [300, 275], words: /registry|hideout|\bbarn\b|convoy|organi[sz]ation|\balley\b|back ?room|knock to buy/i },
  { id: "chop", name: "Chop shop and moonshine (322, 300)", at: [350, 300], words: /chop shop|lock-?up|moonshine|\bstill\b|barrel|lockpick|stolen car/i },
  { id: "quarry", name: "Quarry and mining (178, 168)", at: [178, 168], words: /quarry|mining|\bmine\b|pickaxe|smelt|\bore\b|nugget|lawless/i },
  { id: "annex", name: "Crime annex (30 to 48, 50)", at: [40, 50], words: /crime annex|safe door|rob the safe/i },
  { id: "anywhere", name: "Anywhere in town (inventory, HUD, vehicles, items)", at: null, words: /./ },
];

const PLACE_ORDER = PLACES.map((p) => p.id);
export const BLOCKS = ["solo", "2p", "restart", "desk"];
export const BLOCK_TITLES = {
  solo: "Solo walk, in route order",
  "2p": "Two players (second client on this machine or LAN)",
  restart: "Dedicated server: restart and late joiner (`tools\\cli host start`, stop, start with `-loadSessionSave`, rejoin)",
  desk: "Desk (no game needed)",
};
// Setup minutes paid once per block that has at least one item, and travel minutes per place visited.
export const BLOCK_SETUP_MIN = { solo: 0, "2p": 2, restart: 3, desk: 0 };
export const TRAVEL_MIN = 1;

function placeByCoords(text) {
  // "(177, 258)", "about 179, 244", "at 119, 243.5": x then z, both in the 0 to 1000 range of the town.
  const m = /\b(\d{2,3}(?:\.\d)?)\s*,\s*(\d{2,3}(?:\.\d)?)\b/.exec(text);
  if (!m) return null;
  const x = Number(m[1]);
  const z = Number(m[2]);
  let best = null;
  for (const p of PLACES) {
    if (!p.at) continue;
    const d = Math.hypot(p.at[0] - x, p.at[1] - z);
    if (d <= 60 && (!best || d < best.d)) best = { id: p.id, d };
  }
  return best?.id ?? null;
}

export function placeOf(title, stepsText) {
  const text = `${title}\n${stepsText}`;
  for (const p of PLACES) if (p.id !== "anywhere" && p.words.test(text)) return p.id;
  return placeByCoords(text) ?? "anywhere";
}

// ------------------------------------------------------------------ steps

const FOOTER_RE = /^(🤖|generated with|co-authored-by|refs?\s+#|closes\s+#|fixes\s+#|-{3,}$)/i;

// One manual-steps section into a list of single-line steps with the list markers, bold labels and footers gone.
export function splitSteps(section) {
  const out = [];
  for (const raw of String(section ?? "").replace(/\r\n/g, "\n").split("\n")) {
    let line = raw.trim();
    if (!line || FOOTER_RE.test(line)) continue;
    line = line.replace(/^(\d+[.)]|[-*])\s+/, "").replace(/\*\*/g, "").trim();
    if (!line || /^(steps?|expected|manual testing)\s*:?$/i.test(line)) continue;
    // "None: docs only", "Not run: ...", "Residual risks": notes about the PR, not things to do.
    if (/^(none\b|n\/a\b|not (?:run|exercised|tested)\b|residual\b|nothing\b)/i.test(line)) continue;
    if (FOOTER_RE.test(line)) continue;
    // A continuation (indented, no marker) joins the previous step.
    if (/^\s{2,}\S/.test(raw) && !/^\s*(\d+[.)]|[-*])\s/.test(raw) && out.length) out[out.length - 1] += " " + line;
    else out.push(line);
  }
  return out;
}

// The key two steps share when they say the same thing: words only, lower case, numbers and quoting dropped.
export function stepKey(step) {
  return step.toLowerCase().replace(/`[^`]*`/g, " ").replace(/#\d+/g, " ").replace(/[^a-z ]+/g, " ").replace(/\s+/g, " ").trim();
}

export const TAG_RE = {
  restart: /\brestart(?:s|ed)?\b|-loadSessionSave|stop and start the server|reload the save|\bsave and load\b|\bquit and rejoin|rejoin after/i,
  jip: /\bJIP\b|late join|join(?:s|ed)? (?:late|after|mid)|joins? while|third client|joins? a session already/i,
  mp: /two players|two clients|2 clients|second (?:client|player)|\bclient (?:1|2|a|b)\b|player (?:a|b)\b|\bfriend\b|another player|\bLAN\b/i,
  ded: /dedicated server|\bhost start\b|host (?:a |the )?dedicated/i,
};
// "No restart needed" and "a restart is not needed" are not restart steps.
const NEGATED_RESTART = /(?:no|not|without)\s+(?:a\s+)?restart(?:\s+(?:is\s+)?(?:needed|required))?|restart (?:is )?not (?:needed|required)|restart(?:s)? (?:isn't|is not) (?:needed|required)/gi;

export function tagsOf(text) {
  const clean = text.replace(NEGATED_RESTART, " ");
  return {
    restart: TAG_RE.restart.test(clean),
    jip: TAG_RE.jip.test(clean),
    ded: TAG_RE.ded.test(clean),
    mp: TAG_RE.mp.test(clean),
    look: /\[look\]|\blook(?:s)? (?:right|sane|correct|centred|centered)\b|clip(?:s|ping)?\b|visual/i.test(clean),
  };
}

const DESK_RE = /deploy(?:ed)? preview|life-framework\.org|website|\bdiscord\b|\bblender\b|meshy|elevenlabs|cgtrader|trailer|\bfilm\b|storyboard|outreach|\.md\b|workshop (?:upload|publish)|\bgh (?:issue|pr)\b/i;
const GAME_RE = /\bwalk|inventory|\bspawn|debugworld|playtest town|\bdrive\b|\bhost(?:ed)? (?:the |a )?(?:session|server|playtest)/i;

export function blockOf(item) {
  if (item.tags.restart || item.tags.jip || item.tags.ded) return "restart";
  if (item.tags.mp) return "2p";
  if (item.devTrack || (DESK_RE.test(`${item.title}\n${item.steps.join("\n")}`) && !GAME_RE.test(item.steps.join("\n")))) return "desk";
  return "solo";
}

// Minutes: a minute to get there and read, half a minute per unique step (to 6), plus the step kinds that cost more.
export function estimateMinutes(item) {
  const base = Math.min(6, 1 + Math.ceil(item.steps.length * 0.5));
  return base + (item.tags.mp ? 1 : 0) + (item.tags.restart ? 2 : 0) + (item.tags.jip ? 1 : 0);
}

// ------------------------------------------------------------------ collect

const MILESTONE_RANK = ["M1", "M2 ", "M2.5", "M3", "M4", "M5", "M6"];
export function milestoneRank(title) {
  if (!title) return 99;
  const t = `${title} `;
  const i = MILESTONE_RANK.findIndex((p) => t.startsWith(p));
  return i === -1 ? 90 : i;
}

// What a player meets first. A title or step that names one of these big flows ranks above the ordinary issue.
export const FLOW_RE = /\bvault\b|\brobber(?:y|ies)?\b|\brob (?:the |a )?(?:till|bank|store|kiosk|fuel)|fuel (?:station|till)|\bclerk\b|\bcuff|\barrest|\btaser?\b|\bmedic\b|\bdowned\b|\brevive|\bdispatch|\bpagers?\b|\bpayphones?\b|case board|\bevidence\b|\bwitness|wanted poster|\bdialogue\b|\bconversation\b|\bcatalogue\b|\blicen[cs]e/i;
// Polish and visual-only: a look at a prop, an icon, a label, a clipped line. Last in the cut unless a chapter owns it.
export const POLISH_RE = /^visual\b|\bclip(?:s|ped|ping)?\b|\brenders?\b|\bicons?\b|\boverlap|\bindicators?\b|\btypo\b|\bspanish\b|\btranslat|plain white|\bpolish\b|regroup|\bfloats?\b|\bflicker|\bjuice\b|too spread|\bmis-?aligned|\bz-?fight|\bsubtitle|\bfont\b|\bmaterials? quarry/i;
// Work on the tools and the proofs, not the game: no player meets it, so it goes with the polish.
export const TOOLING_RE = /\bproofs?\b|\bharness\b|benchmark|\bwt e2e\b|^dev\b|\bdev-video|\bdev epic|\bagent\b|\bfixtures?\b|selftest|\bvalidate|\bconfig reload|reload config|\bdiscord\b|\blanding page|\bhero demo|\bscreenshot and gif|\bmerge driver/i;
// M3 framework work (config, packs, specs, patterns) is for server owners, not the first hour of a player.
export const FRAMEWORK_RE = /^M[3-9](?:\.\d+)?-/;
const RECENT_DAYS = 14;
const DAY = 86400000;

// issues: [{ number, title, labels: [{name}|string], milestone: {title}|null }]; prs: merged [{ number, title, body, url, mergedAt }].
// chapters: Map<open chapter or epic number, [issue numbers under it]> (tools/commands/playtest-checklist.mjs reads it from GitHub).
// An issue keeps the manual steps of every merged PR that refs it. Steps another issue already shows are
// dropped from the later one (`sameAs` names the first); an issue whose steps are all covered stays in the list
// at zero extra time so it still gets a pass/fail box.
export function buildItems(issues, prs, { chapters = new Map(), now = Date.now() } = {}) {
  const byIssue = new Map();
  for (const pr of prs) {
    for (const n of new Set(parseRefs(pr.body))) {
      if (!byIssue.has(n)) byIssue.set(n, []);
      byIssue.get(n).push(pr);
    }
  }
  const items = [];
  for (const issue of issues) {
    const labels = (issue.labels ?? []).map((l) => (typeof l === "string" ? l : l.name));
    const myPrs = (byIssue.get(issue.number) ?? []).sort((a, b) => a.number - b.number);
    const steps = [];
    const seen = new Set();
    for (const pr of myPrs) {
      for (const s of splitSteps(manualSteps(pr.body))) {
        const k = stepKey(s);
        if (!k || seen.has(k)) continue;
        seen.add(k);
        steps.push(s);
      }
    }
    const text = steps.join("\n");
    const inChapters = [...chapters].filter(([epic, subs]) => epic === issue.number || subs.includes(issue.number)).map(([epic]) => epic);
    const merged = myPrs.map((p) => Date.parse(p.mergedAt ?? "")).filter((t) => Number.isFinite(t));
    const item = {
      id: String(issue.number),
      number: issue.number,
      title: issue.title,
      milestone: issue.milestone?.title ?? null,
      rank: milestoneRank(issue.milestone?.title),
      labels,
      devTrack: labels.includes("track:dev"),
      prs: myPrs.map((p) => p.number),
      steps,
      keys: [...seen],
      tags: tagsOf(text),
      noSteps: steps.length === 0,
      chapters: inChapters,
      isEpic: chapters.has(issue.number),
      mergedDaysAgo: merged.length ? Math.max(0, Math.floor((now - Math.max(...merged)) / DAY)) : null,
    };
    item.flowTitle = FLOW_RE.test(issue.title);
    item.flow = item.flowTitle || FLOW_RE.test(text);
    item.polish = POLISH_RE.test(issue.title) || TOOLING_RE.test(issue.title) || (item.tags.look && !item.flowTitle);
    item.tier = tierOf(item);
    item.place = placeOf(issue.title, text);
    item.block = blockOf(item);
    item.est = item.noSteps ? 1 : estimateMinutes(item);
    items.push(item);
  }
  return items;
}

// 0 an open chapter or epic, or an issue under one: what closes an M2 chapter. 1 a big player flow named in the title, or one named in
// the steps of a feature merged in the last two weeks (title flows first). 2 the rest. 3 polish and visual-only (and dev tooling, which no player meets).
export function tierOf(item) {
  if (item.chapters.length) return 0;
  if (item.devTrack || item.polish) return 3;
  if (FRAMEWORK_RE.test(item.title)) return 2;
  if (item.flowTitle) return 1;
  if (item.flow && item.mergedDaysAgo !== null && item.mergedDaysAgo <= RECENT_DAYS) return 1;
  return 2;
}

// Order for the cut: tier, then milestone rank, then issues with steps before ones with none, then (chapter work and flows)
// newest merge first by the week, then cheap before dear (more issues per minute), then issue number.
export function rankItems(items) {
  // Milestone breaks ties in every tier but the player flows: a flow the player meets first is not made to wait for its milestone.
  const rk = (i) => (i.tier === 1 ? 0 : i.rank);
  const age = (i) => (i.tier <= 1 ? Math.floor((i.mergedDaysAgo ?? 999) / 7) : 0);
  return [...items].sort((a, b) => a.tier - b.tier || rk(a) - rk(b) || Number(!a.isEpic) - Number(!b.isEpic) || Number(!a.flowTitle) - Number(!b.flowTitle) || Number(a.noSteps) - Number(b.noSteps) || age(a) - age(b) || a.est - b.est || a.number - b.number);
}

// Assign steps to items in rank order: an item shows only the steps no earlier chosen item shows; an item whose steps are
// all shown elsewhere stays at zero minutes and says `same steps as #N`. `owner` maps a step key to the issue showing it.
function place(item, owner) {
  const own = [];
  const covered = new Set();
  item.steps.forEach((step, i) => {
    const k = item.keys[i];
    if (owner.has(k)) covered.add(owner.get(k));
    else own.push({ step, k });
  });
  const shown = own.map((o) => o.step);
  return {
    shown,
    keys: own.map((o) => o.k),
    sameAs: [...covered].filter((n) => n !== item.number),
    minutes: item.noSteps ? 1 : shown.length ? estimateMinutes({ ...item, steps: shown }) : 0,
  };
}

// How much of the hour each tier may fill: chapter work takes at most 70%, so the player flows still get their place; polish and
// the rest only fill what is left over. A tier takes whatever the earlier ones left unused.
export const TIER_SHARE = [0.7, 1, 1, 1];

// The "top hour": walk the ranked list and take what fits in the budget (item minutes plus the setup and travel they cause).
// `reserve` is minutes already spoken for (the smoke list). A fully de-duplicated item costs nothing, so it always rides along.
// A player flow that does not fit closes the door on the tiers below it: leftover minutes are not spent on filler while a
// big flow waits (it leads the next pass). `all` takes everything.
export function selectItems(items, { all = false, budget = 60, reserve = 0 } = {}) {
  const owner = new Map();
  const chosen = [];
  const skipped = [];
  let closedBelow = 3;
  for (const item of rankItems(items)) {
    const p = place(item, owner);
    const trial = { ...item, shownSteps: p.shown, sameAs: p.sameAs, minutes: p.minutes };
    const fits = totalMinutes([...chosen, trial]) + reserve <= budget * TIER_SHARE[item.tier];
    if (all || p.minutes === 0 || (item.tier <= closedBelow && fits)) {
      for (const k of p.keys) owner.set(k, item.number);
      chosen.push(trial);
    } else {
      skipped.push(item);
      if (item.tier === 1 && item.tier <= closedBelow && !fits) closedBelow = 1;
    }
  }
  return { chosen, skipped };
}

export function totalMinutes(chosen) {
  let total = 0;
  const blocksUsed = new Set();
  const places = new Set();
  for (const c of chosen) {
    total += c.minutes;
    blocksUsed.add(c.block);
    if (c.block === "solo") places.add(c.place);
  }
  for (const b of blocksUsed) total += BLOCK_SETUP_MIN[b];
  total += places.size * TRAVEL_MIN;
  return total;
}

// ------------------------------------------------------------------ render

const oneLine = (s, n) => {
  const t = s.replace(/\s+/g, " ").replace(/`/g, "").trim();
  return t.length > n ? `${t.slice(0, n - 3).trimEnd()}...` : t;
};

export function tagLabels(item) {
  const t = [];
  if (item.tags.mp) t.push("2P");
  if (item.tags.restart) t.push("RESTART");
  if (item.tags.jip) t.push("JIP");
  if (item.tags.ded && !item.tags.restart && !item.tags.jip) t.push("DEDICATED");
  if (item.tags.look) t.push("LOOK");
  return t;
}

// `- [ ] #123 Title (~3 min) [2P] [RESTART]   fail [ ]` then the steps. Pass = the first box, fail = the last.
export const SHOWN_STEPS = 4;
// One walk can prove several issues: an issue whose steps another chosen issue already shows, in the same place and block,
// rides on that issue's line (`also`), so the line lists every number and its pass or fail covers all of them.
export function foldCovered(chosen) {
  const byNumber = new Map(chosen.map((c) => [c.number, c]));
  const folded = new Set();
  const also = new Map();
  for (const c of chosen) {
    if (c.noSteps || c.minutes !== 0 || c.sameAs?.length !== 1) continue;
    const owner = byNumber.get(c.sameAs[0]);
    if (!owner || owner.block !== c.block || owner.place !== c.place || folded.has(owner.number)) continue;
    folded.add(c.number);
    if (!also.has(owner.number)) also.set(owner.number, []);
    also.get(owner.number).push(c);
  }
  return chosen.filter((c) => !folded.has(c.number)).map((c) => (also.has(c.number) ? { ...c, also: also.get(c.number) } : c));
}

export function itemLines(item, { steps = false } = {}) {
  const tags = tagLabels(item).map((t) => ` [${t}]`).join("");
  const min = item.minutes ? ` (~${item.minutes} min)` : "";
  const ids = [item.number, ...(item.also ?? []).map((a) => a.number)].map((n) => "#" + n).join(" ");
  const ch = item.chapters?.length ? ` [chapter ${item.chapters.map((n) => "#" + n).join(" ")}]` : "";
  const lines = [`- [ ] ${ids} ${oneLine(item.title, 110)}${min}${tags}${ch}   fail [ ]`];
  for (const a of item.also ?? []) lines.push(`  - also proves #${a.number}: ${oneLine(a.title, 110)}`);
  if (item.sameAs?.length) lines.push(`  - same steps as ${item.sameAs.map((n) => "#" + n).join(", ")}`);
  const shown = item.shownSteps ?? item.steps;
  if (item.noSteps) lines.push(`  - no manual steps in ${item.prs.length ? "PR " + item.prs.map((n) => "#" + n).join(", ") : "any merged PR"}: confirm it in play, or tick on the PR evidence`);
  else if (steps) for (const s of shown) lines.push(`  - ${oneLine(s, 400)}`);
  else {
    for (const s of shown.slice(0, SHOWN_STEPS)) lines.push(`  - ${oneLine(s, 220)}`);
    if (shown.length > SHOWN_STEPS) lines.push(`  - (+${shown.length - SHOWN_STEPS} more steps: ${item.prs.map((n) => "PR #" + n).join(", ")}, or \`--steps\`)`);
  }
  return lines;
}

// smoke: [{ id: "S1", text }] from docs/playtest/smoke-list.md.
export function renderChecklist({ chosen: chosenAll, skipped = [], smoke = [], steps = false, all = false, budget = 60, sha = "", date = "", openCount = null }) {
  const out = [];
  const chosen = foldCovered(chosenAll);
  const total = totalMinutes(chosen) + smoke.reduce((n, s) => n + (s.minutes ?? 1), 0);
  out.push(`# Playtest checklist${all ? " (everything)" : ` (top ${budget} minutes)`}${date ? `: ${date}` : ""}`);
  out.push("");
  out.push(
    `${chosenAll.length} issue(s) on ${chosen.length} line(s), about ${total} min` +
      (smoke.length ? ` including the ${smoke.length} smoke checks` : "") +
      (skipped.length ? `; ${skipped.length} more open issue(s) wait for the next pass (\`--all\` lists them)` : "") +
      (openCount !== null ? `; ${openCount} open in-dev or needs-playtest in all` : "") +
      (sha ? `; main at ${sha}` : "") +
      ".",
  );
  out.push("");
  out.push("Tick `[x]` at the front when the check passes; tick `fail [x]` at the end when it does not, and put the reason on a `> note:` line under it.");
  out.push("Leave a box empty to skip: nothing happens to that issue. Then `tools\\cli playtest verdicts <this file>` prints the `gh` commands for review; it does not run them.");
  out.push("Tags: `[2P]` needs a second client, `[DEDICATED]` needs the dedicated server, `[RESTART]` needs a server restart, `[JIP]` needs a late joiner, `[LOOK]` is a visual only a person can judge.");
  out.push("");
  if (smoke.length) {
    out.push("## Smoke list (every promotion, first; a fail here blocks promotion)");
    out.push("");
    for (const s of smoke) out.push(`- [ ] ${s.id} ${s.text}   fail [ ]`);
    out.push("");
  }
  for (const block of BLOCKS) {
    const inBlock = chosen.filter((c) => c.block === block);
    if (!inBlock.length) continue;
    const min = inBlock.reduce((n, c) => n + c.minutes, 0) + BLOCK_SETUP_MIN[block];
    if (block === "solo") {
      out.push(`## ${BLOCK_TITLES.solo}`);
      out.push("");
      for (const place of PLACE_ORDER) {
        const here = inBlock.filter((c) => c.place === place);
        if (!here.length) continue;
        const pmin = here.reduce((n, c) => n + c.minutes, 0) + TRAVEL_MIN;
        out.push(`### ${PLACES.find((p) => p.id === place).name} (~${pmin} min)`);
        out.push("");
        for (const c of here) out.push(...itemLines(c, { steps }));
        out.push("");
      }
    } else {
      out.push(`## ${BLOCK_TITLES[block]} (~${min} min)`);
      out.push("");
      for (const place of PLACE_ORDER) for (const c of inBlock.filter((x) => x.place === place)) out.push(...itemLines(c, { steps }));
      out.push("");
    }
  }
  return out.join("\n").replace(/\n{3,}/g, "\n\n").trimEnd() + "\n";
}

// ------------------------------------------------------------------ smoke list

// docs/playtest/smoke-list.md: lines "- S1 (2 min): text". Returns [{ id, text, minutes }].
export function parseSmokeList(md) {
  const out = [];
  for (const m of String(md ?? "").matchAll(/^[-*]\s+(S\d+)(?:\s*\((\d+)\s*min\))?\s*:\s*(.+)$/gm)) {
    out.push({ id: m[1], text: m[3].trim(), minutes: m[2] ? Number(m[2]) : 1 });
  }
  return out;
}

// ------------------------------------------------------------------ verdicts

// Reads a ticked checklist. A line `- [x] #12 ... fail [ ]` passes #12 (`- [x] #12 #13 ...` passes both); `fail [x]` fails it whatever the first box says;
// both empty is "open". An indented `> note: text` line under an item is its comment. Smoke ids (S1) count too.
export function parseVerdicts(text) {
  const out = [];
  let cur = null;
  for (const raw of String(text ?? "").replace(/\r\n/g, "\n").split("\n")) {
    const m = /^\s*[-*]\s+\[([^\]]?)\]\s+((?:#\d+\s+)*#\d+|S\d+)\b(.*)$/.exec(raw);
    if (m) {
      const front = m[1].trim().toLowerCase();
      const fail = /\bfail\s*\[\s*([^\]]?)\s*\]\s*$/i.exec(m[3]);
      const failTicked = !!fail && fail[1].trim() !== "";
      const passTicked = front !== "" && front !== " ";
      cur = m[2].split(/\s+/).map((id) => ({ id: id.startsWith("#") ? id.slice(1) : id, smoke: id.startsWith("S"), verdict: failTicked ? "fail" : passTicked ? "pass" : "open", note: "" }));
      out.push(...cur);
      continue;
    }
    const n = /^\s*>\s*note:\s*(.*)$/i.exec(raw);
    if (n && cur) for (const c of cur) c.note = c.note ? `${c.note} ${n[1].trim()}` : n[1].trim();
    else if (/^\s*#/.test(raw)) cur = null;
  }
  return out;
}

const shq = (s) => `"${String(s).replace(/(["\\$`])/g, "\\$1")}"`;

// The gh commands a human would run. Pass: verified label on, in-dev/needs-playtest off, comment, close (only `verified`
// closes an issue). Fail: regression on, the others off, comment, stays open. Smoke fails block promotion and are listed.
export function verdictCommands(verdicts, { by = "the maintainer", date = "", sha = "", repo = "" } = {}) {
  const r = repo ? ` --repo ${repo}` : "";
  const tail = [date && `on ${date}`, sha && `at main ${sha}`].filter(Boolean).join(" ");
  const cmds = [];
  const notes = [];
  for (const v of verdicts) {
    if (v.verdict === "open") continue;
    if (v.smoke) {
      if (v.verdict === "fail") notes.push(`SMOKE ${v.id} FAILED${v.note ? `: ${v.note}` : ""}: do not promote until it passes`);
      continue;
    }
    const n = v.id;
    if (v.verdict === "pass") {
      const body = `Verified by ${by}${tail ? " " + tail : ""} in a playtest checklist pass.${v.note ? " " + v.note : ""}`;
      cmds.push(`gh issue edit ${n}${r} --add-label verified --remove-label needs-playtest --remove-label in-dev`);
      cmds.push(`gh issue comment ${n}${r} --body ${shq(body)}`);
      cmds.push(`gh issue close ${n}${r} --reason completed`);
    } else {
      const body = `Failed verification by ${by}${tail ? " " + tail : ""}.${v.note ? " " + v.note : " No reason written on the checklist."}`;
      cmds.push(`gh issue edit ${n}${r} --add-label regression --remove-label needs-playtest --remove-label in-dev --remove-label verified`);
      cmds.push(`gh issue comment ${n}${r} --body ${shq(body)}`);
    }
  }
  return { cmds, notes };
}
