import { Link } from "react-router-dom";
import { Arrow, DiscordIcon } from "@/components/icons";
import { ClipPlayer, Figure, GoLink, RouteMap, ScopeMeter, Section, Stamp } from "@/components/ui";
import {
  CURRENT_MILESTONE,
  DISCORD_URL,
  blob,
  formatDate,
  githubData,
  milestone,
  milestoneProgress,
  siteData,
  still,
} from "@/lib/content";

const reasons = [
  {
    n: "01",
    title: "In the world, not in a menu",
    body: (
      <>
        Cash, keys, ID cards, a fine ticket, goods on a shelf, a notice on a board. Feedback comes from the world first: an item
        changing hands, a sound, a door that opens. A screen shows up only where it beats walking around, like typing an amount, and
        then it looks like a paper form or a terminal.
      </>
    ),
    more: { label: "The in-world rule", to: "/decisions#in-world-first" },
  },
  {
    n: "02",
    title: "A framework, proven by a flagship",
    body: (
      <>
        Server owners host it, configure it, and add jobs and shops without forking. Our own 1989 Everon server is the proof that it
        plays. The core knows nothing about 1989. Everon is a content pack on top, so a modern setting runs on the same systems.
      </>
    ),
    more: { label: "For server owners", to: "/servers" },
  },
  {
    n: "03",
    title: "Open source, built for agents",
    body: (
      <>
        MIT on GitHub. The repo is set up so an AI agent can pick up a scope cold and finish it, and a person can read every line it
        wrote. Nothing counts until it is proven: tests that can fail, a check on every pull request, and a harness that walks the
        street in the game and takes screenshots.
      </>
    ),
    more: { label: "How to contribute", to: "/contribute" },
  },
];

const Home = () => {
  const m1 = milestone(CURRENT_MILESTONE);
  const { done, total } = milestoneProgress(CURRENT_MILESTONE);
  const m1Name = siteData.milestones.find((m) => m.id === CURRENT_MILESTONE)?.name ?? "Playtest Town";
  const clip = siteData.media.clips[0];

  return (
    <>
      {/* Hero */}
      <div className="wrap pt-10 md:pt-16 pb-12 grid gap-10 lg:grid-cols-[1.05fr_1fr] lg:items-center">
        <div>
          <p className="kicker mb-5">Open-source life and roleplay for Arma Reforger</p>
          <h1 className="display-1">
            Real cash.
            <br />
            Real keys.
            <br />A fine on <span className="underline decoration-sign decoration-[0.12em] underline-offset-[0.08em]">paper</span>.
          </h1>
          <div className="lede mt-7 space-y-4">
            <p>
              Life Framework is a life/RP framework where the things that matter are objects in the world. The money in your pocket can
              be taken off your body. Your car key is an item someone can steal. A police fine is a ticket you carry to the station desk.
            </p>
            <p className="muted">
              We prove it on a 1989 Everon server we run ourselves. It is not out yet. We are building the M1 playtest town now; private
              playtests with friends come next, then the public Workshop release.
            </p>
          </div>
          <div className="mt-8 flex flex-wrap gap-3">
            <Link to="/roadmap" className="btn btn-primary">
              Follow the roadmap <Arrow />
            </Link>
            <a href={DISCORD_URL} className="btn btn-ghost">
              <DiscordIcon className="w-4 h-4" /> Join the Discord
            </a>
          </div>
        </div>
        <div className="relative">
          <Figure still={still("village-street")} eager sizes="(min-width: 1024px) 560px, 100vw" />
          <div className="absolute -top-3 right-3 md:-right-3">
            <Stamp tilt>Playtest village · in progress</Stamp>
          </div>
        </div>
      </div>

      {/* Status */}
      <div className="wrap">
        <div className="sheet p-5 md:p-8">
          <div className="flex flex-wrap items-center justify-between gap-3 mb-6">
            <p className="kicker">Status notice · {formatDate(githubData.fetchedAt)}</p>
            <Stamp>In development</Stamp>
          </div>
          <div className="grid gap-8 lg:grid-cols-[1fr_1.2fr]">
            <div>
              <h2 className="display-3">
                {CURRENT_MILESTONE} {m1Name}
              </h2>
              <p className="mt-3 text-[0.9375rem] muted">
                A signposted test town where a stranger can play the whole loop on a dedicated server with nobody helping. Then two or
                three private playtests with friends, the fixes they turn up, and the first public Workshop release with a showcase
                video.
              </p>
            </div>
            <div>
              <p className="flex items-baseline justify-between gap-4 mb-2">
                <span className="font-display font-bold text-4xl leading-none">
                  {done} <span className="text-ink-2 text-2xl">of {total}</span>
                </span>
                <span className="kicker">scopes done</span>
              </p>
              {m1 && <ScopeMeter issues={m1.issues} label={`${CURRENT_MILESTONE} ${m1Name}`} />}
              <p className="mt-4">
                <GoLink to="/roadmap">See every scope</GoLink>
              </p>
            </div>
          </div>
          <div className="hr mt-8 pt-8">
            <RouteMap current={CURRENT_MILESTONE} compact />
          </div>
        </div>
      </div>

      {/* Why */}
      <Section id="why" kicker="What makes it different" title="Three rules we build by">
        <ol className="grid gap-10 md:grid-cols-3">
          {reasons.map((r) => (
            <li key={r.n} className="flex flex-col">
              <span className="font-mono text-sm text-stamp" aria-hidden="true">
                {r.n}
              </span>
              <h3 className="display-3 mt-2 mb-3">{r.title}</h3>
              <p className="text-[0.9875rem] muted flex-1">{r.body}</p>
              <p className="mt-4">
                <GoLink to={r.more.to}>{r.more.label}</GoLink>
              </p>
            </li>
          ))}
        </ol>
        <figure className="mt-16 md:mt-20 border-l-4 border-sign pl-5 md:pl-8 max-w-4xl">
          <blockquote className="font-display font-bold uppercase text-[clamp(1.6rem,3.6vw,2.6rem)] leading-[1.05]">
            {siteData.ambition}
          </blockquote>
          <figcaption className="kicker mt-4">
            From{" "}
            <Link to="/docs/direction" className="link">
              docs/direction.md
            </Link>
          </figcaption>
        </figure>
      </Section>

      {/* Evidence */}
      <Section
        id="village"
        kicker="Captured in the game"
        title="From the playtest village"
        aside={<Stamp tone="ink">Work in progress</Stamp>}
      >
        <p className="lede mb-10 muted">
          Until 8 November our test world was a white grid with the sites in a row. Now it is a small 1989 village built from
          vanilla Everon buildings, with every M1 site on one street and the test fixtures fenced off to the side. The sites still
          need their signs, sounds, and polish. Every picture here is a capture from the game, linked to the pull request it came
          from.
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
          <ClipPlayer clip={clip} />
          <Figure still={still("street-row")} />
        </div>
      </Section>

      {/* The loop */}
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

      {/* Devlog */}
      <Section id="latest" kicker="Devlog" title="Lately" aside={<GoLink to="/devlog">The whole devlog</GoLink>}>
        <div className="grid gap-12 lg:grid-cols-2">
          <div>
            <h3 className="kicker mb-3">Merged</h3>
            <ul className="border-t-2 border-ink">
              {githubData.latestPulls.map((p) => (
                <li key={p.number} className="py-4 border-b border-rule">
                  <span className="font-mono text-[0.75rem] uppercase tracking-[0.06em] text-ink-2">
                    {formatDate(p.mergedAt)} · #{p.number}
                  </span>
                  <a href={p.url} className="heading link block mt-1">
                    {p.title}
                  </a>
                  {p.summary && <p className="mt-1 text-[0.9375rem] muted leading-snug">{p.summary}</p>}
                </li>
              ))}
            </ul>
          </div>
          <div>
            <h3 className="kicker mb-3">Decided</h3>
            <ul className="border-t-2 border-ink">
              {siteData.latestDecisions.map((d) => (
                <li key={d.id} className="py-4 border-b border-rule">
                  <span className="font-mono text-[0.75rem] uppercase tracking-[0.06em] text-ink-2">{formatDate(d.date)} · decision</span>
                  <Link to={`/decisions#${d.id}`} className="heading link block mt-1">
                    {d.title}
                  </Link>
                  <p className="mt-1 text-[0.9375rem] muted leading-snug">{d.summary}</p>
                </li>
              ))}
            </ul>
          </div>
        </div>
      </Section>

      {/* Signposts */}
      <Section id="ways-in" kicker="Where to go from here" title="Three ways in">
        <ul className="grid gap-6 md:grid-cols-3">
          {[
            {
              who: "Players",
              body: "There is nothing to download yet. Join the Discord to hear about playtests. The first public build goes on the Workshop after the private rounds.",
              to: DISCORD_URL,
              label: "Join the Discord",
            },
            {
              who: "Server owners",
              body: "What the framework gives you, what the flagship adds on top, how it is configured, and which licence covers which copy.",
              to: "/servers",
              label: "Read the brief",
            },
            {
              who: "Contributors",
              body: "Every piece of work is a GitHub issue with its files and its acceptance proof. Pick one, prove it in the game, open a pull request.",
              to: "/contribute",
              label: "Pick a scope",
            },
          ].map((s) => (
            <li key={s.who} className="sheet p-6 flex flex-col">
              <span className="plate text-xl self-start">{s.who}</span>
              <p className="mt-4 muted flex-1">{s.body}</p>
              <p className="mt-5">
                <GoLink to={s.to}>{s.label}</GoLink>
              </p>
            </li>
          ))}
        </ul>
        <p className="mt-10 text-sm muted">
          Life Framework continues the{" "}
          <a href="https://github.com/EveronLife/EveronLife" className="link">
            Everon Life
          </a>{" "}
          framework by Arkensor (MIT). The <code>EL_</code> class prefix is that heritage. See the{" "}
          <a href={blob("README.md")} className="link">
            README
          </a>{" "}
          for credits.
        </p>
      </Section>
    </>
  );
};

export default Home;
