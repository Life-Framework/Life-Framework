import { ClipPlayer, Figure, GoLink, PageHeader, RouteMap, Section, SourceNote, Stamp } from "@/components/ui";
import { LayerTag, MilestoneList } from "@/components/MilestoneList";
import { RoadmapTree } from "@/components/RoadmapTree";
import { CORE_VS_PACK_URL, CURRENT_MILESTONE, githubData, siteData, still } from "@/lib/content";

const Roadmap = () => {
  return (
    <>
      <PageHeader kicker="Roadmap" title="The Tree of Life">
        <p>
          The plan is five milestones on GitHub, done in order. Each one ends with a public build and a filmed showcase, and the next
          one's features wait until it is done. Below them, the Tree of Life sorts the same issues by topic. Find an open branch and
          claim it.
        </p>
      </PageHeader>

      <div className="wrap mb-8">
        <div className="sheet p-5 md:p-8">
          <p className="kicker mb-4">Where we are on the road</p>
          <RouteMap current={CURRENT_MILESTONE} compact />
        </div>
      </div>

      <Section id="milestones" kicker="M1 to M5" title="The milestones">
        <div className="grid gap-10 lg:grid-cols-[1fr_2fr] lg:items-start">
          <div className="space-y-4 muted lg:sticky lg:top-28">
            <p>
              Every issue is tagged <LayerTag layer="Core" /> or <LayerTag layer="Flagship" />. The core decides what the game can do.
              It holds the systems any server can run. The flagship pack decides what our server is. It holds the places, names,
              prices, period look, vehicle catalogue, and story of 1989 Everon.
            </p>
            <p>
              Today both live in one addon. M3 splits them into two Workshop items, the <code>LifeFramework</code> core and the{" "}
              <code>LF_Everon1989</code> pack, so another community can swap our pack for their own and keep the core.
            </p>
            <p>An epic opens to show its sub-issues in order. Click one to read it on GitHub.</p>
            <p>
              <GoLink to={CORE_VS_PACK_URL}>Core vs pack</GoLink>
            </p>
          </div>
          <MilestoneList />
        </div>
        <div className="mt-6">
          <SourceNote source={githubData.source} fetchedAt={githubData.fetchedAt} />
        </div>
      </Section>

      <RoadmapTree />

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
          Our test world used to be a white grid with the sites in a row. Now it is a small 1989 village built from vanilla
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

      <Section id="plans" kicker="Planning" title="Detailed plans">
        <p className="kicker mb-3">The scoped plan</p>
        <ul className="flex flex-wrap gap-x-6 gap-y-3">
          {[
            ["Roadmap index", "/docs/roadmap"],
            ["M1 plan", "/docs/roadmap-m1-playtest-town"],
            ["M2 outline", "/docs/roadmap-m2-outline"],
            ["M3 outline", "/docs/roadmap-m3-outline"],
            ["M4 outline", "/docs/roadmap-m4-outline"],
            ["Scope cards", "/docs/roadmap-scopes"],
            ["Testing plan", "/docs/roadmap-testing"],
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
