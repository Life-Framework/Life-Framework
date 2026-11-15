/* eslint-disable react-refresh/only-export-components -- upcoming and utcText are shared with the servers page link preview (#1615) */
import { useEffect, useState } from "react";
import { GoLink } from "@/components/ui";
import { DISCORD_URL } from "@/lib/content";

// The next flagship session, shared by /servers and /alpha. website/content/sessions.json is written by
// `tools\cli host announce` and checked against content/sessions.schema.json. It holds the next session time
// only (decision 2026-11-14): no player count, no up or down, and the page makes no network request for it.
// The file is optional, so a site built before the first announcement says that nothing is announced yet.
// A session whose time has passed is treated as none: the page never shows an old time.
// `announced` is the ISO time the post went out; a draft that was not posted has none and is not shown (#1537).
type NextSession = { at: string; build: string; wipe: boolean; announced?: string };
const sessionFiles = import.meta.glob<{ default: { next: NextSession | null } }>("../../content/sessions.json", { eager: true });
const sessionFile = Object.values(sessionFiles)[0]?.default;

const MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
const DAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
const two = (n: number) => String(n).padStart(2, "0");

// The server's zone is UTC. Fixed text, so the prerendered page and the first client render agree.
export const utcText = (d: Date) =>
  `${DAYS[d.getUTCDay()]} ${d.getUTCDate()} ${MONTHS[d.getUTCMonth()]} ${d.getUTCFullYear()}, ${two(d.getUTCHours())}:${two(d.getUTCMinutes())} UTC`;

// The visitor's own zone, with its name (CET, GMT+1, ...); only known in the browser, so it is set in an effect.
const localText = (d: Date) =>
  new Intl.DateTimeFormat(undefined, { weekday: "short", day: "numeric", month: "short", year: "numeric", hour: "2-digit", minute: "2-digit", timeZoneName: "short" }).format(d);

// The announced session, or null when none is announced, it is only a draft (no announced time), the file is
// missing or unreadable, or the time has passed.
export const upcoming = (now: number): { when: Date; session: NextSession } | null => {
  const session = sessionFile?.next;
  if (!session || typeof session.at !== "string" || typeof session.announced !== "string") return null;
  const when = new Date(session.at);
  if (Number.isNaN(when.getTime()) || when.getTime() <= now) return null;
  return { when, session };
};

// setTimeout takes a 32-bit delay; a session further out than this wakes once early and arms again.
const MAX_DELAY = 2 ** 31 - 1;

export const NextSessionPanel = () => {
  const [now, setNow] = useState(() => Date.now());
  const next = upcoming(now);
  const [local, setLocal] = useState("");
  const whenMs = next?.when.getTime();
  // Re-read the clock in the browser: the prerendered page is as old as the build.
  useEffect(() => setNow(Date.now()), []);
  useEffect(() => {
    if (whenMs === undefined) return;
    setLocal(localText(new Date(whenMs)));
    // A tab left open outlives the session time, so the panel flips to "none" when it passes.
    const timer = setTimeout(() => setNow(Date.now()), Math.min(Math.max(whenMs - Date.now(), 0) + 1000, MAX_DELAY));
    return () => clearTimeout(timer);
  }, [whenMs, now]);

  return (
    <div className="wrap" id="next-session">
      <div className="sheet p-5 md:p-8 mb-8">
        <p className="kicker">Flagship server</p>
        <h2 className="display-3 mt-1">Next session</h2>
        {next ? (
          <>
            <p className="mt-3 heading">
              <time dateTime={next.when.toISOString()}>{local || utcText(next.when)}</time>
            </p>
            {local && <p className="muted mt-1">Server time: {utcText(next.when)}</p>}
            <p className="muted text-[0.9375rem] mt-3">
              Build {next.session.build}. {next.session.wipe ? "The world starts wiped." : "The world carries on from the last session."}
            </p>
          </>
        ) : (
          <p className="mt-3 heading">No session announced yet: watch Discord</p>
        )}
        <p className="mt-4">
          <GoLink to={DISCORD_URL}>Sessions are announced on Discord</GoLink>
        </p>
      </div>
    </div>
  );
};
