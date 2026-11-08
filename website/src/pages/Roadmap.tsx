import { ClipPlayer, Figure, GoLink, IssueLedger, PageHeader, RouteMap, ScopeMeter, Section, SourceNote, Stamp } from "@/components/ui";
import { CURRENT_MILESTONE, DISCORD_URL, githubData, milestone, siteData, splitTitle, still, type Issue } from "@/lib/content";

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

      <Section id="loop" kicker="The bar for M1" title="One loop, start to finish">
        <div className="grid gap-10 lg:grid-cols-[1fr_1.4fr]">
          <p className="lede muted">
            A stranger joins a dedicated server and gets through this without asking anyone. Every step gives feedback in the world and
            is checked from the player's view. Then we restart the server, and what they earned is still there.
          </p>
          <ol className="border-t-2 border-ink">
            {siteData.loop.map((step, i) => (
              <li key={step} className="grid grid-cols-[2.5rem_1fr] gap-3 py-3 border-b border-rule">
                <span className="font-mono text-sm text-ink-2 pt-0.5">{String(i + 1).padStart(2, "0")}</span>
                <span>{step}</span>
              </li>
            ))}
          </ol>
        </div>
      </Section>

      <Section id="village" kicker="Captured in the game" title="From the playtest village" aside={<Stamp tone="ink">Work in progress</Stamp>}>
        <p className="lede mb-10 muted">
          Until 8 November our test world was a white grid with the sites in a row. Now it is a small 1989 village built from vanilla
          Everon buildings, with every M1 site on one street and the test fixtures fenced off to the side. Every picture here is a
          capture from the game, linked to the pull request it came from.
        </p>
        <div className="grid gap-8 lg:grid-cols-[1.6fr_1fr] lg:items-start">
          <Figure still={still("village-square")} sizes="(min-width: 1024px) 700px, 100vw" />
          <div className="grid gap-8 md:grid-cols-2 lg:grid-cols-1">
            <Figure still={still("village-post")} sizes="(min-width: 1024px) 400px, (min-width: 768px) 50vw, 100vw" />
            <Figure still={still("village-police")} sizes="(min-width: 1024px) 400px, (min-width: 768px) 50vw, 100vw" />
          </div>
        </div>
        <div className="grid gap-8 md:grid-cols-2 mt-8">
          <Figure still={still("village-fuel")} />
          <Figure still={still("sign-board")} />
        </div>
        <h3 className="kicker mt-16 mb-4">Before the village</h3>
        <div className="grid gap-8 lg:grid-cols-2 lg:items-start">
          <ClipPlayer clip={siteData.media.clips[0]} />
          <Figure still={still("street-row")} />
        </div>
      </Section>

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

    </>
  );
};

export default Roadmap;
