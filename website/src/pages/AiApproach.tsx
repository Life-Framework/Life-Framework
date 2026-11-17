import { GoLink, PageHeader } from "@/components/ui";
import { REPO_URL, blob } from "@/lib/content";

// Copy follows the AI Software Factory write-up (figures are from 7 to 17 November 2026) and the
// repo files each section links to. Update both when the process changes.

const loop = [
  {
    n: "01",
    owner: "Person",
    title: "Set the goal",
    body: "Choose what should change for players, and write down how we will tell that it works.",
  },
  {
    n: "02",
    owner: "AI agents",
    title: "Build it in small pieces",
    body: "A lead agent hands each small issue to a worker agent. The worker edits code or game files in its own copy of the project and runs the checks.",
  },
  {
    n: "03",
    owner: "Checks",
    title: "Prove it",
    body: "Repository checks, in-game tests and pictures run on every change. After a merge, a guard compiles and tests main again.",
  },
  {
    n: "04",
    owner: "Person",
    title: "Play it",
    body: "A person plays the build. Only that person's verdict closes an issue.",
  },
];

const agents = [
  { name: "lf-scout", tier: "Small, cheap model", budget: "30 tool calls", takes: "Lookups, searches, summaries of one file" },
  { name: "lf-builder", tier: "Mid-sized model", budget: "80 tool calls", takes: "One scoped issue, from test to pull request" },
  { name: "lf-hard", tier: "Largest model", budget: "80 tool calls", takes: "Design, saving and networking bugs, risky reviews, work the mid tier failed twice" },
];

const ingredients = [
  {
    n: "01",
    name: "One contract for people and agents",
    detail:
      "AGENTS.md holds the repo map, the golden rules, the commands, what done means, and the engine mistakes we have already paid for. Claude Code, Codex, Cursor and opencode all read it.",
    path: "AGENTS.md",
  },
  {
    n: "02",
    name: "The goal and the design calls",
    detail:
      "Direction describes the kind of shared roleplay world we want. Decisions record why we chose one design over another, such as a paper ticket over an automatic bank charge.",
    path: "docs/direction.md · docs/decisions.md",
  },
  {
    n: "03",
    name: "Small issues with a finish line",
    detail:
      "Each issue is sized for one agent in under an hour. It names the files it owns, so two issues that run at once never edit the same file, and it names its acceptance check before any code exists.",
    path: "docs/process/slice-card.md",
  },
  {
    n: "04",
    name: "One-page starter packs and skills",
    detail:
      "A starter pack lists the exact commands and known traps for one kind of work, so an agent does not spend its first fifteen minutes reading. Skills describe how to look up the engine, test, and review a risky change.",
    path: "docs/starter-packs/ · .claude/skills/",
  },
];

const evidence = [
  {
    label: "Script tests",
    title: "Do the rules work?",
    body: "Each test is one LF_Test class with a tier: LOGIC runs on an empty world in about ten seconds, WORLD loads a resource, PERSISTENCE does a save round trip, PERF is a timing gate. Every new test is seen failing once before it counts, so we know it can fail.",
    tool: "tools\\cli test --no-build --only <LF_Test_Class>",
  },
  {
    label: "Server tests",
    title: "Does it work online and after a restart?",
    body: "A dedicated server is the game host without a player's screen. The save and reload proof stops the server, starts it again, and checks the data. A passing run on a hosting player's own game proves nothing about a dedicated server.",
    tool: "tools\\cli wt e2e <slug>",
  },
  {
    label: "Pictures",
    title: "Does it look right?",
    body: "Anything a player can see is looked at before it ships. One command opens a hidden editor, takes a fixed set of pictures in about 25 seconds, and needs no game session. Each picture records which version of the files it rendered, so a before and after pair from the same files is refused.",
    tool: "tools\\cli look <area>",
  },
  {
    label: "Player tests",
    title: "Can someone use it?",
    body: "The agent box drives a real game client. It walks to a shop, uses the interaction, clicks the menu, reads the result and can film the run as a video. It is kept for what a still picture cannot show: menus, motion, a second player, a late joiner.",
    tool: "node agent-box/harness.mjs flow <name>",
  },
];

const bottlenecks = [
  { name: "Model quota", moved: "Three model tiers, a call budget with a handoff, one blocking wait instead of polling, and pacing to the week." },
  { name: "RAM", moved: "Jobs go to the machine with the most free memory, admitted by the measured peak of each kind of job." },
  { name: "Disk", moved: "A disk guard, a safe prune command, and cleanup of run folders." },
  { name: "The shared editor", moved: "Only one program can compile scripts and render pictures, so most iteration moved to offline tools and a warm render server." },
  { name: "Human review", moved: "One planned playtest hour, pictures shown inline, and a default pick when nobody answers." },
];

const toc = [
  ["How the work flows", "the-loop"],
  ["How the project gives context", "environment"],
  ["How agents stay out of each other's way", "isolation"],
  ["How we check a change", "proof"],
  ["What people decide", "people"],
  ["What was hard", "bottlenecks"],
  ["What other mod makers can reuse", "pattern"],
];

function WorkLoop() {
  return (
    <figure className="wrap" aria-labelledby="loop-caption">
      <div className="sheet p-4 md:p-7">
        <div className="flex flex-wrap items-baseline justify-between gap-2 border-b-2 border-ink pb-3">
          <p className="kicker">One issue, start to finish</p>
          <p className="font-mono text-[0.7rem] uppercase tracking-[0.06em] text-ink-2">A person sets the goal and plays the result</p>
        </div>
        <ol className="mt-4 grid gap-px border border-rule bg-rule md:grid-cols-4">
          {loop.map((step, index) => (
            <li key={step.n} className="relative min-w-0 bg-card p-4 md:p-5">
              <div className="flex items-center justify-between gap-2">
                <span className="font-mono text-sm text-stamp">{step.n}</span>
                <span className={`kicker ${step.owner === "Person" ? "text-green" : ""}`}>{step.owner}</span>
              </div>
              <p className="display-3 mt-4">{step.title}</p>
              <p className="mt-2 text-sm muted leading-snug">{step.body}</p>
              {index < loop.length - 1 && (
                <span aria-hidden="true" className="absolute -right-3 top-1/2 z-10 hidden h-6 w-6 -translate-y-1/2 items-center justify-center border border-ink bg-sign font-mono text-sm text-on-sign md:flex">
                  →
                </span>
              )}
            </li>
          ))}
        </ol>
        <figcaption id="loop-caption" className="mt-4 flex flex-wrap items-start gap-x-3 gap-y-1 text-sm muted">
          <span className="font-mono text-stamp" aria-hidden="true">↶</span>
          A failure becomes a new issue, a better check, or a clearer rule for the next agent.
        </figcaption>
      </div>
    </figure>
  );
}

const AiApproach = () => (
  <>
    <PageHeader kicker="Building with AI · How we work" title="AI can build the feature. People decide what belongs.">
      <p>
        Life Framework is an open-source roleplay mod for Arma Reforger: jobs, money, police, property and more, which server owners
        can combine and configure. Between 7 and 17 November 2026 its repository took in about 1,280 merged pull requests, almost all
        written by AI coding agents. One person, one lead agent and a rotating group of 10 to 20 short-lived worker agents did it.
      </p>
      <p className="muted">
        An AI coding agent is a language model in a loop. It reads files, runs commands, edits code, and decides what to do next until
        it finishes or runs out of budget. People choose the goals, look at what players will see, and play the build. The
        <strong> harness</strong> is everything around the agents: the project contract, the commands, the checks and the budgets.
      </p>
    </PageHeader>

    <WorkLoop />

    <div className="wrap pt-14 md:pt-20 grid gap-12 lg:grid-cols-[13rem_minmax(0,1fr)]">
      <aside className="self-start lg:sticky lg:top-28" aria-label="Article contents">
        <p className="kicker mb-3">In this article</p>
        <nav aria-label="On this page">
          <ol className="border-t-2 border-ink">
            {toc.map(([label, id], index) => (
              <li key={id} className="border-b border-rule">
                <a href={`#${id}`} className="flex gap-3 py-2 text-[0.9375rem] hover:text-green">
                  <span className="font-mono text-[0.75rem] text-ink-2">0{index + 1}</span>
                  <span>{label}</span>
                </a>
              </li>
            ))}
          </ol>
        </nav>
        <p className="mt-6">
          <GoLink to={blob("CONTRIBUTING.md#working-with-coding-agents")}>Read the contributor guide</GoLink>
        </p>
      </aside>

      <article className="min-w-0 max-w-4xl">
        <section id="the-loop" aria-labelledby="the-loop-title" className="scroll-mt-28">
          <p className="kicker mb-2">01 · How the work flows</p>
          <h2 id="the-loop-title" className="display-2 rule-top pt-3">A lead dispatches small jobs to workers</h2>
          <div className="prose-lf mt-5">
            <p>
              The <strong>person</strong> is the product owner. They decide what the game should be, review anything a player will
              see, and play the build before anything is called verified. They are not in the loop for routine merges.
            </p>
            <p>
              The <strong>lead</strong> is one long-lived agent. It reads a one-screen dispatch view (open issues by state, open pull
              requests with the conflicting ones first, which worktrees are idle, free memory and disk on each machine, model quota
              against the week), starts workers, and reads each worker's five-line report once. It does not write code.
            </p>
            <p>
              A <strong>worker</strong> gets one job, its own checkout and a tool-call budget, then ends with a report. The agent
              types are files in <code>.claude/agents/</code>, each with a model pinned.
            </p>
          </div>
          <div className="sheet mt-6 overflow-x-auto">
            <table className="w-full min-w-[34rem] text-left text-[0.9375rem]">
              <thead>
                <tr className="border-b-2 border-ink">
                  <th className="kicker p-3">Agent</th>
                  <th className="kicker p-3">Model</th>
                  <th className="kicker p-3">Budget</th>
                  <th className="kicker p-3">Takes</th>
                </tr>
              </thead>
              <tbody>
                {agents.map((a) => (
                  <tr key={a.name} className="border-b border-rule align-top">
                    <td className="p-3"><code>{a.name}</code></td>
                    <td className="p-3">{a.tier}</td>
                    <td className="p-3">{a.budget}</td>
                    <td className="p-3 muted">{a.takes}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          <div className="prose-lf mt-6">
            <p>
              The budgets exist because every turn re-reads the whole context. Context starts near 45,000 tokens and grows about
              1,600 per turn. In the ten costliest runs, 94 to 99 percent of the cost came after the context passed 150,000. An
              agent at its budget commits, writes a short handoff file and ends with a <code>RESUME</code> line, and the lead starts
              a fresh agent from it. After two failed attempts at the same problem, the mid tier stops and the lead restarts the job
              on the largest model with that report as the brief.
            </p>
            <p>
              Shipping is inside the budget. An agent that has green tests and is near its limit ships what it has and lists the
              checks it skipped under a <code>## Manual testing</code> heading.
            </p>
            <p>
              Work is cut into <strong>issues</strong> by a planner agent. The planning tool refuses an issue over 45 minutes, an
              issue with no acceptance check, risky work on a cheap model, a dependency cycle, and two issues that could run at once
              and own the same file. An issue that runs over its budget is split, never extended.
            </p>
          </div>
          <p className="mt-4 flex flex-wrap gap-x-6 gap-y-2 text-[0.9375rem]">
            <a href={blob("docs/agent-budget.md")} className="link">Read the agent budget</a>
            <a href={blob("docs/process/roadmap-pipeline.md")} className="link">Read the roadmap pipeline</a>
          </p>
        </section>

        <section id="environment" aria-labelledby="environment-title" className="mt-16 scroll-mt-28">
          <p className="kicker mb-2">02 · How the project gives context</p>
          <h2 id="environment-title" className="display-2 rule-top pt-3">The repository is the shared memory</h2>
          <p className="mt-5 max-w-prose muted">
            An agent starts with no knowledge of this project, and a long one pays for everything it has read. So the decisions live in
            files that every person and agent reads first, and a brief is the issue number, the files and the acceptance test.
          </p>
          <ol className="mt-6 border-t-2 border-ink">
            {ingredients.map((item) => (
              <li key={item.n} className="grid gap-2 border-b border-rule py-4 sm:grid-cols-[3.5rem_1fr_auto] sm:gap-4">
                <span className="font-mono text-sm text-stamp">{item.n}</span>
                <div>
                  <h3 className="heading">{item.name}</h3>
                  <p className="mt-1 text-[0.9375rem] muted">{item.detail}</p>
                </div>
                <code className="self-start text-[0.75rem] text-ink-2 sm:text-right">{item.path}</code>
              </li>
            ))}
          </ol>
          <div className="prose-lf mt-6">
            <p>
              The engine has no debugger and no hot reload, and agents must not guess its API. They look it up. <code>tools\cli api
              class &lt;Name&gt;</code> prints the real declaration from the installed game, and <code>tools\cli call</code> reaches
              the asset search and game file tools through MCP, a standard way for an assistant to ask other programs for
              information. This is rule one of working in Reforger: read the game's own data instead of inventing a plausible name.
            </p>
          </div>
          <p className="mt-4 flex flex-wrap gap-x-6 gap-y-2 text-[0.9375rem]">
            <a href={blob("AGENTS.md")} className="link">Read AGENTS.md</a>
            <a href={blob("docs/direction.md")} className="link">Read the direction</a>
            <a href={blob("docs/decisions.md")} className="link">Read the decisions</a>
            <a href={blob("docs/starter-packs/slice.md")} className="link">See a starter pack</a>
          </p>
        </section>

        <section id="isolation" aria-labelledby="isolation-title" className="mt-16 scroll-mt-28">
          <p className="kicker mb-2">03 · How agents stay out of each other's way</p>
          <h2 id="isolation-title" className="display-2 rule-top pt-3">Own copy, own files, own cell</h2>
          <div className="prose-lf mt-5">
            <p>
              Every worker starts with <code>tools\cli wt new &lt;slug&gt;</code>. It makes a <strong>worktree</strong>, a second
              working directory on its own branch with its own server ports, so twenty agents can edit at once. A new one is claimed
              from a pool of ready checkouts in about two seconds. The maintainer's own copy of main is the world editor, and heavy
              commands refuse to run there.
            </p>
            <p>
              Conflicts used to be the daily blocker, because every feature appended a line to the same few central lists. Now{" "}
              <strong>shared registries are per-feature files</strong>: a reset hook, a bus event, a proof verb and a harness verb
              each live in the feature that owns them, and <code>validate-registries</code> enforces it. Generated and append-only
              files, such as the string table and the test registry, have merge drivers that keep rows both sides added.
            </p>
            <p>
              The game world is a file two agents cannot both edit, so it is zoned. New features go in a fenced debug grid of
              12 metre cells, one feature per cell, written only through <code>tools\cli grid place</code>. Only a worktree whose name
              starts with <code>town-</code> edits the town, and the town goes up as a draft with pictures for a person to approve.
            </p>
          </div>
          <p className="mt-4 flex flex-wrap gap-x-6 gap-y-2 text-[0.9375rem]">
            <a href={blob("docs/worktrees.md")} className="link">Read about worktrees</a>
            <a href={blob("docs/architecture/world-zones.md")} className="link">Read about world zones</a>
          </p>
        </section>

        <section id="proof" aria-labelledby="proof-title" className="mt-16 scroll-mt-28">
          <p className="kicker mb-2">04 · How we check a change</p>
          <h2 id="proof-title" className="display-2 rule-top pt-3">A green build is only the first check</h2>
          <p className="mt-5 max-w-prose muted">
            Checks are ordered by cost. The cheap ones run constantly, and the expensive ones run once at the end.
            <code> tools\cli validate</code> runs the repository checks whose inputs changed, and GitHub runs all of them on every
            pull request. The in-game suite and the real-path proofs come next.
          </p>
          <ol className="mt-7 grid gap-5 md:grid-cols-2">
            {evidence.map((layer) => (
              <li key={layer.label} className="sheet flex flex-col p-5">
                <p className="kicker">{layer.label}</p>
                <h3 className="display-3 mt-3">{layer.title}</h3>
                <p className="mt-3 flex-1 text-[0.9375rem] muted">{layer.body}</p>
                <code className="mt-5 block border-t border-rule pt-3 text-[0.6875rem] leading-relaxed text-ink-2">{layer.tool}</code>
              </li>
            ))}
          </ol>

          <div className="prose-lf mt-8">
            <h3>Merge on cheap checks, guard after the merge</h3>
            <p>
              Until 15 November every change ran a full local gate first. A one-value config change cost four to five minutes, and
              the queue backed up. We deleted that gate. <code>tools\cli wt ship</code> now checks the pull request body, runs a
              merge preflight and <code>validate</code> on the changed files, pushes, opens the pull request and turns on auto-merge.
              GitHub's <code>validate</code> is the only check before the merge.
            </p>
            <p>
              The <strong>main guard</strong> does the heavy work afterwards. On each new head of main it compiles the scripts first.
              A compile error is bisected to the merge that caused it and reverted at once. A second stage builds, runs the full
              suite, audits placements and does a two-boot save and reload proof.
            </p>
            <p>
              The guard was wrong at first. In its first 48 hours it opened six revert pull requests, and two were false. Its bisect
              called a commit red if any test failed there, so an older merge that broke other tests got blamed for a new failure.
              It now records which tests failed and reruns only those at each step. When no single merge explains the failures, it
              logs &quot;mixed causes&quot; and reverts nothing.
            </p>
          </div>

          <div className="mt-6 border-l-4 border-sign bg-inset p-4 md:p-5">
            <p className="font-semibold">Merged is not done.</p>
            <p className="mt-1 text-[0.9375rem] muted">
              A merged change is labelled <code>in-dev</code>. If its pull request lists manual steps it is <code>needs-playtest</code>.
              Only a person marks it <code>verified</code>, and only that closes the issue. The restart path, a second player, a late
              joiner and a real dedicated server are where the bugs hide, and no agent sees them. A playtest checklist collects those
              steps into one planned hour.
            </p>
          </div>

          <div className="prose-lf mt-8">
            <h3>Failures become checks</h3>
            <p>
              A rule that keeps getting broken becomes a script in <code>tools/validation/</code>. Phones hovered above desks in the
              first shop fit-outs, so a check now snaps prop heights to the real surface and fails a floating prop in about a second.
              A false crash detector matched &quot;Navmesh Init&quot;, so it gets a self-test. Risky areas such as saving, networking,
              money and inventory also get an independent adversarial review before merge. On one issue that review found three real
              bugs that a green suite had missed.
            </p>
          </div>
        </section>

        <section id="people" aria-labelledby="people-title" className="mt-16 scroll-mt-28">
          <p className="kicker mb-2">05 · What people decide</p>
          <h2 id="people-title" className="display-2 rule-top pt-3">People choose what good roleplay feels like</h2>
          <div className="prose-lf mt-5">
            <p>
              Imagine a police officer fines a player. The software could subtract money from the player's bank, or give them a paper
              ticket to carry to the station desk and pay. Both work in code. A person decides which one makes a better moment, and
              the agents build and test that choice.
            </p>
            <p>
              People also catch what tests cannot. After one person called an arrival desk immersion-breaking, the rule became that
              town and arrival work goes up as a draft pull request with pictures, and nothing merges until a person says so. They
              look for the cases a task missed: the player cannot afford the fine, the officer disconnects halfway, the server
              restarts before the ticket is paid.
            </p>
            <p>
              Human review is now the slowest step. On 16 November, 521 issues were merged and waiting for a playtest, and nine draft
              pull requests were waiting for a person's eye. The factory makes &quot;merged, in development&quot;. A person makes
              &quot;verified&quot;.
            </p>
          </div>
          <div className="mt-7 grid gap-px border border-rule bg-rule sm:grid-cols-2">
            <div className="bg-card p-5">
              <p className="kicker">The test for a new player</p>
              <p className="display-3 mt-3">Can they find their way?</p>
              <p className="mt-3 text-[0.9375rem] muted">Can they join, understand the place, finish a useful activity, and see what happened without a developer beside them?</p>
            </div>
            <div className="bg-card p-5">
              <p className="kicker">The test for other server owners</p>
              <p className="display-3 mt-3">Can they make it their own?</p>
              <p className="mt-3 text-[0.9375rem] muted">Can another community change its setting, prices, jobs and rules without rewriting the framework?</p>
            </div>
          </div>
        </section>

        <section id="bottlenecks" aria-labelledby="bottlenecks-title" className="mt-16 scroll-mt-28">
          <p className="kicker mb-2">06 · What was hard</p>
          <h2 id="bottlenecks-title" className="display-2 rule-top pt-3">The bottleneck keeps moving</h2>
          <p className="mt-5 max-w-prose muted">
            Each limit below was the binding one for a day or two. We measured it, fixed it, and the next one showed up. The first
            milestone, built before any of this existed, used up the whole model subscription in three days.
          </p>
          <ol className="mt-6 border-t-2 border-ink">
            {bottlenecks.map((b, i) => (
              <li key={b.name} className="grid gap-1 border-b border-rule py-3 sm:grid-cols-[3rem_11rem_1fr] sm:gap-4">
                <span className="font-mono text-sm text-stamp">0{i + 1}</span>
                <h3 className="heading">{b.name}</h3>
                <p className="text-[0.9375rem] muted">{b.moved}</p>
              </li>
            ))}
          </ol>
          <div className="prose-lf mt-6">
            <p>
              The one idea behind most of the fixes is to iterate where it is cheap. A game boot costs 10 to 300 seconds and up to a
              gigabyte of memory. The map emulator renders the in-game map in 0.3 seconds, and the building check judges prop heights
              in about a second without opening the engine. Do many cheap iterations and one expensive proof at the end.
            </p>
          </div>
        </section>

        <section id="pattern" aria-labelledby="pattern-title" className="mt-16 scroll-mt-28">
          <p className="kicker mb-2">07 · What other mod makers can reuse</p>
          <h2 id="pattern-title" className="display-2 rule-top pt-3">Use the same idea for your own project</h2>
          <p className="mt-5 max-w-prose muted">
            Our game and tools are specific to Reforger. The process is not. If you start, start here:
          </p>
          <ol className="mt-5 grid gap-3 sm:grid-cols-2">
            <li className="border border-rule bg-card p-4">
              <span className="kicker">01 · Write the contract</span>
              <p className="mt-2">One file at the repository root with the repo map, the rules, the commands and what done means.</p>
            </li>
            <li className="border border-rule bg-card p-4">
              <span className="kicker">02 · Cut small issues</span>
              <p className="mt-2">Under an hour each, with the acceptance check written first and the files they own listed.</p>
            </li>
            <li className="border border-rule bg-card p-4">
              <span className="kicker">03 · Isolate every agent</span>
              <p className="mt-2">One worktree each, its own ports, and no central list that every feature has to edit.</p>
            </li>
            <li className="border border-rule bg-card p-4">
              <span className="kicker">04 · Turn repeats into checks</span>
              <p className="mt-2">A mistake that happens twice becomes a script that runs on every change.</p>
            </li>
            <li className="border border-rule bg-card p-4">
              <span className="kicker">05 · Iterate on something cheap</span>
              <p className="mt-2">Find your expensive real thing, build a fast stand-in for it, and prove on the real thing once.</p>
            </li>
            <li className="border border-rule bg-card p-4">
              <span className="kicker">06 · Keep a human verdict</span>
              <p className="mt-2">Merged is one label, verified is another, and only a person sets the second.</p>
            </li>
          </ol>
          <div className="mt-8 border-t-2 border-ink pt-5 flex flex-col gap-5 sm:flex-row sm:items-end sm:justify-between">
            <div>
              <p className="kicker mb-2">The ambition</p>
              <blockquote className="display-3 max-w-2xl">The most played and most built-upon mod in Arma's history.</blockquote>
              <p className="mt-3 max-w-2xl text-[0.9375rem] muted">
                We get there by being the most playable, the most provable, and the easiest to contribute to, in that order.
              </p>
            </div>
            <div className="flex flex-wrap gap-x-6 gap-y-3 sm:shrink-0">
              <GoLink to="/contribute">Start contributing</GoLink>
              <GoLink to={`${REPO_URL}/blob/main/agent-box/README.md`}>See the game-test tools</GoLink>
            </div>
          </div>
        </section>
      </article>
    </div>
  </>
);

export default AiApproach;
