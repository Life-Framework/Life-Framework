import { useEffect, useRef, useState, type PointerEvent as ReactPointerEvent } from "react";
import { DISCORD_URL, REPO_URL, formatDate, githubData, issueStatus, issueTopic, splitTitle, type Issue, type Milestone } from "@/lib/content";

// Milestone and epic markers are shown as badges, so drop them from the display title.
function titleOf(issue: Issue) {
  const { code, title } = splitTitle(issue.title);
  const t = title.replace(/^M\d+\s+(?:epic|flagship):\s*/i, "").replace(/\s*\(M\d+\)$/, "");
  return { code, title: t.charAt(0).toUpperCase() + t.slice(1) };
}

/*
 * The Tree of Life: GitHub issues drawn as a tree.
 * Root -> pillar (topic: label) -> system (Parent: line) -> issue.
 * Each pillar is a vertical spine; each system on it grows a chain of issues to one side.
 */

type Filter = "all" | "open" | "in-progress" | "done";
type Item = { issue: Issue; milestone: Milestone };
type System = { name: string; items: Item[] };
type Pillar = { name: string; items: Item[]; systems: System[] };
type Selection = { pillar: string; system: string | null; issue: number | null } | null;

type IssuePos = { item: Item; x: number; y: number };
type SystemPos = { system: System; x: number; y: number; side: 1 | -1; issues: IssuePos[] };
type PillarPos = { pillar: Pillar; x: number; y: number; spineEnd: number; systems: SystemPos[] };

const ROOT_Y = 70;
const PILLAR_Y = 270;
const FIRST_SYSTEM_GAP = 130;
const SYSTEM_GAP = 120;
const ISSUE_DX = 150;
const ISSUE_DY = 54;
const ISSUE_STEP = 92;
const LABEL_HALF = 80;
const PILLAR_LABEL = 210;
const PILLAR_GAP = 40;
const MARGIN = 60;
const MIN_ZOOM = 0.3;
const MAX_ZOOM = 1.5;
const DEFAULT_ZOOM = 0.8;
const PICK_LIMIT = 6;

const PILLAR_ORDER = [
  "Law & Order",
  "Economy & Work",
  "Life & Identity",
  "Community & Organizations",
  "World & Travel",
  "Framework & Playtest",
  "Flagship Story",
  "Unsorted",
  "Multiple topics",
];

const FILTERS: { id: Filter; label: string }[] = [
  { id: "open", label: "Needs a hand" },
  { id: "in-progress", label: "In progress" },
  { id: "done", label: "Done" },
  { id: "all", label: "Everything" },
];

const ICONS: Record<string, string> = {
  root: "M12 21v-7M12 14c-4 0-7-2.5-7-6 2.5 0 5 1 7 3.5M12 14c4 0 7-2.5 7-6-2.5 0-5 1-7 3.5M12 11.5C12 8 10 5 12 3c2 2 0 5 0 8.5",
  "Law & Order": "M12 3l7 3v5c0 4.2-3 7.3-7 9-4-1.7-7-4.8-7-9V6l7-3zM9.5 11.5l1.7 1.7 3.6-4",
  "Economy & Work": "M12 3a9 9 0 100 18 9 9 0 000-18zM14.5 9.5c-.4-.9-1.3-1.5-2.5-1.5-1.4 0-2.5.7-2.5 1.8 0 2.7 5 1.3 5 4 0 1.1-1.1 1.9-2.5 1.9-1.2 0-2.1-.6-2.5-1.5M12 6.5V8m0 8v1.5",
  "Life & Identity": "M12 20s-7-4.4-7-9a4 4 0 017-2.6A4 4 0 0119 11c0 4.6-7 9-7 9z",
  "Community & Organizations": "M8.5 11a3 3 0 100-6 3 3 0 000 6zm7 0a3 3 0 100-6 3 3 0 000 6zM3 20c0-3 2.3-5 5.5-5s5.5 2 5.5 5m1-5c2.8 0 5 2 5 5",
  "World & Travel": "M12 3a9 9 0 100 18 9 9 0 000-18zm0 0c3.2 3 3.2 15 0 18m0-18c-3.2 3-3.2 15 0 18M3.5 12h17",
  "Framework & Playtest": "M14.7 6.3a4 4 0 00-5.4 5.4L3 18l3 3 6.3-6.3a4 4 0 005.4-5.4l-2.5 2.5-2.4-.6-.6-2.4 2.5-2.5z",
  "Flagship Story": "M12 3l2.6 5.6 6.1.8-4.5 4.2 1.2 6-5.4-3-5.4 3 1.2-6-4.5-4.2 6.1-.8L12 3z",
  "Town Foundation": "M4 11l8-7 8 7M6 9.5V20h12V9.5M10 20v-5h4v5",
  "Test Harness": "M9 3h6M10 3v6l-5 9a2 2 0 002 3h10a2 2 0 002-3l-5-9V3M7.5 15h9",
  "Money & Property": "M8 14a5 5 0 110-10 5 5 0 010 10zM11.5 11.5L20 20M16 16l2-2M18.5 18.5l2-2",
  "Identity & Arrival": "M3 6h18v12H3zM7 10.5a1.5 1.5 0 103 0 1.5 1.5 0 00-3 0M5.5 15.5c.4-1.4 1.6-2 3-2s2.6.6 3 2M14 10h4M14 13h3",
  "Police & Response": "M7 18v-6a5 5 0 0110 0v6M5 18h14v3H5zM12 3v2M4.2 6.2l1.4 1.4M19.8 6.2l-1.4 1.4",
  "Social Interaction": "M4 5h16v10H9l-5 4V5z",
  "Crime & Contraband": "M3 7l9-4 9 4v10l-9 4-9-4V7zM3 7l9 4 9-4M12 11v10",
  Organizations: "M5 21V4M5 4h11l-2 4 2 4H5",
  "Justice & Consequences": "M12 3v18M7 21h10M5 7h14M5 7l-3 6a3 3 0 006 0L5 7zm14 0l-3 6a3 3 0 006 0l-3-6z",
  "Release Proof": "M9 12l2 2 4-4M12 3a9 9 0 100 18 9 9 0 000-18z",
  "Health & Survival": "M9 3h6v6h6v6h-6v6H9v-6H3V9h6z",
  Moderation: "M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12zm10-3a3 3 0 100 6 3 3 0 000-6z",
  Communication: "M8 3h8v18H8zM11 18h2",
  Travel: "M5 11l2-5h10l2 5M4 11h16v6H4zM7 17v2M17 17v2",
  "Town Services": "M3 21h18M5 21V10l7-5 7 5v11M9 21v-6h6v6",
  Story: "M4 5a2 2 0 012-2h13v16H6a2 2 0 00-2 2V5zM4 19a2 2 0 012-2h13",
  system: "M12 8a4 4 0 100 8 4 4 0 000-8zm0-5v2m0 14v2M3 12h2m14 0h2",
  done: "M5 12.5l4.5 4.5L19 7.5",
  "in-progress": "M12 7v5l3 2M12 3a9 9 0 100 18 9 9 0 000-18z",
  open: "M12 6v12M6 12h12",
};

function Icon({ name, fallback = "system" }: { name: string; fallback?: string }) {
  return (
    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
      <path d={ICONS[name] ?? ICONS[fallback]} />
    </svg>
  );
}

function stateOf(issue: Issue): Exclude<Filter, "all"> {
  if (issue.state === "closed") return "done";
  return issue.assignees.length ? "in-progress" : "open";
}

function matches(issue: Issue, filter: Filter) {
  return filter === "all" || stateOf(issue) === filter;
}

function collectPillars(): Pillar[] {
  const pillars = new Map<string, Item[]>();
  for (const milestone of githubData.milestones) {
    for (const issue of milestone.issues) {
      const name = issueTopic(issue);
      if (!pillars.has(name)) pillars.set(name, []);
      pillars.get(name)!.push({ issue, milestone });
    }
  }
  const rank = (name: string) => {
    const i = PILLAR_ORDER.indexOf(name);
    return i < 0 ? PILLAR_ORDER.length : i;
  };
  return Array.from(pillars, ([name, items]) => {
    const systems = new Map<string, Item[]>();
    for (const item of items) {
      const system = item.issue.parent?.trim() || "General";
      if (!systems.has(system)) systems.set(system, []);
      systems.get(system)!.push(item);
    }
    const order = (a: Item, b: Item) => a.milestone.number - b.milestone.number || a.issue.number - b.issue.number;
    return {
      name,
      items,
      systems: Array.from(systems, ([system, list]) => ({ name: system, items: list.sort(order) })).sort(
        (a, b) => order(a.items[0], b.items[0]),
      ),
    };
  }).sort((a, b) => rank(a.name) - rank(b.name) || a.name.localeCompare(b.name));
}

/* Lay the tree out once; filters dim nodes instead of moving them, so the map stays stable. */
function layout(pillars: Pillar[]) {
  let cursor = MARGIN;
  let bottom = 0;
  const placed: PillarPos[] = pillars.map((pillar) => {
    const hasLeft = pillar.systems.length > 1;
    const left = hasLeft ? ISSUE_DX + LABEL_HALF : LABEL_HALF;
    const right = Math.max(ISSUE_DX + LABEL_HALF, PILLAR_LABEL);
    const x = cursor + left;
    cursor = x + right + PILLAR_GAP;

    const sideBottom = { 1: 0, [-1]: 0 } as Record<1 | -1, number>;
    let y = PILLAR_Y + FIRST_SYSTEM_GAP - SYSTEM_GAP;
    const systems: SystemPos[] = pillar.systems.map((system, index) => {
      const side: 1 | -1 = index % 2 === 0 ? 1 : -1;
      y = Math.max(y + SYSTEM_GAP, sideBottom[side] - ISSUE_DY);
      const issues = system.items.map((item, i) => ({ item, x: x + side * ISSUE_DX, y: y + ISSUE_DY + i * ISSUE_STEP }));
      sideBottom[side] = issues[issues.length - 1].y + ISSUE_STEP + 30;
      return { system, x, y, side, issues };
    });
    const last = systems[systems.length - 1];
    const spineEnd = (last?.y ?? PILLAR_Y) + 70;
    bottom = Math.max(bottom, spineEnd, sideBottom[1], sideBottom[-1]);
    return { pillar, x, y: PILLAR_Y, spineEnd, systems };
  });
  const width = Math.max(cursor - PILLAR_GAP + MARGIN, 800);
  return { placed, width, height: bottom + 40, rootX: width / 2 };
}

const clampZoom = (value: number) => Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, Math.round(value * 100) / 100));
const plural = (n: number, word: string) => `${n} ${word}${n === 1 ? "" : "s"}`;

function Progress({ items }: { items: Item[] }) {
  const done = items.filter((item) => item.issue.state === "closed").length;
  const open = items.filter((item) => stateOf(item.issue) === "open").length;
  return (
    <div className="tree-menu__progress">
      <div className="tree-menu__bar" aria-hidden="true">
        <span style={{ width: `${(done / Math.max(1, items.length)) * 100}%` }} />
      </div>
      <p>
        {done}/{items.length} done{open > 0 && <strong> · {open} need a hand</strong>}
      </p>
    </div>
  );
}

function IssueRow({ item, onPick }: { item: Item; onPick: (item: Item) => void }) {
  const state = stateOf(item.issue);
  const { title } = titleOf(item.issue);
  return (
    <li>
      <button className={`tree-menu__row tree-menu__row--${state}`} type="button" onClick={() => onPick(item)}>
        <span className="tree-menu__row-dot" aria-hidden="true">
          <Icon name={state} />
        </span>
        <span className="tree-menu__row-title">{title}</span>
        <span className="tree-menu__row-ms">{item.milestone.id}</span>
      </button>
    </li>
  );
}

// An open issue whose dependencies are all done (or outside the roadmap) can start today.
function ready(item: Item, byNumber: Map<number, Item>) {
  return (item.issue.dependsOn ?? []).every((n) => byNumber.get(n)?.issue.state !== "open");
}

function StartHere({ items, byNumber, onPick }: { items: Item[]; byNumber: Map<number, Item>; onPick: (item: Item) => void }) {
  const open = items.filter((i) => stateOf(i.issue) === "open");
  const pick = open.find((i) => ready(i, byNumber)) ?? open[0];
  if (!pick) return null;
  const { title } = titleOf(pick.issue);
  return (
    <button type="button" className="tree-menu__start" onClick={() => onPick(pick)}>
      <span className="tree-menu__label">Start here</span>
      <span className="tree-menu__start-title">{title}</span>
      <span className="tree-menu__start-meta">
        {pick.milestone.id} · {ready(pick, byNumber) ? "nothing blocking it" : "waits on other work"} →
      </span>
    </button>
  );
}

function Related({ label, numbers, byNumber, onPick }: { label: string; numbers: number[]; byNumber: Map<number, Item>; onPick: (item: Item) => void }) {
  if (!numbers.length) return null;
  return (
    <div className="tree-menu__section">
      <p className="tree-menu__label">{label}</p>
      <ul className="tree-menu__list tree-menu__list--tight">
        {numbers.map((n) => {
          const dep = byNumber.get(n);
          if (dep) return <IssueRow item={dep} key={n} onPick={onPick} />;
          return (
            <li key={n}>
              <a className="tree-menu__row" href={`${REPO_URL}/issues/${n}`} target="_blank" rel="noreferrer">
                <span className="tree-menu__row-dot" aria-hidden="true">
                  #
                </span>
                <span className="tree-menu__row-title">Issue #{n} (outside the roadmap)</span>
                <span className="tree-menu__row-ms">↗</span>
              </a>
            </li>
          );
        })}
      </ul>
    </div>
  );
}

function Menu({
  pillar,
  system,
  item,
  filter,
  byNumber,
  onPick,
  onPickSystem,
  onClose,
}: {
  pillar: Pillar;
  system: System | undefined;
  item: Item | undefined;
  filter: Filter;
  byNumber: Map<number, Item>;
  onPick: (item: Item) => void;
  onPickSystem: (system: System) => void;
  onClose: () => void;
}) {
  const close = (
    <button className="tree-menu__close" type="button" aria-label="Close details" onClick={onClose}>
      ×
    </button>
  );

  if (item) {
    const issue = item.issue;
    const state = stateOf(issue);
    const status = issueStatus(issue);
    const { code, title } = titleOf(issue);
    const deps = issue.dependsOn ?? [];
    const blocking = deps.filter((n) => byNumber.get(n)?.issue.state === "open");
    const unlocks = [...byNumber.values()].filter((other) => (other.issue.dependsOn ?? []).includes(issue.number)).map((other) => other.issue.number);
    const why = issue.why || issue.excerpt;
    const labels = issue.labels.filter((l) => !l.startsWith("topic:") && !/^m\d+$/i.test(l));
    return (
      <aside className="tree-menu" aria-label="Issue details">
        {close}
        <p className="tree-menu__crumbs">
          {pillar.name} / {system?.name ?? "General"}
        </p>
        <h3 className="tree-menu__title">{title}</h3>
        <p className="tree-menu__tags">
          <span className={`tree-menu__state tree-menu__state--${state}`}>{status.label}</span>
          <span>{item.milestone.id}</span>
          <span>{code ?? `#${issue.number}`}</span>
          {issue.wave && <span>Wave {issue.wave}</span>}
          {labels.slice(0, 3).map((l) => (
            <span key={l}>{l}</span>
          ))}
        </p>

        {state === "open" && blocking.length > 0 && (
          <p className="tree-menu__note">
            Waits on {plural(blocking.length, "open issue")} below. You can still claim it and start on the parts that do not depend on them.
          </p>
        )}

        {why && (
          <div className="tree-menu__section">
            <p className="tree-menu__label">Why</p>
            <p className="tree-menu__copy">{why}</p>
          </div>
        )}
        {(issue.work?.length ?? 0) > 0 && (
          <div className="tree-menu__section">
            <p className="tree-menu__label">{state === "done" ? "What was built" : "What to build"}</p>
            <ul className="tree-menu__bullets">
              {issue.work!.map((w) => (
                <li key={w}>{w}</li>
              ))}
            </ul>
          </div>
        )}
        {(issue.proof?.length ?? 0) > 0 && (
          <div className="tree-menu__section">
            <p className="tree-menu__label">Done when</p>
            <ul className="tree-menu__bullets">
              {issue.proof!.map((p) => (
                <li key={p}>{p}</li>
              ))}
            </ul>
          </div>
        )}
        <Related label="Waits on" numbers={deps} byNumber={byNumber} onPick={onPick} />
        <Related label="Unlocks" numbers={unlocks} byNumber={byNumber} onPick={onPick} />

        {issue.assignees.length > 0 && (
          <div className="tree-menu__section">
            <p className="tree-menu__label">{state === "done" ? "Built by" : "On it"}</p>
            <ul className="tree-menu__people">
              {issue.assignees.map((login) => (
                <li key={login}>
                  <a href={`https://github.com/${login}`} target="_blank" rel="noreferrer">
                    <img src={`https://github.com/${login}.png?size=48`} alt="" width={24} height={24} loading="lazy" />
                    {login}
                  </a>
                </li>
              ))}
            </ul>
          </div>
        )}

        <p className="tree-menu__activity">
          {state === "done" && issue.closedAt
            ? `Closed ${formatDate(issue.closedAt)}`
            : issue.updatedAt
              ? `Updated ${formatDate(issue.updatedAt)}`
              : null}
          {issue.comments ? ` · ${plural(issue.comments, "comment")}` : ""}
        </p>

        {state === "open" && (
          <ol className="tree-menu__steps">
            <li>Read the full issue on GitHub: files, scope, and how to start.</li>
            <li>
              Comment <code>/claim</code>. A bot assigns you, and the tree updates.
            </li>
            <li>Ask questions in the issue's thread in the Discord #roadmap forum.</li>
          </ol>
        )}
        {state === "in-progress" && (
          <p className="tree-menu__copy">
            Up to ten people can work on one issue. Comment <code>/claim</code> to join, and talk it through in the Discord #roadmap forum.
          </p>
        )}
        <div className="tree-menu__actions">
          <a className="btn btn-primary" href={issue.url} target="_blank" rel="noreferrer">
            {state === "done" ? "See what shipped" : state === "open" ? "Pick this up" : "Join the team"} ↗
          </a>
          {state !== "done" && (
            <a className="btn" href={DISCORD_URL} target="_blank" rel="noreferrer">
              Discuss on Discord ↗
            </a>
          )}
        </div>
      </aside>
    );
  }

  if (system) {
    const shown = system.items.filter((i) => matches(i.issue, filter));
    return (
      <aside className="tree-menu" aria-label="System details">
        {close}
        <p className="tree-menu__crumbs">{pillar.name}</p>
        <h3 className="tree-menu__title">{system.name}</h3>
        <Progress items={system.items} />
        <StartHere items={system.items} byNumber={byNumber} onPick={onPick} />
        <ul className="tree-menu__list">
          {(shown.length ? shown : system.items).map((i) => (
            <IssueRow item={i} key={i.issue.number} onPick={onPick} />
          ))}
        </ul>
      </aside>
    );
  }

  const milestones = [...new Set(pillar.items.map((i) => i.milestone.id))].sort();
  return (
    <aside className="tree-menu" aria-label="Pillar details">
      {close}
      <p className="tree-menu__crumbs">Pillar</p>
      <h3 className="tree-menu__title">{pillar.name}</h3>
      <Progress items={pillar.items} />
      <p className="tree-menu__tags">
        {milestones.map((id) => {
          const inMs = pillar.items.filter((i) => i.milestone.id === id);
          return (
            <span key={id}>
              {id}: {inMs.filter((i) => i.issue.state === "closed").length}/{inMs.length}
            </span>
          );
        })}
      </p>
      <StartHere items={pillar.items} byNumber={byNumber} onPick={onPick} />
      <p className="tree-menu__label tree-menu__section">Systems</p>
      <ul className="tree-menu__list tree-menu__list--tight">
        {pillar.systems.map((s) => {
          const open = s.items.filter((i) => stateOf(i.issue) === "open").length;
          return (
            <li key={s.name}>
              <button className="tree-menu__row" type="button" onClick={() => onPickSystem(s)}>
                <span className="tree-menu__row-dot" aria-hidden="true">
                  <Icon name={s.name} />
                </span>
                <span className="tree-menu__row-title">{s.name}</span>
                <span className={`tree-menu__row-ms ${open ? "tree-menu__row-ms--open" : ""}`}>
                  {open ? `${open} open` : `${s.items.filter((i) => i.issue.state === "closed").length}/${s.items.length} done`}
                </span>
              </button>
            </li>
          );
        })}
      </ul>
    </aside>
  );
}

export function RoadmapTree() {
  const pillars = collectPillars();
  const { placed, width, height, rootX } = layout(pillars);
  const allItems = pillars.flatMap((p) => p.items);
  const byNumber = new Map(allItems.map((i) => [i.issue.number, i]));
  const counts: Record<Filter, number> = {
    all: allItems.length,
    open: allItems.filter((i) => stateOf(i.issue) === "open").length,
    "in-progress": allItems.filter((i) => stateOf(i.issue) === "in-progress").length,
    done: allItems.filter((i) => stateOf(i.issue) === "done").length,
  };
  const openItems = allItems
    .filter((i) => stateOf(i.issue) === "open")
    .sort((a, b) => a.milestone.number - b.milestone.number || a.issue.number - b.issue.number);

  const [filter, setFilter] = useState<Filter>("all");
  const [showAllOpen, setShowAllOpen] = useState(false);
  const frameRef = useRef<HTMLDivElement>(null);
  const [selection, setSelection] = useState<Selection>(null);
  const [zoom, setZoom] = useState(DEFAULT_ZOOM);
  const [dragging, setDragging] = useState(false);
  const viewportRef = useRef<HTMLDivElement>(null);
  const zoomRef = useRef(zoom);
  const dragRef = useRef<{ id: number; x: number; y: number; left: number; top: number; moved: boolean } | null>(null);
  const suppressClick = useRef(false);
  zoomRef.current = zoom;

  // Ctrl/Cmd + wheel (and trackpad pinch) zooms around the cursor; a plain wheel scrolls.
  useEffect(() => {
    const viewport = viewportRef.current;
    if (!viewport) return;
    function onWheel(event: WheelEvent) {
      if (!event.ctrlKey && !event.metaKey) return;
      event.preventDefault();
      const previous = zoomRef.current;
      const next = clampZoom(previous * (event.deltaY > 0 ? 0.9 : 1.1));
      if (next === previous || !viewport) return;
      const rect = viewport.getBoundingClientRect();
      const px = event.clientX - rect.left;
      const py = event.clientY - rect.top;
      const gx = (viewport.scrollLeft + px) / previous;
      const gy = (viewport.scrollTop + py) / previous;
      zoomRef.current = next;
      setZoom(next);
      requestAnimationFrame(() => {
        viewport.scrollLeft = gx * next - px;
        viewport.scrollTop = gy * next - py;
      });
    }
    viewport.addEventListener("wheel", onWheel, { passive: false });
    return () => viewport.removeEventListener("wheel", onWheel);
  }, []);

  useEffect(() => {
    const viewport = viewportRef.current;
    if (viewport) viewport.scrollLeft = rootX * zoomRef.current - viewport.clientWidth / 2;
    // Centre the root once on mount.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    function onKey(event: KeyboardEvent) {
      if (event.key === "Escape") setSelection(null);
    }
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  function centerOn(x: number, y: number) {
    const viewport = viewportRef.current;
    if (!viewport) return;
    const z = zoomRef.current;
    // Leave room for the details menu on the right when the map is wide enough to show both.
    const visible = viewport.clientWidth > 900 ? viewport.clientWidth - 380 : viewport.clientWidth;
    // Instant: two smooth scrolls at once (this and the page's scrollIntoView) cancel each other in Chrome.
    viewport.scrollTo({ left: x * z - visible / 2, top: y * z - viewport.clientHeight / 3 });
  }

  function zoomTo(next: number) {
    const viewport = viewportRef.current;
    const previous = zoomRef.current;
    next = clampZoom(next);
    if (!viewport || next === previous) return;
    const cx = (viewport.scrollLeft + viewport.clientWidth / 2) / previous;
    const cy = (viewport.scrollTop + viewport.clientHeight / 2) / previous;
    zoomRef.current = next;
    setZoom(next);
    requestAnimationFrame(() => {
      viewport.scrollLeft = cx * next - viewport.clientWidth / 2;
      viewport.scrollTop = cy * next - viewport.clientHeight / 2;
    });
  }

  function fit() {
    const viewport = viewportRef.current;
    if (!viewport) return;
    const next = clampZoom(Math.min(viewport.clientWidth / width, viewport.clientHeight / height));
    zoomRef.current = next;
    setZoom(next);
    requestAnimationFrame(() => viewport.scrollTo({ left: 0, top: 0 }));
  }

  function pickIssue(item: Item, focus = false) {
    for (const p of placed) {
      for (const s of p.systems) {
        const hit = s.issues.find((i) => i.item.issue.number === item.issue.number);
        if (hit) {
          setSelection({ pillar: p.pillar.name, system: s.system.name, issue: item.issue.number });
          if (focus) {
            centerOn(hit.x, hit.y);
            frameRef.current?.scrollIntoView({ behavior: "smooth", block: "nearest" });
          }
          return;
        }
      }
    }
  }

  function startDrag(event: ReactPointerEvent<HTMLDivElement>) {
    if (event.pointerType === "touch" || event.button !== 0) return;
    const viewport = viewportRef.current;
    if (!viewport) return;
    dragRef.current = { id: event.pointerId, x: event.clientX, y: event.clientY, left: viewport.scrollLeft, top: viewport.scrollTop, moved: false };
  }

  function moveDrag(event: ReactPointerEvent<HTMLDivElement>) {
    const viewport = viewportRef.current;
    const drag = dragRef.current;
    if (!viewport || !drag || drag.id !== event.pointerId) return;
    const dx = event.clientX - drag.x;
    const dy = event.clientY - drag.y;
    if (!drag.moved && Math.abs(dx) + Math.abs(dy) > 5) {
      drag.moved = true;
      viewport.setPointerCapture(event.pointerId);
      setDragging(true);
    }
    if (!drag.moved) return;
    viewport.scrollLeft = drag.left - dx;
    viewport.scrollTop = drag.top - dy;
  }

  function endDrag(event: ReactPointerEvent<HTMLDivElement>) {
    const viewport = viewportRef.current;
    const drag = dragRef.current;
    if (!viewport || !drag || drag.id !== event.pointerId) return;
    if (drag.moved) {
      suppressClick.current = true;
      window.setTimeout(() => (suppressClick.current = false), 0);
    }
    dragRef.current = null;
    setDragging(false);
    if (viewport.hasPointerCapture(event.pointerId)) viewport.releasePointerCapture(event.pointerId);
  }

  const selPillar = selection ? pillars.find((p) => p.name === selection.pillar) : undefined;
  const selSystem = selection?.system ? selPillar?.systems.find((s) => s.name === selection.system) : undefined;
  const selItem = selection?.issue ? selSystem?.items.find((i) => i.issue.number === selection.issue) : undefined;
  const hasSel = Boolean(selPillar);

  // Wires on the path from the root to the selection light up.
  const litPillar = (p: Pillar) => selection?.pillar === p.name;
  const litSystem = (p: Pillar, s: System) => litPillar(p) && selection?.system === s.name;
  const litIssue = (p: Pillar, s: System, index: number) => {
    if (!litSystem(p, s) || selection?.issue == null) return false;
    const at = s.items.findIndex((i) => i.issue.number === selection.issue);
    return index <= at;
  };
  const systemDim = (s: System) => !s.items.some((i) => matches(i.issue, filter));
  const pillarDim = (p: Pillar) => p.systems.every(systemDim);

  return (
    <section className="wrap pb-4" aria-labelledby="tree-title">
      <div className="sheet p-5 md:p-8">
        <div className="flex flex-wrap items-end justify-between gap-5 mb-6">
          <div>
            <p className="kicker mb-2">Pillar → system → issue</p>
            <h2 id="tree-title" className="display-2">
              Find a branch to grow
            </h2>
            <p className="muted mt-3 max-w-prose">
              Every node is a GitHub issue. The dashed, pulsing ones need a hand: click one, read it, and comment <code>/claim</code>{" "}
              on GitHub. You can claim alongside others.
            </p>
          </div>
          <p className="font-mono text-[0.75rem] uppercase tracking-[0.06em] text-ink-2">
            {counts.done}/{counts.all} done · {counts.open} open to claim
          </p>
        </div>

        {openItems.length > 0 && (
          <div className="tree-pick" aria-label="Issues that need a hand">
            <p className="kicker">Up for grabs · earliest milestone first</p>
            <ul>
              {(showAllOpen ? openItems : openItems.slice(0, PICK_LIMIT)).map((item) => (
                <li key={item.issue.number}>
                  <button type="button" className="tree-pick__chip" onClick={() => pickIssue(item, true)}>
                    <span>{item.milestone.id}</span>
                    {titleOf(item.issue).title}
                  </button>
                </li>
              ))}
              {openItems.length > PICK_LIMIT && (
                <li>
                  <button type="button" className="tree-pick__more" onClick={() => setShowAllOpen(!showAllOpen)}>
                    {showAllOpen ? "Show fewer" : `+${openItems.length - PICK_LIMIT} more`}
                  </button>
                </li>
              )}
            </ul>
          </div>
        )}

        <div className="tree-bar">
          <div className="tree-bar__filters" role="group" aria-label="Highlight issues by status">
            {FILTERS.map((option) => (
              <button
                className={`tree-bar__filter tree-bar__filter--${option.id}`}
                type="button"
                aria-pressed={filter === option.id}
                key={option.id}
                onClick={() => setFilter(filter === option.id ? "all" : option.id)}
              >
                {option.id !== "all" && (
                  <span className={`tree-key tree-key--${option.id}`} aria-hidden="true">
                    <Icon name={option.id} />
                  </span>
                )}
                {option.label}
                <span className="tree-bar__count">{counts[option.id]}</span>
              </button>
            ))}
          </div>
          <div className="tree-bar__zoom" role="group" aria-label="Map zoom">
            <button type="button" aria-label="Zoom out" disabled={zoom <= MIN_ZOOM} onClick={() => zoomTo(zoom / 1.2)}>
              −
            </button>
            <span>{Math.round(zoom * 100)}%</span>
            <button type="button" aria-label="Zoom in" disabled={zoom >= MAX_ZOOM} onClick={() => zoomTo(zoom * 1.2)}>
              +
            </button>
            <button type="button" onClick={fit}>
              Fit
            </button>
          </div>
        </div>

        <div className="tree-frame" ref={frameRef}>
          <div
            ref={viewportRef}
            className={`tree-viewport ${dragging ? "tree-viewport--dragging" : ""}`}
            role="region"
            aria-label="Tree of Life map. Drag to pan, Ctrl or Command plus wheel to zoom."
            onPointerDown={startDrag}
            onPointerMove={moveDrag}
            onPointerUp={endDrag}
            onPointerCancel={endDrag}
            onClickCapture={(event) => {
              if (!suppressClick.current) return;
              event.preventDefault();
              event.stopPropagation();
            }}
            onClick={(event) => {
              if (event.target === event.currentTarget || (event.target as HTMLElement).classList.contains("tree")) setSelection(null);
            }}
          >
            <div style={{ width: width * zoom, height: height * zoom }}>
              <div className={`tree ${hasSel ? "tree--focused" : ""}`} style={{ width, height, transform: `scale(${zoom})` }}>
                <svg className="tree__wires" width={width} height={height} aria-hidden="true">
                  {placed.map((p) => (
                    <g key={p.pillar.name} className={pillarDim(p.pillar) ? "tree__dim" : undefined}>
                      <line className={`tree__wire ${litPillar(p.pillar) ? "tree__wire--lit" : ""}`} x1={rootX} y1={ROOT_Y} x2={p.x} y2={p.y} />
                      <line className="tree__wire" x1={p.x} y1={p.y} x2={p.x} y2={p.systems[p.systems.length - 1]?.y ?? p.y} />
                      <line className="tree__wire tree__wire--future" x1={p.x} y1={p.systems[p.systems.length - 1]?.y ?? p.y} x2={p.x} y2={p.spineEnd} />
                      {p.systems.map((s) => (
                        <g key={s.system.name} className={systemDim(s.system) ? "tree__dim" : undefined}>
                          {litSystem(p.pillar, s.system) && <line className="tree__wire tree__wire--lit" x1={p.x} y1={p.y} x2={p.x} y2={s.y} />}
                          {s.issues.map((issue, i) => {
                            const from = i === 0 ? { x: s.x, y: s.y } : s.issues[i - 1];
                            return (
                              <line
                                key={issue.item.issue.number}
                                className={`tree__wire tree__wire--issue ${litIssue(p.pillar, s.system, i) ? "tree__wire--lit" : ""} ${
                                  matches(issue.item.issue, filter) ? "" : "tree__wire--dim"
                                }`}
                                x1={from.x}
                                y1={from.y}
                                x2={issue.x}
                                y2={issue.y}
                              />
                            );
                          })}
                        </g>
                      ))}
                    </g>
                  ))}
                </svg>

                <div className="tree-node tree-node--root" style={{ left: rootX, top: ROOT_Y }}>
                  <span className="tree-node__dot">
                    <Icon name="root" />
                  </span>
                  <span className="tree-node__side">
                    <span className="tree-node__name">Life Framework</span>
                    <span className="tree-node__meta">
                      {pillars.length} pillars · {counts.all} issues
                    </span>
                  </span>
                </div>

                {placed.map((p) => {
                  const open = p.pillar.items.filter((i) => stateOf(i.issue) === "open").length;
                  const selected = selection?.pillar === p.pillar.name && !selection.system;
                  return (
                    <div key={p.pillar.name} className={pillarDim(p.pillar) ? "tree__dim" : undefined}>
                      <button
                        className={`tree-node tree-node--pillar ${selected ? "tree-node--selected" : ""}`}
                        style={{ left: p.x, top: p.y }}
                        type="button"
                        aria-pressed={selected}
                        onClick={() => setSelection({ pillar: p.pillar.name, system: null, issue: null })}
                      >
                        <span className="tree-node__dot">
                          <Icon name={p.pillar.name} />
                        </span>
                        <span className="tree-node__side">
                          <span className="tree-node__name">{p.pillar.name}</span>
                          <span className="tree-node__meta">
                            {p.pillar.items.filter((i) => i.issue.state === "closed").length}/{p.pillar.items.length} done
                            {open > 0 && <strong> · {open} open</strong>}
                          </span>
                        </span>
                      </button>

                      {p.systems.map((s) => {
                        const sysSelected = selection?.pillar === p.pillar.name && selection.system === s.system.name && selection.issue == null;
                        return (
                          <div key={s.system.name} className={systemDim(s.system) ? "tree__dim" : undefined}>
                            <button
                              className={`tree-node tree-node--system ${sysSelected ? "tree-node--selected" : ""}`}
                              style={{ left: s.x, top: s.y }}
                              type="button"
                              aria-pressed={sysSelected}
                              onClick={() => setSelection({ pillar: p.pillar.name, system: s.system.name, issue: null })}
                            >
                              <span className="tree-node__dot">
                                <Icon name={s.system.name} />
                              </span>
                              <span className="tree-node__under">{s.system.name}</span>
                            </button>

                            {s.issues.map(({ item, x, y }) => {
                              const state = stateOf(item.issue);
                              const selectedIssue = selection?.issue === item.issue.number;
                              const { code, title } = titleOf(item.issue);
                              return (
                                <button
                                  key={item.issue.number}
                                  className={`tree-node tree-node--issue tree-node--${state} ${selectedIssue ? "tree-node--selected" : ""} ${
                                    matches(item.issue, filter) ? "" : "tree-node--dim"
                                  }`}
                                  style={{ left: x, top: y }}
                                  type="button"
                                  aria-pressed={selectedIssue}
                                  aria-label={`${title}, ${issueStatus(item.issue).label}, ${item.milestone.id} ${code ?? `#${item.issue.number}`}`}
                                  onClick={() => setSelection({ pillar: p.pillar.name, system: s.system.name, issue: item.issue.number })}
                                >
                                  <span className="tree-node__dot">
                                    <Icon name={state} />
                                    {item.milestone.id !== "M1" && <span className="tree-node__badge">{item.milestone.id}</span>}
                                  </span>
                                  <span className="tree-node__under">{title}</span>
                                </button>
                              );
                            })}
                          </div>
                        );
                      })}
                    </div>
                  );
                })}
              </div>
            </div>
          </div>

          {selPillar && (
            <Menu
              pillar={selPillar}
              system={selSystem}
              item={selItem}
              filter={filter}
              byNumber={byNumber}
              onPick={(item) => pickIssue(item)}
              onPickSystem={(s) => setSelection({ pillar: selPillar.name, system: s.name, issue: null })}
              onClose={() => setSelection(null)}
            />
          )}
        </div>

        <p className="tree-hint">Drag to pan · Ctrl/⌘ + wheel to zoom · click a node for details · Esc closes</p>
      </div>
    </section>
  );
}
