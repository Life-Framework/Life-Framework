import { GoLink, PageHeader } from "@/components/ui";
import { HtmlBody } from "@/components/HtmlBody";
import { blob, githubData, type Doc } from "@/lib/content";
import { useData } from "@/lib/data";

// The body is docs/credits.md, rendered at build time, so this page cannot drift from it.
// The commit counts come from the GitHub contributors list.
const Credits = () => {
  const doc = useData<Doc>("doc-credits");
  const people = githubData.contributors;
  return (
    <>
      <PageHeader kicker="Credits" title="Built on other people's work">
        <p>
          Life Framework grew from Everon Life by Arkensor and borrows from Overthrow and LNGCLifeRP_Public. The game assets are
          Bohemia Interactive's. Everything below comes from the repository.
        </p>
      </PageHeader>
      <div className="wrap max-w-4xl">
        {!doc ? <p className="muted">Loading the credits.</p> : <HtmlBody html={doc.html} className="prose-lf" />}
        <section aria-labelledby="who" className="mt-14">
          <h2 id="who" className="display-2 rule-top pt-3 mb-6">
            Who has shipped code
          </h2>
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
        </section>
        <p className="mt-12 pt-6 border-t border-rule flex flex-wrap gap-x-6 gap-y-2">
          <GoLink to={blob("docs/credits.md")}>Source file</GoLink>
          <GoLink to={blob("THIRD_PARTY.md")}>Per-file third-party credits</GoLink>
        </p>
      </div>
    </>
  );
};

export default Credits;
