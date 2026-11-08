import { Link, useParams } from "react-router-dom";
import { HtmlBody } from "@/components/HtmlBody";
import { GoLink, PageHeader } from "@/components/ui";
import { docs, groupDocs, type Doc } from "@/lib/content";
import { useData } from "@/lib/data";
import NotFound from "@/pages/NotFound";

export const DocsIndex = () => (
  <>
    <PageHeader kicker="Docs" title="The paperwork">
      <p>
        The plan, the design calls, and the scopes, rendered from <code>docs/</code> in the repository every time the site is built.
        What you read here is what the agents read.
      </p>
    </PageHeader>
    <div className="wrap space-y-14">
      {groupDocs(docs).map(([group, list]) => (
        <section key={group} aria-labelledby={`g-${group}`}>
          <h2 id={`g-${group}`} className="display-2 rule-top pt-3 mb-6">
            {group}
          </h2>
          <ul className="grid gap-x-10 md:grid-cols-2">
            {list.map((d) => (
              <li key={d.slug} className="border-b border-rule py-4">
                <Link to={d.href} className="heading link">
                  {d.title}
                </Link>
                <span className="block font-mono text-[0.75rem] text-ink-2 mt-0.5">{d.repoPath}</span>
                {d.description && <span className="block mt-1.5 text-[0.9375rem] muted leading-snug">{d.description}</span>}
              </li>
            ))}
          </ul>
        </section>
      ))}
    </div>
  </>
);

function Sidebar({ current }: { current: string }) {
  return (
    <nav aria-label="All docs" className="text-[0.9375rem]">
      {groupDocs(docs).map(([group, list]) => (
        <div key={group} className="mb-6">
          <p className="kicker mb-2">{group}</p>
          <ul className="border-l-2 border-rule">
            {list.map((d) => (
              <li key={d.slug}>
                <Link
                  to={d.href}
                  aria-current={d.slug === current ? "page" : undefined}
                  className={`block pl-3 py-1 -ml-[2px] border-l-2 leading-snug ${
                    d.slug === current ? "border-stamp font-semibold" : "border-transparent muted hover:text-ink hover:border-ink"
                  }`}
                >
                  {d.title}
                </Link>
              </li>
            ))}
          </ul>
        </div>
      ))}
    </nav>
  );
}

export const DocView = () => {
  const { slug = "" } = useParams();
  const doc = useData<Doc>(`doc-${slug}`);
  if (doc === null) return <NotFound />;
  return (
    <div className="wrap pt-10 md:pt-14 grid gap-12 lg:grid-cols-[15rem_minmax(0,1fr)] xl:grid-cols-[15rem_minmax(0,1fr)_13rem]">
      <aside className="hidden lg:block lg:sticky lg:top-28 self-start max-h-[calc(100vh-8rem)] overflow-y-auto pr-2">
        <Sidebar current={slug} />
      </aside>
      <article className="min-w-0">
        {doc === undefined ? (
          <p className="muted">Loading.</p>
        ) : (
          <>
            <p className="kicker mb-3">
              <Link to="/docs" className="link">
                Docs
              </Link>{" "}
              · {doc.group}
            </p>
            <h1 className="display-2 mb-3">{doc.title}</h1>
            <p className="font-mono text-[0.75rem] text-ink-2 mb-8 flex flex-wrap gap-x-5 gap-y-1">
              <a href={doc.sourceUrl} className="link">
                {doc.repoPath}
              </a>
              <a href={doc.editUrl} className="link">
                Edit on GitHub
              </a>
            </p>
            <HtmlBody html={doc.html} className="prose-lf" />
            <p className="mt-16 pt-6 border-t border-rule flex flex-wrap gap-x-6 gap-y-2">
              <GoLink to={doc.editUrl}>Suggest a change</GoLink>
              <GoLink to="/docs">All docs</GoLink>
            </p>
          </>
        )}
      </article>
      {doc && doc.headings.length > 2 && (
        <aside className="hidden xl:block xl:sticky xl:top-28 self-start max-h-[calc(100vh-8rem)] overflow-y-auto">
          <p className="kicker mb-2">On this page</p>
          <ul className="text-sm space-y-1.5">
            {doc.headings.map((h) => (
              <li key={h.id}>
                <a href={`#${h.id}`} className="muted hover:text-ink leading-snug block">
                  {h.text}
                </a>
              </li>
            ))}
          </ul>
        </aside>
      )}
    </div>
  );
};
