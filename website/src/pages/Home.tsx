import { Link } from "react-router-dom";
import { Arrow, DiscordIcon, GitHubIcon } from "@/components/icons";
import { Figure, GoLink, RouteMap, ScopeMeter, Section, Stamp } from "@/components/ui";
import {
  CURRENT_MILESTONE,
  CURRENT_MILESTONE_NAME,
  DISCORD_URL,
  REPO_URL,
  blob,
  githubData,
  issuesUrl,
  milestone,
  milestoneProgress,
  siteData,
  still,
} from "@/lib/content";

const building = [
  {
    still: "arrival-clerk",
    title: "A town you arrive in",
    body: "You spawn on a village square, sign the register with the clerk, and walk off with an ID card in your pocket. Every building on the street has a sign that says what happens inside.",
  },
  {
    still: "atm-terminal",
    title: "Money you can lose",
    body: "Jobs pay cash, and cash is an item. Bank it at the ATM, or carry it and risk it: if you die or get robbed, what was in your pockets stays with your body.",
  },
  {
    still: "registry-form",
    title: "Keys, cards, and a fine on paper",
    body: "Your car key is an item someone can steal. A police fine is a ticket you carry to the station desk. Where a screen helps, it looks like a form or a terminal from 1989.",
  },
];

const different = [
  {
    n: "01",
    title: "Native, immersive, polished",
    body: "Feedback comes from the world: an item changing hands, a sound, a door that opens. A harness walks the street as a player and takes screenshots, so nothing counts as done until it plays.",
    more: { label: "The in-world rule", to: "/decisions#in-world-first" },
  },
  {
    n: "02",
    title: "AI-first tooling",
    body: "One rulebook for people and agents, issues written as ready-to-take scopes, a test suite that boots the game in about ten seconds, and one command that checks, builds, and opens your pull request. Bring your own AI agent; the repo is set up for it.",
    more: { label: "How a change ships", to: "/contribute" },
  },
  {
    n: "03",
    title: "A framework with a flagship",
    body: "Server owners host it and add jobs and shops without forking. Our own 1989 Everon server is the proof that it plays. The core knows nothing about 1989, so a modern setting runs on the same systems.",
    more: { label: "For server owners", to: "/servers" },
  },
];

const skills = ["Enforce Script", "Mapping and world building", "UI", "3D models and art", "Sound", "Testing", "Docs"];

const Home = () => {
  const m1 = milestone(CURRENT_MILESTONE);
  const { done, total } = milestoneProgress(CURRENT_MILESTONE);
  const m1Name = CURRENT_MILESTONE_NAME;
  const left = total - done;
  const unassigned = (m1?.issues ?? []).filter((i) => i.state === "open" && i.assignees.length === 0).length;
  const goodFirst = githubData.goodFirst.length;

  return (
    <>
      {/* Hero */}
      <div className="band border-b-2 border-ink">
        <div className="wrap pt-10 pb-14 md:pt-16 md:pb-20 grid gap-10 lg:grid-cols-[1.1fr_1fr] lg:items-center">
          <div>
            <img src="/brand/logo.svg" alt="" width={112} height={112} decoding="async" {...{ fetchpriority: "low" }} className="w-24 h-24 md:w-28 md:h-28 mb-6" />
            <p className="kicker mb-4">Open-source life and roleplay for Arma Reforger</p>
            <h1 className="font-display font-bold uppercase leading-[0.95] text-[clamp(2.5rem,7vw,4.75rem)] [text-wrap:balance]">
              We are building the modern Life Framework
            </h1>
            <p className="lede mt-6">
              Inspired by Altis Life, built for the next era. No janky menus: a town you play in, with real cash, keys in your
              pocket, and a fine on paper.
            </p>
            <div className="mt-8 flex flex-wrap gap-3">
              <a href={DISCORD_URL} className="btn btn-primary">
                <DiscordIcon className="w-4 h-4" /> Join the Discord
              </a>
              <Link to="/contribute" className="btn btn-sign">
                Start contributing <Arrow />
              </Link>
            </div>
            <p className="mt-6">
              <a href={REPO_URL} className="inline-flex items-center gap-2 font-mono text-[0.8125rem] uppercase tracking-[0.06em] link">
                <GitHubIcon className="w-4 h-4" /> The code on GitHub, MIT
              </a>
            </p>
          </div>
          <Figure still={still("spawn-square")} eager sizes="(min-width: 1024px) 540px, 100vw" />
        </div>
      </div>

      {/* What we're building */}
      <Section id="building" kicker="What we're building" title="Life in the world, not in a menu">
        <p className="lede muted mb-12">
          No janky UIs. We want life on Reforger to feel native, immersive, and polished. The things that matter are objects you can
          hold, give, lose, or have taken. Every picture here is a capture from the game.
        </p>
        <ul className="grid gap-10 md:grid-cols-3">
          {building.map((b) => (
            <li key={b.still} className="flex flex-col">
              <h3 className="display-3 mb-3">{b.title}</h3>
              <p className="text-[0.9875rem] muted mb-5">{b.body}</p>
              <Figure still={still(b.still)} className="mt-auto" sizes="(min-width: 768px) 33vw, 100vw" />
            </li>
          ))}
        </ul>
      </Section>

      {/* Why it's different */}
      <Section id="different" kicker="Why it's different" title="Built for the next era">
        <p className="lede muted mb-12">
          Altis Life showed what a life server on Arma can be: a town, a job, a police car, and a decade of stories. We keep that idea
          and change how it is built.
        </p>
        <ol className="grid gap-10 md:grid-cols-3">
          {different.map((r) => (
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
        <p className="mt-12 text-sm muted max-w-prose">
          Altis Life is the classic Arma 3 life mode. We are not affiliated with it and use none of its code or assets.
        </p>
      </Section>

      {/* Where we are */}
      <Section id="status" kicker="Where we are" title="Nothing to download yet">
        <div className="sheet p-5 md:p-8">
          <div className="flex flex-wrap items-center justify-between gap-3 mb-6">
            <h3 className="display-3">
              {CURRENT_MILESTONE} {m1Name}
            </h3>
            <Stamp>In development</Stamp>
          </div>
          <div className="grid gap-8 lg:grid-cols-[1fr_1.2fr]">
            <p className="muted">
              {left > 0 && left <= 5 ? "The playtest town is nearly done. " : "We are building the playtest town now. "}
              It is a signposted village where a stranger can play the whole loop on a dedicated server without asking anyone. Next
              come private playtests with friends, then the first public Workshop release.
            </p>
            <div>
              <p className="flex items-baseline justify-between gap-4 mb-2">
                <span className="font-display font-bold text-4xl leading-none">
                  {done} <span className="text-ink-2 text-2xl">of {total}</span>
                </span>
                <span className="kicker">scopes done</span>
              </p>
              {m1 && <ScopeMeter issues={m1.issues} label={`${CURRENT_MILESTONE} ${m1Name}`} />}
              <p className="mt-4 flex flex-wrap gap-x-6 gap-y-2">
                <GoLink to="/roadmap">The roadmap</GoLink>
                <GoLink to="/devlog">The devlog</GoLink>
              </p>
            </div>
          </div>
          <div className="hr mt-8 pt-8">
            <RouteMap current={CURRENT_MILESTONE} compact />
          </div>
        </div>
      </Section>

      {/* How you can help */}
      <Section id="help" kicker="How you can help" title="Pick what you enjoy">
        <ul className="grid gap-6 md:grid-cols-3">
          <li className="sheet p-6 flex flex-col">
            <h3 className="heading">Test what we build</h3>
            <p className="mt-2 text-[0.9375rem] muted flex-1">
              Play a session and tell us in <span className="font-mono text-ink">#feedback</span> on Discord what you did, what you
              expected, and what broke. That note often becomes the next fix.
            </p>
            <p className="mt-5 flex flex-wrap gap-x-6 gap-y-2">
              <GoLink to={DISCORD_URL}>#feedback on Discord</GoLink>
              <GoLink to={blob("docs/playtest.md")}>Playtest guide</GoLink>
            </p>
          </li>
          <li className="sheet p-6 flex flex-col">
            <h3 className="heading">Pick an issue</h3>
            <p className="mt-2 text-[0.9375rem] muted flex-1">
              Every piece of work is a GitHub issue that names its files and how to prove it. Say you are on it, open a pull request,
              and a maintainer reviews it with you.
              {unassigned > 0 && ` ${unassigned} ${CURRENT_MILESTONE} ${unassigned === 1 ? "scope has" : "scopes have"} nobody on ${unassigned === 1 ? "it" : "them"} right now.`}
            </p>
            <p className="mt-5 flex flex-wrap gap-x-6 gap-y-2">
              <GoLink to={issuesUrl("is:open is:issue no:assignee")}>Open issues</GoLink>
              <GoLink to={issuesUrl('is:open is:issue label:"good first issue"')}>
                {goodFirst > 0 ? `Good first issues (${goodFirst})` : "Good first issues"}
              </GoLink>
            </p>
          </li>
          <li className="sheet p-6 flex flex-col">
            <h3 className="heading">Bring a skill</h3>
            <p className="mt-2 text-[0.9375rem] muted">Most of this list is not code. We need people for:</p>
            <ul className="mt-4 flex flex-wrap gap-2 flex-1 content-start" aria-label="Skills we need">
              {skills.map((s) => (
                <li key={s} className="font-mono text-[0.75rem] uppercase tracking-[0.06em] border border-ink px-2 py-1">
                  {s}
                </li>
              ))}
            </ul>
            <p className="mt-5">
              <GoLink to="/contribute">Start contributing</GoLink>
            </p>
          </li>
        </ul>
        <div className="mt-10 border-l-4 border-sign pl-5 md:pl-6 max-w-3xl">
          <h3 className="kicker mb-1">Becoming a maintainer</h3>
          <p>
            Maintainers come from contributors: show steady commitment to the roadmap and land high-quality code through reviewed pull
            requests.
          </p>
        </div>
      </Section>

      {/* Closing */}
      <div className="band border-y-2 border-ink">
        <div className="wrap py-14 md:py-20 grid gap-8 lg:grid-cols-[1.4fr_1fr] lg:items-end">
          <div>
            <p className="kicker mb-4">Come build it with us</p>
            <p className="font-display font-bold uppercase leading-[1] text-[clamp(1.9rem,4.2vw,3rem)] [text-wrap:balance]">
              An ambitious community project, and we'd like you in it.
            </p>
            <p className="lede mt-5">
              We are looking forward to connecting, learning, and building together, and to making some lifelong friends along the
              way.
            </p>
          </div>
          <div className="flex flex-wrap gap-3 lg:justify-end">
            <a href={DISCORD_URL} className="btn btn-primary">
              <DiscordIcon className="w-4 h-4" /> Join the Discord
            </a>
            <Link to="/contribute" className="btn btn-ghost">
              Start contributing <Arrow />
            </Link>
          </div>
        </div>
      </div>

      <p className="wrap pt-10 text-sm muted">
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
    </>
  );
};

export default Home;
