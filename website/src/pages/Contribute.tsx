import { Link } from "react-router-dom";
import { GoLink, IssueLedger, PageHeader, Section, Stamp } from "@/components/ui";
import { CURRENT_MILESTONE, DISCORD_URL, REPO_URL, blob, githubData, issuesUrl, milestone } from "@/lib/content";

const steps = [
  {
    n: "1",
    title: "Read the contract",
    body: (
      <>
        <a href={blob("AGENTS.md")} className="link">
          AGENTS.md
        </a>{" "}
        is the one rulebook, for people and agents alike: the repo map, the golden rules, the commands, and the engine lessons each
        paid for with a real bug. Set up the toolchain with{" "}
        <a href={blob("docs/setup.md")} className="link">
          docs/setup.md
        </a>
        .
      </>
    ),
  },
  {
    n: "2",
    title: "Take a scope",
    body: (
      <>
        Work is a GitHub issue written as a scope: the files it touches, how to prove it, and no hidden context. Take an unassigned
        one from the list below and comment that you are on it.
      </>
    ),
  },
  {
    n: "3",
    title: "Prove it in the game",
    body: (
      <>
        Work in your own worktree (<code>tools\cli wt new &lt;slug&gt;</code>). Write a test that fails first, make it pass, then drive
        the real path in the world. A test that cannot go red proves nothing.
      </>
    ),
  },
  {
    n: "4",
    title: "Ship a pull request",
    body: (
      <>
        <code>tools\cli wt ship</code> validates, builds, runs the full suite, and opens the pull request. Say what changed, how it
        stays in the world, the red and green evidence, and any steps a person has to check by hand.
      </>
    ),
  },
];

const Contribute = () => {
  const m1 = milestone(CURRENT_MILESTONE);
  const unassigned = (m1?.issues ?? []).filter((i) => i.state === "open" && i.assignees.length === 0);
  const pick = githubData.goodFirst.length > 0 ? githubData.goodFirst : unassigned;
  const people = githubData.contributors;

  return (
    <>
      <PageHeader kicker="Contribute" title="Pick a scope. Prove it in the game.">
        <p>
          The mod, the docs, and this site live in one repository, and every change is a pull request. The repo assumes the
          contributor is an AI agent working beside other agents, and that a person can read and reason about all of it. Both are
          welcome.
        </p>
      </PageHeader>

      <div className="wrap">
        <ol className="grid gap-px bg-rule border border-rule md:grid-cols-2 xl:grid-cols-4">
          {steps.map((s) => (
            <li key={s.n} className="bg-card p-6">
              <span className="font-display font-bold text-5xl leading-none text-stamp" aria-hidden="true">
                {s.n}
              </span>
              <h2 className="display-3 mt-3 mb-2">{s.title}</h2>
              <p className="text-[0.9375rem] muted">{s.body}</p>
            </li>
          ))}
        </ol>
      </div>

      <Section id="agents" kicker="AI-first" title="Bring your agent">
        <div className="grid gap-10 lg:grid-cols-[1fr_1.1fr]">
          <div className="space-y-4 muted">
            <p>
              Claude Code reads <code>CLAUDE.md</code>, which points at <code>AGENTS.md</code>. opencode, Codex, and Cursor read{" "}
              <code>AGENTS.md</code> directly. Skills in <code>.claude/skills/</code> add the workflows. Give an agent an issue number
              and the{" "}
              <a href={blob(".claude/skills/agent-scope/SKILL.md")} className="link">
                agent-scope
              </a>{" "}
              skill, and it has the whole path from worktree to merged pull request.
            </p>
            <p>
              Agents improve their own tools. When a session is slowed by a missing validator, a gap in the harness, or a wrong doc,
              fixing that is part of the work. Each pull request names the friction it hit.
            </p>
            <p>
              Never guess an Enfusion API. Look it up through the MCP tools (<code>tools\cli call api_search</code>) before writing
              script.
            </p>
          </div>
          <div className="terminal p-5 md:p-6 overflow-x-auto">
            <p className="font-mono text-[0.75rem] uppercase tracking-[0.08em] opacity-70 mb-3">One scope, start to finish</p>
            <pre className="font-mono text-[0.8125rem] leading-relaxed">
              {`tools\\cli wt new m1-19-paper-fine     # your own worktree and ports
tools\\cli validate                     # cheap repo checks, run often
tools\\cli test --no-build --tier fast  # the in-game suite, ~10 s
node agent-box/harness.mjs flow <name> # walk it as a player
tools\\cli wt ship m1-19-paper-fine --closes 104`}
            </pre>
          </div>
        </div>
      </Section>

      <Section
        id="scopes"
        kicker={githubData.goodFirst.length > 0 ? "Good first issues" : `${CURRENT_MILESTONE} issues nobody is on`}
        title="Open now"
        aside={<Stamp>{pick.length} open</Stamp>}
      >
        {pick.length === 0 ? (
          <p className="muted">
            Nothing is unclaimed right now. Check the <Link to="/roadmap" className="link">roadmap</Link> or ask on{" "}
            <a href={DISCORD_URL} className="link">
              Discord
            </a>{" "}
            what needs a hand.
          </p>
        ) : (
          <IssueLedger issues={pick} />
        )}
        <p className="mt-6 flex flex-wrap gap-x-6 gap-y-2">
          <GoLink to={issuesUrl('is:open is:issue label:"good first issue"')}>Good first issues</GoLink>
          <GoLink to={issuesUrl(`is:open is:issue label:${CURRENT_MILESTONE.toLowerCase()} no:assignee`)}>
            Unassigned {CURRENT_MILESTONE} issues
          </GoLink>
        </p>
      </Section>

      <Section id="other" kicker="Not only code" title="Other ways to help">
        <ul className="grid gap-6 md:grid-cols-3">
          <li className="sheet p-6">
            <h3 className="heading">Play a session</h3>
            <p className="mt-2 text-[0.9375rem] muted">
              Run the test world on your own PC and write down what you did, what you expected, and what broke. That note becomes the
              next agent's prompt.
            </p>
            <p className="mt-4">
              <GoLink to={blob("docs/playtest.md")}>Playtest guide</GoLink>
            </p>
          </li>
          <li className="sheet p-6">
            <h3 className="heading">Report a bug</h3>
            <p className="mt-2 text-[0.9375rem] muted">
              Use the bug or playtest template on GitHub. Say where you were standing, what you pressed, and what happened.
            </p>
            <p className="mt-4">
              <GoLink to={`${REPO_URL}/issues/new/choose`}>Open an issue</GoLink>
            </p>
          </li>
          <li className="sheet p-6">
            <h3 className="heading">Fix the docs or this site</h3>
            <p className="mt-2 text-[0.9375rem] muted">
              Both are built from the repo. Every page under Docs has an edit link, and the site lives in <code>website/</code>.
            </p>
            <p className="mt-4">
              <GoLink to={blob("website/README.md")}>Site README</GoLink>
            </p>
          </li>
        </ul>
      </Section>

      <Section id="people" kicker="Credits" title="Who has shipped code">
        <ul className="flex flex-wrap gap-3">
          {people.map((p) => (
            <li key={p.login}>
              <a href={p.url} className="sheet inline-flex items-baseline gap-3 px-4 py-2.5 hover:bg-inset">
                <span className="font-mono">{p.login}</span>
                <span className="font-mono text-[0.75rem] text-ink-2">{p.contributions} commits</span>
              </a>
            </li>
          ))}
        </ul>
        <p className="mt-6 text-[0.9375rem] muted max-w-prose">
          Counted by GitHub from the commit history. Most agent-written commits carry the name of the person who ran the agent. Life Framework
          started as a continuation of{" "}
          <a href="https://github.com/EveronLife/EveronLife" className="link">
            Everon Life
          </a>{" "}
          by Arkensor and its contributors.
        </p>
      </Section>
    </>
  );
};

export default Contribute;
