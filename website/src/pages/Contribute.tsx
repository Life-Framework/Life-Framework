import { useState } from "react";
import { Link } from "react-router-dom";
import { GoLink, IssueLedger, PageHeader, Section, Stamp } from "@/components/ui";
import { CURRENT_MILESTONE, DISCORD_URL, REPO_URL, blob, githubData, issuesUrl, milestone } from "@/lib/content";

const steps = [
  {
    n: "1",
    title: "Join the Discord",
    body: (
      <>
        <a href={DISCORD_URL} className="link">
          Join our Discord
        </a>{" "}
        and say hello. Tell us what you would like to help with and we will find a first issue together. You do not need to be a
        programmer to join in.
      </>
    ),
  },
  {
    n: "2",
    title: "Pick something",
    body: (
      <>
        Choose a good first issue or an unclaimed M1 scope below. Each issue says what to change and how to prove it. Comment that you
        are taking it so we can coordinate.
      </>
    ),
  },
  {
    n: "3",
    title: "Build and playtest",
    body: (
      <>
        Follow the issue, work in your own worktree, and test the change in the game. The repo has a playtest town and a prompt below
        to get a coding agent set up.
      </>
    ),
  },
  {
    n: "4",
    title: "Raise a PR; ship it together",
    body: (
      <>
        Open a pull request with the evidence. The team reviews it with you, helps resolve feedback, and merges it when it is ready.
      </>
    ),
  },
];

const playtestPrompt = `I want to help with Life Framework. Prepare this checkout so I can open the current M1 playtest town. Do not make code changes yet.

1. Read AGENTS.md, CONTRIBUTING.md, docs/setup.md, and docs/playtest.md.
2. Run tools\\cli status and check the local Reforger and Workbench setup. Follow docs/setup.md for local paths; do not commit machine-specific settings.
3. Check tools\\cli playtest status. If the playtest is stopped, run tools\\cli playtest and wait until the server is ready and the game is connected. Do not start a second instance.
4. Tell me how to register and suggest a few things to try in the town. Keep the checkout unchanged and wait for me to choose an issue.
5. Once I choose an issue, help me coordinate in Discord, use a named worktree, follow the issue's proof requirements, and open a PR for the team to review and merge.`;

function CopyPromptButton({ text }: { text: string }) {
  const [state, setState] = useState<"idle" | "copied" | "failed">("idle");

  const copy = async () => {
    if (!navigator.clipboard) {
      setState("failed");
      return;
    }

    try {
      await navigator.clipboard.writeText(text);
      setState("copied");
    } catch {
      setState("failed");
    }
  };

  return (
    <div className="flex items-center gap-3">
      <button type="button" className="btn btn-primary" onClick={copy}>
        {state === "copied" ? "Prompt copied" : "Copy prompt"}
      </button>
      <span className="text-sm muted" role="status" aria-live="polite">
        {state === "failed" ? "Select the prompt and copy it." : ""}
      </span>
    </div>
  );
}

const Contribute = () => {
  const m1 = milestone(CURRENT_MILESTONE);
  const unassigned = (m1?.issues ?? []).filter((i) => i.state === "open" && i.assignees.length === 0);
  const pick = githubData.goodFirst.length > 0 ? githubData.goodFirst : unassigned;
  const people = githubData.contributors;

  return (
    <>
      <PageHeader kicker="Contribute" title="Pick a scope. Prove it in the game.">
        <p>
          Join us on Discord, choose a piece of work, and build it with the team. People and coding agents are both welcome. We will
          help you get the game open, review your pull request, and get your first change merged.
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

      <Section id="reviewers" kicker="Grow with the team" title="From contributor to approver">
        <p className="lede muted max-w-3xl">
          After a few solid pull requests have landed and the team has seen your reviews, we can grant approval and merge access.
          We want that step to happen quickly: trusted contributors help review and merge the next changes, so good work keeps moving.
        </p>
      </Section>

      <Section id="playtest-prompt" kicker="Open the town" title="Bring a coding agent to the playtest">
        <p className="lede muted max-w-3xl mb-6">
          Paste this into your coding agent from the repository root. It will check the local setup, open the playtest, and leave the
          checkout ready for you to choose an issue.
        </p>
        <div className="terminal p-5 md:p-6">
          <div className="flex flex-wrap items-center justify-between gap-4 mb-4">
            <p className="font-mono text-[0.75rem] uppercase tracking-[0.08em] opacity-70">Setup and open the playtest</p>
            <CopyPromptButton text={playtestPrompt} />
          </div>
          <pre className="font-mono text-[0.8125rem] leading-relaxed whitespace-pre-wrap">{playtestPrompt}</pre>
        </div>
      </Section>

      <Section id="agents" kicker="AI-first" title="Build with your agent">
        <div className="grid gap-10 lg:grid-cols-[1fr_1.1fr]">
          <div className="space-y-4 muted">
            <p>
              Claude Code reads <code>CLAUDE.md</code>, which points at <code>AGENTS.md</code>. opencode, Codex, and Cursor read{" "}
              <code>AGENTS.md</code> directly. Skills in <code>.claude/skills/</code> add the workflows. Once you have picked an issue,
              give your agent its number and the{" "}
              <a href={blob(".claude/skills/agent-scope/SKILL.md")} className="link">
                agent-scope
              </a>{" "}
              skill. It covers the path from a fresh worktree through a reviewed pull request.
            </p>
            <p>
              A pull request is not the finish line. Stay in the conversation, fix review notes, and learn the team's review standards.
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
        kicker={githubData.goodFirst.length > 0 ? "Good first issues" : `${CURRENT_MILESTONE} scopes nobody is on`}
        title="Open now"
        aside={<Stamp>{pick.length} open</Stamp>}
      >
        {pick.length === 0 ? (
          <p className="muted">
            Nothing is unclaimed right now. Check the <Link to="/roadmap" className="link">current plan</Link> or ask on{" "}
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
          <GoLink to={issuesUrl("is:open is:issue label:m1 no:assignee")}>Unassigned M1 scopes</GoLink>
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
