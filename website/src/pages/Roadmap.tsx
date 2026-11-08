import { Link } from "react-router-dom";
import { GoLink, PageHeader, Section, SourceNote } from "@/components/ui";
import { RoadmapTree } from "@/components/RoadmapTree";
import { githubData, siteData } from "@/lib/content";

const Roadmap = () => (
  <>
    <PageHeader kicker="Roadmap" title="The Tree of Life">
      <p>
        Everything planned for Life Framework, grown from GitHub issues. Each pillar branches into systems, and each system into the
        work that builds it. Find an open branch and claim it.
      </p>
    </PageHeader>

    <RoadmapTree />

    <div className="wrap mt-4">
      <SourceNote source={githubData.source} fetchedAt={githubData.fetchedAt} />
    </div>

    <Section id="plans" kicker="Planning" title="Detailed plans">
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
          mod. The live plan is the Tree of Life above and the <Link to="/docs/roadmap" className="link">roadmap docs</Link>.
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

export default Roadmap;
