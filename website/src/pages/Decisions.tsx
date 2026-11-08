import { GoLink, PageHeader, Stamp } from "@/components/ui";
import { HtmlBody } from "@/components/HtmlBody";
import { REPO_URL, blob, formatDate, type Decisions as DecisionsData } from "@/lib/content";
import { useData } from "@/lib/data";

const Decisions = () => {
  const data = useData<DecisionsData>("decisions");
  return (
    <>
      <PageHeader kicker="Decisions" title="The calls that set the taste">
        <p>
          Each entry is a design call someone could reasonably have made the other way, with the date, why, and what it means for
          the code. To argue with one, open an issue. Do not quietly build the alternative.
        </p>
      </PageHeader>
      {!data ? (
        <p className="wrap muted">Loading the decisions.</p>
      ) : (
        <div className="wrap grid gap-12 lg:grid-cols-[14rem_1fr]">
          <nav aria-label="Sections" className="lg:sticky lg:top-28 self-start">
            <p className="kicker mb-3">Sections</p>
            <ul className="space-y-1 border-t-2 border-ink">
              {data.sections.map((s) => (
                <li key={s.id} className="border-b border-rule">
                  <a href={`#${s.id}`} className="flex justify-between py-2 font-display font-bold uppercase text-xl hover:text-green">
                    {s.title}
                    <span className="font-mono text-sm font-normal text-ink-2 pt-1">{s.entries.length}</span>
                  </a>
                </li>
              ))}
            </ul>
            <p className="mt-6 flex flex-col gap-2">
              <GoLink to={blob("docs/decisions.md")}>Source file</GoLink>
              <GoLink to={`${REPO_URL}/issues/new/choose`}>Open an issue</GoLink>
            </p>
          </nav>
          <div className="min-w-0">
            <HtmlBody html={data.introHtml} className="prose-lf muted mb-4" />
            {data.sections.map((s) => (
              <section key={s.id} aria-labelledby={s.id} className="mt-14 first:mt-8">
                <h2 id={s.id} className="display-2 rule-top pt-3 mb-6">
                  {s.title}
                </h2>
                <ol className="space-y-6">
                  {s.entries.map((e) => (
                    <li key={e.id} id={e.id} className={`sheet p-5 md:p-7 scroll-mt-28 ${e.superseded ? "!bg-inset" : ""}`}>
                      <div className="flex flex-wrap items-center justify-between gap-3 mb-3">
                        <time dateTime={e.date} className="kicker">
                          Filed {formatDate(e.date)}
                        </time>
                        <span className="flex gap-2">
                          {e.amended && <Stamp tone="ink">Amended {formatDate(e.amended, { day: "numeric", month: "short" })}</Stamp>}
                          {e.superseded && <Stamp>Superseded</Stamp>}
                        </span>
                      </div>
                      <h3 className="display-3">
                        <a href={`#${e.id}`} className="hover:text-green">
                          {e.title}
                        </a>
                      </h3>
                      <HtmlBody html={e.html} className="prose-lf mt-3 text-[0.9875rem]" />
                    </li>
                  ))}
                </ol>
              </section>
            ))}
          </div>
        </div>
      )}
    </>
  );
};

export default Decisions;
