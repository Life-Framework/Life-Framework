import { GoLink, PageHeader } from "@/components/ui";
import { useData } from "@/lib/data";

type Picture = { id: string; src: string; srcSet: string; width: number; height: number; caption: string };
type GalleryData = { pictures: Picture[]; count: number };

const SIZES = "(min-width: 1024px) 380px, (min-width: 640px) 50vw, 100vw";

const Gallery = () => {
  const data = useData<GalleryData>("gallery");
  return (
    <>
      <PageHeader kicker="Gallery" title="Captured in the game">
        <p>A few pictures from the playtest village, taken in the game with nothing added.</p>
      </PageHeader>
      <div className="wrap pb-16">
        {!data ? (
          <p className="muted">Loading the gallery.</p>
        ) : (
          <div className="grid gap-x-6 gap-y-10 sm:grid-cols-2 lg:grid-cols-3">
            {data.pictures.map((p, i) => (
              <figure key={p.id} className="photo">
                <img
                  src={p.src}
                  srcSet={p.srcSet}
                  sizes={SIZES}
                  width={1280}
                  height={720}
                  alt={p.caption}
                  loading={i < 3 ? "eager" : "lazy"}
                  decoding="async"
                  className="aspect-video w-full object-cover"
                />
                <figcaption>{p.caption}</figcaption>
              </figure>
            ))}
          </div>
        )}
      </div>
      <div className="wrap pb-16 flex flex-wrap gap-x-8 gap-y-3">
        <GoLink to="/features">What each system does</GoLink>
        <GoLink to="/roadmap">Roadmap</GoLink>
      </div>
    </>
  );
};

export default Gallery;
