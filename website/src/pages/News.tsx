import { Link } from "react-router-dom";
import { GoLink, PageHeader, Section } from "@/components/ui";
import { PLAYTEST_DOWNLOAD_URL } from "@/lib/release";

// What is new in each playtest build, in player words. Newest first. Add a new entry at the top
// when a build is released; the pictures are in content/gallery.json (`featured`), which is what
// converts them to webp, and they come from website/public/docs-images/.

type Shot = { id: string; caption: string };
type Entry = {
  build: number;
  tag: string;
  date: string;
  intro: string;
  added: { title: string; text: string }[];
  fixed: string[];
  shots: Shot[];
};

const ENTRIES: Entry[] = [
  {
    build: 6,
    tag: "playtest-m2-6",
    date: "14 November 2026",
    intro:
      "Build 6 adds a bank vault you can open, a fuel station robbery, a rebuilt market and a lot of new objects in your hands. Download it from the playtest page; your launcher tells you when the server is on a newer build than yours.",
    added: [
      {
        title: "A strong room behind the bank",
        text: "The bank now has a vault with a heavy round door that swings open and shut.",
      },
      {
        title: "Rob the fuel station",
        text: "Walk up to the clerk with a weapon out and talk to them to start a robbery. Keep the clerk covered and stay near the till. The robbery stops if you leave, put the weapon away, go down, or the police arrive.",
      },
      {
        title: "An alarm for each robbery",
        text: "Each robbery now sets off its own alarm.",
      },
      {
        title: "People dressed for the job",
        text: "Townspeople now wear what suits their work: the farmer in work clothes, the police in uniform, the fuel clerk behind the till. Press F to talk to them.",
      },
      {
        title: "A rebuilt market",
        text: "The market on the square has new stalls, umbrellas and benches, and a juicer on one of the tables.",
      },
      {
        title: "The police case board",
        text: "The police now have a case board for the cases they are working.",
      },
      {
        title: "The taser works on people in town",
        text: "The taser now brings townspeople down.",
      },
      {
        title: "The ambulance siren",
        text: "The ambulance now has a siren.",
      },
      {
        title: "A practice patient for solo medics",
        text: "A medic playing alone now has a practice patient to treat.",
      },
      {
        title: "New 3D models",
        text: "These items now have their own models in your hands and on the ground: handcuffs, taser, spike strip, strongbox, lockpick, pager, evidence marker, money bag, juicer, and ore and ingots.",
      },
    ],
    fixed: [
      "Cash now looks like money in your hands and on the ground.",
      "The car HUD is fixed.",
      "The store counter is fixed.",
      "Parcels are fixed.",
    ],
    shots: [
      { id: "bank-vault-interior/vault-open-after", caption: "The vault door at the side of the bank, swung open." },
      { id: "polish-vault-market/bank-roadmap-bank-vault-after", caption: "The vault door closed, seen from the front." },
      { id: "polish-vault-market/market-ob180-after", caption: "The rebuilt market on the square, seen from above." },
      { id: "924/juicer-closeup-after", caption: "The new juicer on a market table." },
      { id: "npc-outfits/talk-after", caption: "Press F to talk to the farmer, who is dressed for the job." },
      { id: "925/cash-van-box-after", caption: "The new strongbox beside the cash van, with an evidence marker on the grass." },
      { id: "ore-rocks-natural/gold-after", caption: "Ore rocks at the quarry, with a gold vein through the stone." },
    ],
  },
];

const SIZES = "(min-width: 1024px) 380px, (min-width: 640px) 50vw, 100vw";

const News = () => (
  <>
    <PageHeader kicker="News" title="What is new in each playtest build">
      <p>
        A plain list of what changed for players in each playtest build. Every merged change is in the{" "}
        <Link to="/devlog" className="link">devlog</Link>.
      </p>
    </PageHeader>
    {ENTRIES.map((e) => (
      <Section
        key={e.tag}
        id={`build-${e.build}`}
        kicker={e.date}
        title={`Playtest M2, build ${e.build}`}
        aside={
          <a href={PLAYTEST_DOWNLOAD_URL} className="btn btn-download" download>
            Download the playtest
          </a>
        }
      >
        <p className="max-w-3xl">{e.intro}</p>

        <h3 className="heading mt-10 mb-4">New in build {e.build}</h3>
        <ul className="grid gap-x-10 gap-y-6 md:grid-cols-2 max-w-6xl">
          {e.added.map((a) => (
            <li key={a.title}>
              <h4 className="font-semibold">{a.title}</h4>
              <p className="mt-1">{a.text}</p>
            </li>
          ))}
        </ul>

        <h3 className="heading mt-10 mb-4">Fixed</h3>
        <ul className="list-disc pl-5 space-y-1.5 max-w-3xl">
          {e.fixed.map((f) => (
            <li key={f}>{f}</li>
          ))}
        </ul>

        <h3 className="heading mt-10 mb-4">In the game</h3>
        <div className="grid gap-x-6 gap-y-10 sm:grid-cols-2 lg:grid-cols-3">
          {e.shots.map((p, i) => {
            const base = `/gallery/${p.id}`;
            return (
              <figure key={p.id} className="photo">
                <img
                  src={`${base}-1280.webp`}
                  srcSet={`${base}-640.webp 640w, ${base}-1280.webp 1280w`}
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
            );
          })}
        </div>
      </Section>
    ))}
    <div className="wrap pb-16 flex flex-wrap gap-x-8 gap-y-3">
      <GoLink to="/playtest">Download and install</GoLink>
      <GoLink to="/gallery">Gallery</GoLink>
      <GoLink to="/features">What each system does</GoLink>
    </div>
  </>
);

export default News;
