import { Link } from "react-router-dom";
import { GoLink, IssueLedger, PageHeader, RouteMap, ScopeMeter, Section, SourceNote, Stamp } from "@/components/ui";
import { CURRENT_MILESTONE, DISCORD_URL, githubData, milestone, siteData, splitTitle, type Issue } from "@/lib/content";

const byCode = (a: Issue, b: Issue) =>
  (splitTitle(a.title).code ?? "").localeCompare(splitTitle(b.title).code ?? "", undefined, { numeric: true }) || a.number - b.number;

const PLAN_DOCS: Record<string, string> = { M1: "/docs/roadmap-m1-playtest-town", M2: "/docs/roadmap-m2-outline" };

function MilestoneBlock({ id }: { id: string }) {
  const ms = milestone(id);
  const row = siteData.milestones.find((m) => m.id === id);
  if (!ms || !row) return null;
  const closed = ms.issues.filter((i) => i.state === "closed").sort(byCode);
  const open = ms.issues.filter((i) => i.state === "open").sort(byCode);
  const free = open.filter((i) => i.assignees.length === 0);
  const taken = open.filter((i) => i.assignees.length > 0);
  const current = id === CURRENT_MILESTONE;

  return (
    <Section
      id={id.toLowerCase()}
      kicker={current ? `${id} · the current milestone` : `${id} · next`}
      title={row.name}
      aside={current ? <Stamp>In progress</Stamp> : <Stamp tone="ink">Outline</Stamp>}
    >
      <div className="grid gap-10 lg:grid-cols-[1fr_1.3fr] mb-12">
        <div>
          <p className="kicker mb-2">Done when</p>
          <p className="muted">{row.doneWhen}</p>
          <p className="mt-5 flex flex-wrap gap-x-6 gap-y-2">
            {PLAN_DOCS[id] && <GoLink to={PLAN_DOCS[id]}>Read the plan</GoLink>}
            <GoLink to={ms.url}>Milestone on GitHub</GoLink>
          </p>
        </div>
        {ms.issues.length > 0 && (
          <div>
            <p className="flex items-baseline justify-between gap-4 mb-2">
              <span className="font-display font-bold text-4xl leading-none">
                {closed.length} <span className="text-ink-2 text-2xl">of {ms.issues.length}</span>
              </span>
              <span className="kicker">{current ? "scopes done" : "epics done"}</span>
            </p>
            <ScopeMeter issues={ms.issues} label={`${id} ${row.name}`} />
            <dl className="grid grid-cols-3 gap-4 mt-5 font-mono text-[0.8125rem] uppercase tracking-[0.06em]">
              <div>
                <dt className="text-ink-2">Done</dt>
                <dd className="text-lg">{closed.length}</dd>
              </div>
              <div>
                <dt className="text-ink-2">In progress</dt>
                <dd className="text-lg">{taken.length}</dd>
              </div>
              <div>
                <dt className="text-stamp">Needs a hand</dt>
                <dd className="text-lg">{free.length}</dd>
              </div>
            </dl>
          </div>
        )}
      </div>

      {free.length > 0 && (
        <div className="mb-12">
          <h3 className="display-3 mb-1">Needs a hand</h3>
          <p className="text-[0.9375rem] muted mb-4">
            Nobody is on these yet. Each issue lists its files and how to prove it. Comment on it to take it, or ask on{" "}
            <a href={DISCORD_URL} className="link">
              Discord
            </a>
            .
          </p>
          <IssueLedger issues={free} showStatus={false} />
        </div>
      )}
      {taken.length > 0 && (
        <div className="mb-12">
          <h3 className="display-3 mb-4">In progress</h3>
          <IssueLedger issues={taken} showStatus={false} />
        </div>
      )}
      {closed.length > 0 && (
        <details className="group">
          <summary className="cursor-pointer select-none display-3 list-none flex items-center gap-3">
            <span aria-hidden="true" className="font-mono text-base w-5 group-open:rotate-90 transition-transform">
              ▸
            </span>
            Done ({closed.length})
          </summary>
          <div className="mt-4">
            <IssueLedger issues={closed} showExcerpt={false} showStatus={false} />
          </div>
        </details>
      )}
    </Section>
  );
}

const Roadmap = () => {
  const later = siteData.milestones.filter((m) => !githubData.milestones.some((g) => g.id === m.id) && m.id > CURRENT_MILESTONE);
  return (
    <>
      <PageHeader kicker="Roadmap" title="Where we are on the road">
        <p>
          Each milestone ends with a public build and a filmed showcase. We do not start a milestone's features before the one before
          it is done. New systems wait; playability does not.
        </p>
      </PageHeader>
      <div className="wrap">
        <div className="sheet p-5 md:p-8">
          <RouteMap current={CURRENT_MILESTONE} />
        </div>
        <div className="mt-4">
          <SourceNote source={githubData.source} fetchedAt={githubData.fetchedAt} />
        </div>
      </div>

      {githubData.milestones.map((m) => (
        <MilestoneBlock key={m.id} id={m.id} />
      ))}

      <Section id="after" kicker="Further out" title="After that">
        <ul className="grid gap-6 md:grid-cols-2 mb-12">
          {later.map((m) => (
            <li key={m.id} className="sheet p-6">
              <p className="kicker">{m.id}</p>
              <h3 className="display-3 mt-1">{m.name}</h3>
              <p className="mt-2 muted text-[0.9375rem]">{m.doneWhen}</p>
            </li>
          ))}
        </ul>
        <p className="kicker mb-3">The scoped plan</p>
        <ul className="flex flex-wrap gap-x-6 gap-y-3">
          {[
            ["Roadmap index", "/docs/roadmap"],
            ["Phases", "/docs/roadmap-phases"],
            ["Release scopes", "/docs/roadmap-scopes"],
            ["Testing plan", "/docs/roadmap-testing"],
            ["What the scopes reuse", "/docs/roadmap-sources"],
          ].map(([label, to]) => (
            <li key={to}>
              <GoLink to={to}>{label}</GoLink>
            </li>
          ))}
        </ul>
      </Section>

      <div className="wrap">
        <details className="sheet p-5 md:p-8 group">
          <summary className="cursor-pointer select-none list-none flex items-center gap-3">
            <span aria-hidden="true" className="font-mono w-5 group-open:rotate-90 transition-transform">
              ▸
            </span>
            <span>
              <span className="kicker block">Archive</span>
              <span className="display-3">The 2025 community roadmap</span>
            </span>
          </summary>
          <p className="muted text-[0.9375rem] mt-4 mb-8 max-w-prose">
            The first plan, from the old site's database, exported {siteData.archive.exported}. Most of its systems now exist in the
            mod. The live plan is the milestones above and the <Link to="/docs/roadmap" className="link">roadmap docs</Link>.
          </p>
          <div className="grid gap-8 md:grid-cols-2">
            {siteData.archive.phases.map((p) => (
              <div key={p.number}>
                <h3 className="heading">
                  Phase {p.number}: {p.name}
                </h3>
                <p className="text-sm muted mb-3">{p.description}</p>
                <ul className="text-[0.9375rem] space-y-1.5">
                  {p.items.map((it) => (
                    <li key={it.title} className="grid grid-cols-[5.5rem_1fr] gap-2">
                      <span className="font-mono text-[0.7rem] uppercase tracking-[0.06em] text-ink-2 pt-1">
                        {it.status === "completed" ? "done" : it.status === "in-progress" ? "started" : "planned"}
                      </span>
                      <span>{it.title}</span>
                    </li>
                  ))}
                </ul>
              </div>
            ))}
          </div>
        </details>
      </div>
    </>
  );
};

export default Roadmap;
