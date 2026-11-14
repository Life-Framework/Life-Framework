import { Link } from "react-router-dom";
import { GoLink, PageHeader, Section } from "@/components/ui";
import { PLAYTEST_DOWNLOAD_URL } from "@/lib/release";

// What is new in each playtest build, in player words. Newest first. Add a new entry at the top
// when a build is released; the pictures are in content/gallery.json (`featured` or `usedByPages`),
// which is what converts them to webp, and they come from website/public/docs-images/.

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
      "Build 6 adds a bank vault you can open, a fuel station robbery, a market under red canopies and a lot of new objects in your hands. Download it from the playtest page; your launcher tells you when the server is on a newer build than yours.",
    added: [
      {
        title: "A strong room behind the bank",
        text: "The bank now has a vault with a heavy round door that swings open and shut. The front matches the bank's light plaster, with a corrugated hood and a caged lamp over the door, and the open door stands clear of the drainpipe.",
      },
      {
        title: "Rob the fuel station",
        text: "Walk up to the clerk with a weapon out and talk to them to start a robbery. Keep the clerk covered and stay near the till. The robbery stops if you leave, put the weapon away, go down, or the police arrive.",
      },
      {
        title: "An alarm for each robbery",
        text: "The bank vault rings a loud bell you hear across the street, the fuel till a shrill pulsing alarm, the store till a slow buzzer, and the cash van honks. The armory uses the bank bell, and crimes with no kind keep the old siren. Nobody on duty makes the alarm louder and longer.",
      },
      {
        title: "People dressed for the job",
        text: "Townspeople now wear what suits their work: the farmer in work clothes, the police in uniform, the fuel clerk behind the till. Press F to talk to them.",
      },
      {
        title: "A market under cover",
        text: "Five stalls now stand on the cobbles, each under a red cloth canopy on four wooden posts, with chalk price boards, sacks, a handcart and a delivery van. The juicer is still on one of the tables.",
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
        title: "Cuffing you can watch",
        text: "The cuffs open in the officer's hand, click shut, then go on the suspect.",
      },
      {
        title: "The bank teller is inside now",
        text: "The teller stands behind the lobby counter inside the bank. The outside counters keep only their sound emitters.",
      },
      {
        title: "New 3D models",
        text: "These items now have their own models in your hands and on the ground: handcuffs, taser, spike strip, strongbox, lockpick, pager, evidence marker, money bag, juicer, and ore and ingots. The handcuffs, taser and spike strip were rebuilt with more detail. New this build: a cement bag, a police scanner, a post parcel, a moonshine still, a smelter furnace and a dispatch console.",
      },
      {
        title: "Printed paperwork",
        text: "The fine ticket, till receipt, hospital bill, sentence slip and R-89 ID card are now printed forms, with a photo box, stamps and seals. They lie flat on the ground and sit in your hand.",
      },
      {
        title: "Ore that reads as rock",
        text: "Iron, copper and gold rocks now look like stone, with the ore only in the cracks and thin veins.",
      },
      {
        title: "A placement preview for the spike strip",
        text: "In dev-tools worlds only, an officer who selects Deploy on a folded strip sees a ghost where it will lie: blue when the server will accept it, red when not. Deploy now also refuses a strip on top of another, under a car or against a post, saying something is in the way.",
      },
    ],
    fixed: [
      "Cash now looks like money in your hands and on the ground.",
      "The car HUD is fixed.",
      "The store counter is fixed.",
      "Parcels are fixed.",
      "Townspeople wear boots from the right equipment slot.",
      "The inventory no longer flashes when you split or merge a stack.",
      "The fuel counter, till and clerk no longer clip through the floor or the window wall.",
      "Cars you buy stay parked on a slope instead of rolling away.",
      "The vehicle, weapon and action HUD slots are back.",
      "Held items sit in your fist the right way up.",
      "The back room door says what knocking does and shows when it is shut, and the general store points to it.",
    ],
    shots: [
      { id: "bank-vault-interior/vault-open-after", caption: "The vault door at the side of the bank, swung open." },
      { id: "polish-vault-market/bank-roadmap-bank-vault-after", caption: "The vault door closed, seen from the front." },
      { id: "polish-vault-market/vault-inside-after", caption: "Inside the vault: the steel room, the deposit boxes and the ceiling lights." },
      { id: "polish-vault-market/market-ob180-after", caption: "The market on the square, seen from above." },
      { id: "market-canopies/eye-level-after", caption: "The five market stalls, each under a red cloth canopy on four posts." },
      { id: "924/juicer-closeup-after", caption: "The new juicer on a market table." },
      { id: "npc-outfits/talk-after", caption: "Press F to talk to the farmer, who is dressed for the job." },
      { id: "925/cash-van-box-after", caption: "The new strongbox beside the cash van, with an evidence marker on the grass." },
      { id: "664/cuffs-hand-after", caption: "The handcuffs, held in the officer's fist." },
      { id: "ore-rocks-natural/gold-after", caption: "Ore rocks at the quarry, with a gold vein through the stone." },
      { id: "asset-batch2/cement-after", caption: "The cement bag on the quarry table: kraft paper with dark stencil type." },
      { id: "paper-items-print/ground-after", caption: "The printed forms and the R-89 ID card, lying on the ground." },
      { id: "asset-batch3/furnace-after", caption: "The smelter furnace at the quarry." },
      { id: "asset-batch3/console-after", caption: "The dispatch console on its desk." },
      { id: "928/ghost-valid-after", caption: "The spike strip preview, blue where the server will accept it (dev tools)." },
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
