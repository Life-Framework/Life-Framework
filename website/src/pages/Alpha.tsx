import { useEffect, useState, type ReactNode } from "react";
import { NextSessionPanel } from "@/components/NextSession";
import { GoLink, PageHeader, Section, Stamp } from "@/components/ui";
import { DISCORD_URL, blob } from "@/lib/content";

// The invite-only alpha page (#1468), written from docs/community/alpha.md (#555). The server password is the whole
// gate: this page says how to ask for it and never holds it. The next session time comes from NextSession.tsx
// (time only, no player count or live status). The wipe notice and the opening gate are the doc's own words.

const SERVER_NAME = "Life Framework | Everon 1989 (Alpha)";

// The wipe notice (#1616). `host announce --wipe --send` writes `next.wipe: true` and `announced` to
// content/sessions.json (#1413, #1549); the same rule as the next-session panel applies: a row that is not
// announced, or whose time has passed, shows nothing. The wipe policy gives 24 hours' notice on Discord.
type SessionRow = { at?: unknown; wipe?: unknown; announced?: unknown };
const sessionFiles = import.meta.glob<{ default: { next: SessionRow | null } }>("../../content/sessions.json", { eager: true });
const sessionRow = Object.values(sessionFiles)[0]?.default?.next;

const MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
const DAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];

// The date of the announced wipe session in the server's zone (UTC), or null when there is no notice to show.
// Fixed text, so the prerendered page and the first client render agree.
const wipeDate = (now: number): { iso: string; text: string } | null => {
  if (!sessionRow || sessionRow.wipe !== true || typeof sessionRow.announced !== "string" || typeof sessionRow.at !== "string") return null;
  const d = new Date(sessionRow.at);
  if (Number.isNaN(d.getTime()) || d.getTime() <= now) return null;
  return { iso: d.toISOString(), text: `${DAYS[d.getUTCDay()]} ${d.getUTCDate()} ${MONTHS[d.getUTCMonth()]} ${d.getUTCFullYear()}` };
};

const WipeNotice = () => {
  const [now, setNow] = useState(() => Date.now());
  // The prerendered page is as old as the build: re-read the clock in the browser.
  useEffect(() => setNow(Date.now()), []);
  const wipe = wipeDate(now);
  if (!wipe) return null;
  return (
    <div className="wrap" id="wipe-notice">
      <div className="sheet p-5 md:p-8 mb-8" role="note">
        <p className="kicker">Wipe announced</p>
        <h2 className="display-3 mt-1">
          The world is wiped before the session on <time dateTime={wipe.iso}>{wipe.text}</time>
        </h2>
        <p className="muted mt-3">
          The announcement went out on Discord with at least 24 hours' notice. A wipe clears all progress, so do not count on
          anything you built before it. Read the <GoLink to={blob("docs/community/wipe-policy.md")}>wipe policy</GoLink>.
        </p>
      </div>
    </div>
  );
};

const asking: { title: string; body: ReactNode }[] = [
  {
    title: "Join the Discord",
    body: (
      <>
        Everything about the alpha happens there. <GoLink to={DISCORD_URL}>Life Framework Discord</GoLink>
      </>
    ),
  },
  {
    title: "Tell staff you want in",
    body: "Post in #general or message a Community Manager. Say your Steam name, because your character uses it.",
  },
  {
    title: "Install the game and the mod",
    body: (
      <>
        Do this first. The <GoLink to="/playtest">playtest page</GoLink> covers the download and the first join.
      </>
    ),
  },
  {
    title: "Wait for your invite",
    body: "A staff member sends it in a direct message, with the server password. If you do not hear back, ask again in #general. Invites go out in small batches so the server is not full on the first night.",
  },
];

const gate: { title: string; body: string }[] = [
  { title: "Sessions at the player target", body: "Several sessions in a row fill the server's 16 slots." },
  { title: "No save loss", body: "No session lost or corrupted a save, and a restart brought every player's progress back." },
  { title: "No blocking regression", body: "No open regression stops a player from joining, playing a job, or keeping progress." },
  { title: "Trial moderators in place", body: "The staff roster names them, chosen from the playtesters, before the public opening." },
];

const Alpha = () => (
  <>
    <PageHeader kicker="Invite-only alpha" title="Join the Everon 1989 alpha">
      <p>
        The flagship server is open to a few people first. It is called <strong>{SERVER_NAME}</strong> in the server browser. The
        password is the whole gate: there is no account list and no code to redeem. Ask for an invite and staff send you the
        password.
      </p>
    </PageHeader>

    <WipeNotice />
    <NextSessionPanel />

    <Section id="invite" kicker="Getting in" title="Asking for an invite">
      <ol className="grid gap-8 md:grid-cols-2 lg:grid-cols-4 list-none p-0 max-w-6xl">
        {asking.map((s, i) => (
          <li key={s.title}>
            <p className="kicker">Step {i + 1}</p>
            <h3 className="heading mb-2">{s.title}</h3>
            <p className="muted">{s.body}</p>
          </li>
        ))}
      </ol>
      <p className="mt-8 max-w-prose muted">
        The invite carries a password for this server only. Staff never ask for your Steam login, and the password is never
        posted in public. Please do not share it: ask staff if a friend wants in.
      </p>
    </Section>

    <Section id="expect" kicker="What to expect" title="An alpha, in sessions">
      <div className="sheet p-6 md:p-8 grid gap-6 md:grid-cols-[auto_1fr] md:items-center max-w-4xl">
        <Stamp tilt>Alpha</Stamp>
        <div className="space-y-3">
          <p>
            The server runs in sessions announced on Discord, not around the clock. The save can be wiped at any time with at least
            24 hours' notice on Discord, and a wipe clears all progress.
          </p>
          <p className="muted">
            Read the <GoLink to={blob("docs/community/wipe-policy.md")}>wipe policy</GoLink> before you invest time in a long build.
            Report what broke in #playtest-feedback.
          </p>
        </div>
      </div>
    </Section>

    <Section id="open" kicker="The opening gate" title="When it opens to everyone">
      <p className="max-w-prose muted mb-6">
        The alpha opens to the public when all four hold. The maintainer then removes the password and announces it on Discord.
        Progress is kept from that day.
      </p>
      <ul className="border-t-2 border-ink text-[0.9375rem] max-w-3xl">
        {gate.map((g) => (
          <li key={g.title} className="py-2.5 border-b border-rule">
            <span className="font-semibold">{g.title}</span>
            <span className="block muted">{g.body}</span>
          </li>
        ))}
      </ul>
    </Section>

    <Section id="own" kicker="Not on our server" title="Run your own">
      <p className="max-w-prose muted">
        Life Framework is open source. You do not need an invite to host it: install the dedicated server, set your own rules and
        economy, and keep the save across restarts.
      </p>
      <p className="mt-4 flex flex-wrap gap-x-6 gap-y-2">
        <GoLink to={blob("docs/owner/install.md")}>Run your own server: install guide</GoLink>
        <GoLink to="/servers">For server owners</GoLink>
      </p>
    </Section>
  </>
);

export default Alpha;
