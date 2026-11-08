import { Link } from "react-router-dom";
import { PageHeader, SourceNote } from "@/components/ui";
import { formatDate, type Devlog as DevlogData, type DevlogEntry } from "@/lib/content";
import { useData } from "@/lib/data";

function byDay(entries: DevlogEntry[]): [string, DevlogEntry[]][] {
  const days = new Map<string, DevlogEntry[]>();
  for (const e of entries) {
    const d = e.date.slice(0, 10);
    if (!days.has(d)) days.set(d, []);
    days.get(d)!.push(e);
  }
  return [...days.entries()];
}

const Devlog = () => {
  const data = useData<DevlogData>("devlog");
  return (
    <>
      <PageHeader kicker="Devlog" title="What changed, day by day">
        <p>
          Every pull request merged into <code>main</code> and every design decision recorded in the repo, newest first. Written
          from the pull requests themselves when the site is built.
        </p>
      </PageHeader>
      <div className="wrap">
        {!data ? (
          <p className="muted">Loading the devlog.</p>
        ) : (
          <>
            <SourceNote source={data.source} fetchedAt={data.fetchedAt} />
            <ol className="mt-8">
              {byDay(data.entries).map(([day, entries]) => {
                const pulls = entries.filter((e) => e.kind === "pr");
                const decisions = entries.filter((e) => e.kind === "decision");
                return (
                  <li key={day} className="grid gap-x-10 gap-y-4 md:grid-cols-[11rem_1fr] border-t-2 border-ink pt-4 pb-12">
                    <h2 className="md:sticky md:top-28 self-start">
                      <span className="font-display font-bold uppercase text-3xl leading-none block">
                        {formatDate(day, { day: "numeric", month: "short" })}
                      </span>
                      <span className="kicker">
                        {formatDate(day, { weekday: "long" })} {day.slice(0, 4)}
                      </span>
                    </h2>
                    <div className="min-w-0">
                      {decisions.length > 0 && (
                        <div className="sheet p-5 mb-6">
                          <p className="kicker mb-3">
                            {decisions.length} decision{decisions.length === 1 ? "" : "s"} recorded
                          </p>
                          <ul className="space-y-2 text-[0.9875rem]">
                            {decisions.map((d) => (
                              <li key={d.href} className="grid grid-cols-[1rem_1fr] gap-2">
                                <span aria-hidden="true" className="text-stamp">
                                  ■
                                </span>
                                <Link to={d.href} className="link">
                                  {d.title}
                                </Link>
                              </li>
                            ))}
                          </ul>
                        </div>
                      )}
                      <ul>
                        {pulls.map((p) => (
                          <li key={p.href} className="py-3 border-b border-rule last:border-b-0 grid gap-x-4 sm:grid-cols-[4.5rem_1fr]">
                            <span className="font-mono text-[0.8125rem] text-ink-2 pt-0.5">#{p.number}</span>
                            <span className="min-w-0">
                              <a href={p.href} className="heading link">
                                {p.title}
                              </a>
                              {p.summary && <span className="block mt-1 text-[0.9375rem] muted leading-snug">{p.summary}</span>}
                            </span>
                          </li>
                        ))}
                      </ul>
                    </div>
                  </li>
                );
              })}
            </ol>
          </>
        )}
      </div>
    </>
  );
};

export default Devlog;
