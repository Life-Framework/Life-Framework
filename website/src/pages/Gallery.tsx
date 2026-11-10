import { AnchorLink, GoLink, PageHeader, Section, Stamp } from "@/components/ui";
import { useData } from "@/lib/data";

type Picture = { id: string; src: string; srcSet: string; width: number; height: number; caption: string };
type GalleryData = {
  sections: { id: string; title: string; blurb: string; issues: { number: number; title: string; url: string; pictures: Picture[] }[] }[];
  areas: { id: string; number: number; title: string; pairs: { name: string; before: Picture; after: Picture }[]; singles: Picture[] }[];
  count: number;
};

const SIZES = "(min-width: 1024px) 380px, (min-width: 640px) 50vw, 100vw";

// "Medic" -> "medic", "The village" -> "the-village": the anchor for an epic section.
const slug = (title: string) => title.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
// An area pair's anchor comes from its before-file, "area-1-spawn-square/detail-before" -> "area-1-spawn-square-detail".
const pairAnchor = (pic: Picture) => pic.id.replace(/-before$/, "").replace("/", "-");

function Shot({ pic, sizes = SIZES }: { pic: Picture; sizes?: string }) {
  return (
    <figure className="photo">
      <img src={pic.src} srcSet={pic.srcSet} sizes={sizes} width={pic.width} height={pic.height} alt={pic.caption} loading="lazy" decoding="async" className="h-auto" />
      <figcaption>{pic.caption}</figcaption>
    </figure>
  );
}

const Gallery = () => {
  const data = useData<GalleryData>("gallery");
  return (
    <>
      <PageHeader kicker="Gallery" title="Captured in the game">
        <p>
          Pictures taken in the playtest village while each system was built, grouped by the part of M2 they belong to. Each group
          links to the GitHub issue it came from. Shots with debug overlays or a cluttered screen are left out.
        </p>
      </PageHeader>
      {!data ? (
        <div className="wrap pb-16">
          <p className="muted">Loading the gallery.</p>
        </div>
      ) : (
        <>
          {data.sections.map((s) => (
            <Section key={s.id} id={slug(s.title)} kicker="M2 Living Town" title={<>{s.title}<AnchorLink id={slug(s.title)} label={s.title} /></>}>
              {s.blurb && <p className="lede muted mb-10 max-w-3xl">{s.blurb}</p>}
              <div className="grid gap-12">
                {s.issues.map((issue) => (
                  <div key={issue.number}>
                    <h3 id={`issue-${issue.number}`} className="kicker mb-4">
                      {issue.title} <span className="text-ink-2">·</span>{" "}
                      <a href={issue.url} className="link">
                        #{issue.number}
                      </a>
                      <AnchorLink id={`issue-${issue.number}`} label={issue.title} />
                    </h3>
                    <div className="grid gap-6 sm:grid-cols-2 lg:grid-cols-3">
                      {issue.pictures.map((p) => (
                        <Shot key={p.id} pic={p} />
                      ))}
                    </div>
                  </div>
                ))}
              </div>
            </Section>
          ))}
          <Section id="areas" kicker="Area polish" title={<>Before and after<AnchorLink id="areas" label="before and after" /></>} aside={<Stamp tone="ink">Work in progress</Stamp>}>
            <p className="lede muted mb-10 max-w-3xl">
              The village is being polished one area at a time. Each finished area adds its before and after shots here when its pull
              request merges.
            </p>
            {data.areas.length === 0 ? (
              <p className="muted">No area has landed yet.</p>
            ) : (
              <div className="grid gap-14">
                {data.areas.map((a) => (
                  <div key={a.id} id={a.id}>
                    <h3 className="kicker mb-4">
                      Area {a.number} <span className="text-ink-2">·</span> {a.title}
                      <AnchorLink id={a.id} label={`area ${a.number}, ${a.title}`} />
                    </h3>
                    <div className="grid gap-10">
                      {a.pairs.map((pair) => (
                        <div key={pair.name} id={pairAnchor(pair.before)} className="grid gap-6 md:grid-cols-2">
                          <Shot pic={pair.before} sizes="(min-width: 768px) 50vw, 100vw" />
                          <Shot pic={pair.after} sizes="(min-width: 768px) 50vw, 100vw" />
                        </div>
                      ))}
                      {a.singles.length > 0 && (
                        <div className="grid gap-6 sm:grid-cols-2 lg:grid-cols-3">
                          {a.singles.map((p) => (
                            <Shot key={p.id} pic={p} />
                          ))}
                        </div>
                      )}
                    </div>
                  </div>
                ))}
              </div>
            )}
          </Section>
          <div className="wrap pb-16 flex flex-wrap gap-x-8 gap-y-3">
            <GoLink to="/features">What each system does</GoLink>
            <GoLink to="/roadmap">Roadmap</GoLink>
          </div>
        </>
      )}
    </>
  );
};

export default Gallery;
