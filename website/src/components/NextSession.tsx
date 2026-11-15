import { useEffect, useState } from "react";
import { GoLink } from "@/components/ui";
import { DISCORD_URL } from "@/lib/content";

// The next flagship session, shared by /servers and /alpha. website/content/sessions.json is written by
// `tools\cli host announce` and checked against content/sessions.schema.json. It holds the next session time
// only (decision 2026-11-14): no player count, no up or down, and the page makes no network request for it.
// The file is optional, so a site built before the first announcement says that nothing is scheduled.
type NextSession = { at: string; build: string; wipe: boolean; announced: string };
const sessionFiles = import.meta.glob<{ default: { next: NextSession | null } }>("../../content/sessions.json", { eager: true });
const sessionFile = Object.values(sessionFiles)[0]?.default;

const MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
const DAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
const two = (n: number) => String(n).padStart(2, "0");

// Fixed UTC text, so the prerendered page and the first client render agree; the viewer's own time follows in an effect.
const utcText = (d: Date) =>
  `${DAYS[d.getUTCDay()]} ${d.getUTCDate()} ${MONTHS[d.getUTCMonth()]} ${d.getUTCFullYear()}, ${two(d.getUTCHours())}:${two(d.getUTCMinutes())} UTC`;

const localText = (d: Date) =>
  new Intl.DateTimeFormat(undefined, { weekday: "short", day: "numeric", month: "short", hour: "2-digit", minute: "2-digit", timeZoneName: "short" }).format(d);

// The announced session, or null when none is scheduled, the file is missing or unreadable, or the time has passed.
const upcoming = (): { when: Date; session: NextSession } | null => {
  const session = sessionFile?.next;
  if (!session || typeof session.at !== "string") return null;
  const when = new Date(session.at);
  if (Number.isNaN(when.getTime()) || when.getTime() < Date.now()) return null;
  return { when, session };
};

export const NextSessionPanel = () => {
  const next = upcoming();
  const [local, setLocal] = useState("");
  const whenMs = next?.when.getTime();
  useEffect(() => {
    if (whenMs !== undefined) setLocal(localText(new Date(whenMs)));
  }, [whenMs]);

  return (
    <div className="wrap" id="next-session">
      <div className="sheet p-5 md:p-8 mb-8">
        <p className="kicker">Flagship server</p>
        <h2 className="display-3 mt-1">Next session</h2>
        {next ? (
          <>
            <p className="mt-3 heading">
              <time dateTime={next.when.toISOString()}>{utcText(next.when)}</time>
            </p>
            {local && <p className="muted mt-1">Your time: {local}</p>}
            <p className="muted text-[0.9375rem] mt-3">
              Build {next.session.build}. {next.session.wipe ? "The world starts wiped." : "The world carries on from the last session."}
            </p>
          </>
        ) : (
          <p className="mt-3 muted">No session is scheduled. The next one shows here once it is announced.</p>
        )}
        <p className="mt-4">
          <GoLink to={DISCORD_URL}>Sessions are announced on Discord</GoLink>
        </p>
      </div>
    </div>
  );
};
