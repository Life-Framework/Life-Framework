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
  type SubIssue,
} from "@/lib/content";

// "M2 epic: Moderation (incident log + Game Master tools)" -> "Moderation (incident log + Game Master tools)"
const shortTitle = (t: string) => {
  // Also drops a sub-issue's "M2-Moderation:" prefix; the epic above it already says so.
  const s = splitTitle(t)
    .title.replace(/^M\d+\s+(?:epic|flagship):\s*/i, "")
    .replace(/^M\d+-[A-Za-z][\w &]*:\s*/, "")
    .replace(/\s*\(M\d+\)$/, "");
  // Capitalize a plain first word ("relocated"), not a command or path ("tools\cli").
  return /^[a-z]+(?=\s)/.test(s) ? s.charAt(0).toUpperCase() + s.slice(1) : s;
};
const isEpic = (i: Issue) => (i.subIssues?.length ?? 0) > 0 || i.labels.includes("epic");

const ROW = "grid grid-cols-[3.25rem_minmax(0,1fr)_auto] gap-x-3";

export function LayerTag({ layer }: { layer: Layer }) {
  return (
    <span
      className={`inline-block font-mono text-[0.65rem] uppercase tracking-[0.06em] px-1.5 py-px border align-[0.1em] ${
        layer === "Flagship" ? "border-ink bg-ink text-paper" : "border-ink-2 text-ink-2"
      }`}
    >
      {layer}
    </span>
  );
}

function StateMark({ state, assignees }: { state: "open" | "closed"; assignees: string[] }) {
  const label = state === "closed" ? "Done" : assignees.length ? "In progress" : "Open";
  return (
    <span className={`font-mono text-[0.7rem] uppercase tracking-[0.06em] whitespace-nowrap pt-0.5 ${state === "closed" ? "text-green" : "text-ink-2"}`}>
      {state === "closed" ? "✓ " : ""}
      {label}
    </span>
  );
}

function SubRow({ s, milestoneId }: { s: SubIssue; milestoneId: string }) {
  return (
    <li className={`${ROW} py-2 border-b border-rule last:border-b-0 text-[0.9375rem]`}>
      <span className="font-mono text-[0.75rem] text-ink-2 pt-0.5">#{s.number}</span>
      <a href={s.url} className="link leading-snug">
        {shortTitle(s.title)}
        {s.milestone && s.milestone !== milestoneId && <span className="font-mono text-[0.7rem] text-ink-2"> · {s.milestone}</span>}{" "}
        <LayerTag layer={issueLayer(s, s.milestone ?? milestoneId)} />
      </a>
      <StateMark state={s.state} assignees={s.assignees} />
    </li>
  );
}

function IssueRow({ issue, milestoneId }: { issue: Issue; milestoneId: string }) {
  const subs = issue.subIssues ?? [];
  const { code } = splitTitle(issue.title);
  const layer = issueLayer(issue, milestoneId);
  const title = (
    <>
      {shortTitle(issue.title)} <LayerTag layer={layer} />
    </>
  );
  if (subs.length === 0) {
    return (
      <li className={`${ROW} py-2.5 border-b border-rule`}>
        <span className="font-mono text-[0.75rem] text-ink-2 pt-0.5">{code ?? `#${issue.number}`}</span>
        <span className="leading-snug">
          {isEpic(issue) && <span className="kicker block">Epic · sub-issues not filed yet</span>}
          <a href={issue.url} className="link">
            {title}
          </a>
        </span>
        <StateMark state={issue.state} assignees={issue.assignees} />
      </li>
    );
  }
  const done = subs.filter((s) => s.state === "closed").length;
  return (
    <li className="border-b border-rule">
      <details className="group">
        <summary className={`${ROW} py-2.5 cursor-pointer`}>
          <span className="font-mono text-[0.75rem] text-ink-2 pt-0.5">#{issue.number}</span>
          <span className="leading-snug">
            <span className="kicker block">
              <span aria-hidden="true" className="inline-block transition-transform group-open:rotate-90">▸</span> Epic · {done}/{subs.length}{" "}
              sub-issues done
            </span>
            <span className="font-semibold">{title}</span>
          </span>
          <StateMark state={issue.state} assignees={issue.assignees} />
        </summary>
        <div className="ml-3 md:ml-[3.25rem] mb-3 border-l-2 border-ink pl-3">
          <ol>
            {subs.map((s) => (
              <SubRow key={s.number} s={s} milestoneId={milestoneId} />
            ))}
          </ol>
          <p className="mt-2">
            <GoLink to={issue.url}>The epic on GitHub</GoLink>
          </p>
        </div>
      </details>
    </li>
  );
}

function MilestoneCard({ m }: { m: Milestone }) {
  const plan = MILESTONE_PLAN[m.id];
  const status = milestoneStatus(m);
  // An issue that is a sub-issue of an epic in the same milestone is listed under the epic only.
  const nested = new Set(m.issues.flatMap((i) => (i.subIssues ?? []).map((s) => s.number)));
  const top = m.issues
    .filter((i) => !nested.has(i.number))
    .sort((a, b) => Number(a.state === "closed") - Number(b.state === "closed") || Number(isEpic(b)) - Number(isEpic(a)) || a.number - b.number);
  const done = m.issues.filter((i) => i.state === "closed").length;
  const epics = top.filter(isEpic).length;
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
      {top.length > 0 ? (
        <details className="mt-4 border-t-2 border-ink" open={m.state !== "closed" && m.id !== "M1" && top.length <= 12}>
          <summary className="cursor-pointer py-2.5 font-mono text-[0.75rem] uppercase tracking-[0.06em]">
            {m.issues.length} {m.issues.length === 1 ? "issue" : "issues"}
            {epics > 0 && ` · ${epics} ${epics === 1 ? "epic" : "epics"}`} · {done} done
          </summary>
          <ul className="border-t border-rule">
            {top.map((i) => (
              <IssueRow key={i.number} issue={i} milestoneId={m.id} />
            ))}
          </ul>
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
