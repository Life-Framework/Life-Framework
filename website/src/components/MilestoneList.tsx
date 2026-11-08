import { GoLink, Stamp } from "@/components/ui";
import {
  MILESTONE_LAYER,
  MILESTONE_PLAN,
  issueLayer,
  milestoneName,
  milestoneStatus,
  milestonesInOrder,
  splitTitle,
  type Issue,
  type Layer,
  type Milestone,
} from "@/lib/content";

// "M2-Medic: Downed state on lethal damage" -> { area: "Medic", name: "Downed state on lethal damage" }.
// Other titles lose only their milestone markers: "M2 epic: ", "M1: M1-24 ", " (M2)".
function parseTitle(t: string): { area: string | null; name: string } {
  const m = /^M\d+-([A-Za-z][\w &]*?):\s*(.+)$/.exec(t);
  const raw = m
    ? m[2]
    : splitTitle(t)
        .title.replace(/^M\d+\s+(?:epic|flagship):\s*/i, "")
        .replace(/\s*\(M\d+\)$/, "");
  // Capitalize a plain first word ("relocated"), not a command or path ("tools\cli", "schemaVersion").
  const name = /^[a-z]{3,}(?=\s)/.test(raw) ? raw.charAt(0).toUpperCase() + raw.slice(1) : raw;
  return { area: m ? m[1] : null, name };
}

// "Config layer (one owner config.json over shipped defaults)" -> "Config layer"
const branchName = (epic: Issue) => parseTitle(epic.title).name.replace(/\s*\([^)]*\)\s*$/, "");

export function LayerTag({ layer }: { layer: Layer }) {
  return (
    <span
      className={`inline-block font-mono text-[0.625rem] leading-none uppercase tracking-[0.06em] px-1 py-[3px] border align-[0.15em] ${
        layer === "Flagship" ? "border-ink bg-ink text-paper" : "border-ink-2 text-ink-2"
      }`}
    >
      {layer}
    </span>
  );
}

type Item = { number: number; title: string; state: "open" | "closed"; url: string; assignees: string[]; layer: Layer; milestone: string | null };

// A small state marker: done, someone on it, or open.
function Marker({ item }: { item: Item }) {
  const [mark, label, cls] =
    item.state === "closed" ? ["✓", "Done", "text-green"] : item.assignees.length ? ["◐", "In progress", "text-ink"] : ["○", "Open", "text-ink-2"];
  return (
    <span className={`font-mono text-[0.75rem] w-3 shrink-0 text-center ${cls}`} title={label}>
      <span aria-hidden="true">{mark}</span>
      <span className="sr-only">{label}</span>
    </span>
  );
}

function ItemLine({ item, blockLayer, milestoneId }: { item: Item; blockLayer: Layer; milestoneId: string }) {
  const { name } = parseTitle(item.title);
  return (
    <li className="flex gap-2 py-[3px] text-[0.875rem] leading-snug">
      <Marker item={item} />
      <span className="min-w-0">
        <a href={item.url} className={`link ${item.state === "closed" ? "text-ink-2" : ""}`} title={`#${item.number} ${item.title}`}>
          {name}
        </a>
        {item.milestone && item.milestone !== milestoneId && <span className="font-mono text-[0.7rem] text-ink-2"> · {item.milestone}</span>}
        {item.layer !== blockLayer && (
          <>
            {" "}
            <LayerTag layer={item.layer} />
          </>
        )}
      </span>
    </li>
  );
}

type Block = { key: string; heading: string; href: string | null; hint: string; layer: Layer; items: Item[]; epic: boolean };

function BlockCard({ b, milestoneId }: { b: Block; milestoneId: string }) {
  const done = b.items.filter((i) => i.state === "closed").length;
  return (
    <section className={`mb-5 border-t-2 border-ink pt-2 ${b.epic ? "break-inside-avoid" : ""}`} aria-label={b.heading}>
      <h4 className="flex items-baseline justify-between gap-3">
        <span className="font-semibold leading-snug">
          {b.href ? (
            <a href={b.href} className="link" title={b.hint}>
              {b.heading}
            </a>
          ) : (
            b.heading
          )}{" "}
          <LayerTag layer={b.layer} />
        </span>
        {b.items.length > 0 && (
          <span className="font-mono text-[0.75rem] text-ink-2 whitespace-nowrap">
            {done}/{b.items.length}
          </span>
        )}
      </h4>
      {b.items.length > 0 ? (
        <ol className="mt-1">
          {b.items.map((i) => (
            <ItemLine key={i.number} item={i} blockLayer={b.layer} milestoneId={milestoneId} />
          ))}
        </ol>
      ) : (
        <p className="mt-1 text-[0.8125rem] muted">Sub-issues not filed yet.</p>
      )}
    </section>
  );
}

// Epics become branches with their sub-issues in GitHub's (build) order. Other issues group by their
// "M2-Area:" prefix when they have one, else into one "Other issues" block.
function blocksOf(m: Milestone): Block[] {
  const nested = new Set(m.issues.flatMap((i) => (i.subIssues ?? []).map((s) => s.number)));
  const isEpic = (i: Issue) => (i.subIssues?.length ?? 0) > 0 || i.labels.includes("epic");
  const asItem = (i: Issue): Item => ({ ...i, layer: issueLayer(i, m.id), milestone: m.id });
  const blocks: Block[] = m.issues
    .filter((i) => isEpic(i) && !nested.has(i.number))
    .sort((a, b) => a.number - b.number)
    .map((e) => ({
      key: `epic-${e.number}`,
      heading: branchName(e),
      href: e.url,
      hint: `#${e.number} ${e.title}`,
      layer: issueLayer(e, m.id),
      epic: true,
      items: (e.subIssues ?? []).map((s) => ({ ...s, layer: issueLayer(s, s.milestone ?? m.id) })),
    }));
  const rest = m.issues.filter((i) => !isEpic(i) && !nested.has(i.number)).sort((a, b) => a.number - b.number);
  const byArea = new Map<string, Issue[]>();
  for (const i of rest) {
    const area = parseTitle(i.title).area ?? "";
    if (!byArea.has(area)) byArea.set(area, []);
    byArea.get(area)!.push(i);
  }
  for (const [area, list] of byArea) {
    if (!area) continue;
    const layers = new Set(list.map((i) => issueLayer(i, m.id)));
    blocks.push({ key: `area-${area}`, heading: area, href: null, hint: "", layer: layers.size === 1 ? [...layers][0] : "Core", epic: true, items: list.map(asItem) });
  }
  const other = byArea.get("") ?? [];
  if (other.length) {
    const items = other.map(asItem).sort((a, b) => Number(a.state === "closed") - Number(b.state === "closed") || a.number - b.number);
    const layers = new Set(items.map((i) => i.layer));
    blocks.push({
      key: "other",
      heading: blocks.length ? "Other issues" : "Scopes",
      href: null,
      hint: "",
      layer: layers.size === 1 ? [...layers][0] : "Core",
      // One long list may run across columns; the branch blocks stay whole.
      epic: items.length <= 12,
      items,
    });
  }
  return blocks;
}

function MilestoneCard({ m }: { m: Milestone }) {
  const plan = MILESTONE_PLAN[m.id];
  const status = milestoneStatus(m);
  const blocks = blocksOf(m);
  const done = m.issues.filter((i) => i.state === "closed").length;
  const branches = blocks.filter((b) => b.key.startsWith("epic-")).length;
  const goal = plan?.goal ?? m.description.split(/(?<=\.)\s/, 1)[0];
  return (
    <li className="sheet p-5 md:p-6" id={m.id.toLowerCase()}>
      <div className="flex flex-wrap items-start justify-between gap-3">
        <div>
          <p className="kicker">
            {m.id}
            {MILESTONE_LAYER[m.id] && <span className="text-ink-2"> · {MILESTONE_LAYER[m.id]}</span>}
          </p>
          <h3 className="display-3 mt-1">{milestoneName(m)}</h3>
        </div>
        <Stamp tone={status.tone}>{status.label}</Stamp>
      </div>
      <p className="mt-3 muted max-w-prose">{goal}</p>
      <p className="mt-3 flex flex-wrap gap-x-6 gap-y-2">
        {plan && <GoLink to={plan.plan.to}>{plan.plan.label}</GoLink>}
        <GoLink to={m.url}>{m.id} on GitHub</GoLink>
      </p>
      {blocks.length > 0 ? (
        <details className="mt-4 border-t border-rule" open={m.state !== "closed" && m.id !== "M1"}>
          <summary className="cursor-pointer py-2.5 font-mono text-[0.75rem] uppercase tracking-[0.06em]">
            {m.issues.length} {m.issues.length === 1 ? "issue" : "issues"}
            {branches > 0 && ` · ${branches} ${branches === 1 ? "branch" : "branches"}`} · {done} done
          </summary>
          <div className="columns-1 sm:columns-2 xl:columns-3 gap-8 pt-2">
            {blocks.map((b) => (
              <BlockCard key={b.key} b={b} milestoneId={m.id} />
            ))}
          </div>
        </details>
      ) : (
        <p className="mt-4 text-sm muted">No issues filed yet.</p>
      )}
    </li>
  );
}

export function MilestoneList() {
  return (
    <ol className="grid gap-6">
      {milestonesInOrder().map((m) => (
        <MilestoneCard key={m.id} m={m} />
      ))}
    </ol>
  );
}
