import { GoLink, PageHeader, Section, Stamp } from "@/components/ui";
import { REPO_URL } from "@/lib/content";

// Player-facing facts for the M2 systems, written from docs/features.md, docs/playtest.md and the
// merged pull requests. No dates, no promises: if it is not in the playtest build, it is not here.
type Shot = { file: string; alt: string };
type System = {
  id: string;
  title: string;
  epic: number;
  lede: string;
  facts: { head: string; text: string }[];
  shot?: Shot;
};

const SYSTEMS: System[] = [
  {
    id: "moderation",
    title: "Moderation",
    epic: 145,
    lede: "The server keeps a record of what happens between players, and staff have tools to read it.",
    facts: [
      {
        head: "Incident log",
        text: "Kills, damage, cash moves, downings, hold-ups, aimed weapons and admin actions are written to a log on the server. Players never see it.",
      },
      {
        head: "Game Master view",
        text: "A Game Master sees the incidents as markers around the camera, with what happened, how long ago and who was involved. Only administrators can open it.",
      },
      {
        head: "Report to staff",
        text: "Type /report in chat after something happens to you. It flags your incidents from the last ten minutes and staff are told. A second report within two minutes is refused.",
      },
      {
        head: "Hold up",
        text: "Face a player within 2 m with a weapon in hand and pick Hold up. Your hand goes up and both of you hear a beep. It changes nothing else except the log.",
      },
      {
        head: "Aimed weapons",
        text: "Keeping a raised weapon on another player for 3 seconds writes one row to the log.",
      },
      {
        head: "Lawless quarry",
        text: "The quarry north of the square is the one marked lawless area. Two red signs give the rules. For now it labels incidents and keeps wanted on. It has no fence or gate.",
      },
      {
        head: "Safe square",
        text: "On the spawn square you cannot hurt another player and they cannot hurt you. Weapons still fire.",
      },
    ],
    shot: { file: "243/quarry-sign-close", alt: "A red Lawless Area sign on a post beside the quarry lane." },
  },
  {
    id: "comms",
    title: "Comms",
    epic: 147,
    lede: "A call has to reach someone. Payphones, radios and dispatch boards carry it.",
    facts: [
      {
        head: "Payphones",
        text: "A booth takes one coin from the cash you carry. Pick Report a crime, Report an emergency or Report someone suspicious. An on-duty officer hears it on the radio and sees it on the wanted board. One call a minute per player. With nobody on duty nothing connects and no coin is taken.",
      },
      {
        head: "Where",
        text: "Payphones stand on the square, at the fuel station, outside the hospital and outside the police station.",
      },
      {
        head: "Dispatch boards",
        text: "The police board sits beside the wanted board at the station. The medic board is in the hospital. Read one to see open calls. An on-duty player can take a call, and the caller is told the unit is on the way.",
      },
      {
        head: "Radios and pagers",
        text: "The police duty kit has a radio on 41000 kHz and the medic kit one on 42000 kHz. Two officers hear each other, a civilian does not. The medic kit also has a pager that beeps for calls.",
      },
      {
        head: "Police scanner",
        text: "The fence sells one for $900 after two crimes. It chirps and shows a toast for every police or medic call. It is illegal: an officer who searches a wanted suspect takes it and files a charge.",
      },
    ],
    shot: { file: "250/police-board-call", alt: "The police dispatch board showing a crime reported at a fuel station." },
  },
  {
    id: "medic",
    title: "Medic",
    epic: 146,
    lede: "A lethal hit knocks a player down. Someone can still get to them in time.",
    facts: [
      {
        head: "Downed, not dead",
        text: "You lie on the ground, cannot move or act, take no further damage, and others can search you. NPCs still die as before.",
      },
      {
        head: "Bleed-out",
        text: "You bleed out after three minutes. A bandage, tourniquet or saline bag adds time, up to five minutes in all. Morphine adds none. Hold X (pad Y) after 30 seconds to give up, which is death. Disconnecting while downed is death too.",
      },
      {
        head: "Medic duty",
        text: "Clock in at the duty desk in the hospital. Anyone can, there is no roster. You swap to the medic kit and your civilian inventory is kept until you clock out, when duty pay is handed over in cash.",
      },
      {
        head: "Revive",
        text: "An on-duty medic within about two metres of a downed player can pick Revive. They get up with a sliver of health and keep what they carry.",
      },
      {
        head: "Drag, carry, load",
        text: "A downed body can be dragged, carried, put down or loaded into a vehicle.",
      },
      {
        head: "Hospital and ambulance",
        text: "The Everon Village Hospital is the two-storey house on the east side of the square road, with three beds in the ground-floor ward. The ambulance waits in its bay by the road and runs its siren only while its driver is on duty.",
      },
      {
        head: "Bills on paper",
        text: "A medic's revive costs $100 and waking in hospital after a death costs $150, on 1989 paper, paid in cash at the desk. An unpaid hospital bill becomes bank debt.",
      },
    ],
    shot: { file: "263/hospital-ambulance-siren", alt: "An olive ambulance with a red light bar lit, parked beside the hospital." },
  },
  {
    id: "investigation",
    title: "Investigation",
    epic: 148,
    lede: "Crimes leave traces. Officers can follow them, and criminals have to earn their tools.",
    facts: [
      {
        head: "Witnesses",
        text: "Witnesses near a crime can describe the suspect. The description appears on the police wanted board, and an officer can pick Ask what you saw on a nearby NPC.",
      },
      {
        head: "Evidence",
        text: "After a crime, shots near it leave casings and dropped items get a paper marker. A clocked-in officer bags them and logs the bags at the evidence locker beside the armory. Read case file lists what is on file.",
      },
      {
        head: "The fence",
        text: "Vince works behind the old barn at the north-west edge of the village. He sells crime tools only to proven criminals: a drill after 3 completed crimes, a breaching charge after 5, a keycard after 8. The crime count is hidden from players.",
      },
      {
        head: "Serials",
        text: "Each fence tool carries a serial. Use Read serial to see it. A serial on a bagged tool puts the buyer's name on the wanted card. Serials survive a restart.",
      },
      {
        head: "Bank vault",
        text: "The vault door opens only to a fence tool. Opening it files a crime report like any other.",
      },
      {
        head: "Cash van",
        text: "A van parks at the bank, the general store and the fuel station in turn. Its box opens to a drill and drops three money bags behind it. The money is cash in a bag, never a bank balance.",
      },
      {
        head: "Police armory",
        text: "The armory cabinet at the station opens to the fence keycard or a charge, and always radios every on-duty officer. The weapons inside carry serials booked to whoever took them.",
      },
    ],
    shot: { file: "271/fence-list", alt: "The fence's list of tools with prices and an order slip." },
  },
  {
    id: "underworld",
    title: "Underworld",
    epic: 410,
    lede: "Locks, stolen cars and an illegal still, each with a way for police to find it.",
    facts: [
      {
        head: "Lockpick",
        text: "The fence sells one for $250 with no record needed. It is a stack, and each opened lock or snapped pick wears one use. Picking takes 6 to 20 seconds depending on your level. It opens, slips or snaps. The yard gate, the bank vault and the jail door cannot be picked.",
      },
      {
        head: "Steal a car",
        text: "A locked car will not start without its key. Pick its lock, get in, and use Hotwire the ignition at the wheel. It takes 6 to 15 seconds and can fail, and a failure may be heard. The car is then marked stolen.",
      },
      {
        head: "Reporting and recovery",
        text: "The owner can use Report the car stolen and the plate goes on the wanted board. An officer can Check the plate on any car and Recover stolen vehicle on a reported, empty one, which returns it to the owner's garage.",
      },
      {
        head: "Chop shop",
        text: "In a walled yard behind the chain barrels, park a stolen car beside the engine crane, take it apart, and take the cash and parts. It pays 30 percent of the car's value and two spare parts. More hands strip it faster. The car is gone for good and the yard takes nothing for five minutes.",
      },
      {
        head: "Moonshine",
        text: "Mash at a barrel, then distill at a still. There is no menu and no progress bar: you hold the action and the prompt says how far along the batch is. Every worker adds to the same batch, so two workers finish a barrel in half the time. The crates are what the barn-to-pub convoy pays for.",
      },
      {
        head: "Smoke and smell",
        text: "A working site gives off smoke, and players within range notice the smell. Officers get a message and dispatch logs a police call, at most one per site every five minutes.",
      },
      {
        head: "Searches",
        text: "An officer can search a player or a vehicle for moonshine. The more is carried, the better the chance. A find seizes the goods and files a charge. Searching the same target again within two minutes finds nothing.",
      },
    ],
    shot: { file: "418/m2_418_still_smoke_officer_client3_1", alt: "Grey smoke over a still and its workers, seen by an officer." },
  },
  {
    id: "village",
    title: "The village",
    epic: 0,
    lede: "The playtest town the systems above run in.",
    facts: [
      {
        head: "General store",
        text: "Goods stand on the counter as real items. Picking one up by hand tags it unpaid. The price list still works and checks stock before it takes your cash. An empty line refuses the sale.",
      },
      {
        head: "Spawn square",
        text: "The payphone moved off the tree to the edge of the square. The clerk has a desk and chair, and a handcart stands by the benches on the north side.",
      },
    ],
  },
];

const NOT_YET = [
  "A fence, a gate or lawless-only rules at the quarry (the zone and its signs are in)",
  "A police mission board",
  "Buying a house",
  "A general crafting bench",
  "World events",
];

function Picture({ shot }: { shot: Shot }) {
  const base = `/gallery/${shot.file}`;
  return (
    <figure className="photo">
      <img
        src={`${base}-1280.webp`}
        srcSet={`${base}-640.webp 640w, ${base}-1280.webp 1280w`}
        sizes="(min-width: 1024px) 480px, 100vw"
        width={1280}
        height={720}
        alt={shot.alt}
        loading="lazy"
        decoding="async"
        className="h-auto"
      />
    </figure>
  );
}

const Features = () => (
  <>
    <PageHeader kicker="Features" title="What the playtest town does">
      <p>
        These are the M2 systems that run in the playtest build today: moderation, comms, medic, investigation and the underworld. The
        text comes from the repository's feature notes and playtest guide. If a system is not in the build, it is not on this page.
      </p>
      <p className="text-[1rem] muted">
        <a href="#moderation" className="link">Moderation</a> · <a href="#comms" className="link">Comms</a> · <a href="#medic" className="link">Medic</a> ·{" "}
        <a href="#investigation" className="link">Investigation</a> · <a href="#underworld" className="link">Underworld</a> ·{" "}
        <a href="#village" className="link">The village</a>
      </p>
    </PageHeader>

    {SYSTEMS.map((s) => (
      <Section
        key={s.id}
        id={s.id}
        kicker={s.epic ? "M2 Living Town" : "Playtest village"}
        title={s.title}
        aside={s.epic ? <GoLink to={`${REPO_URL}/issues/${s.epic}`}>Epic #{s.epic}</GoLink> : undefined}
      >
        <div className={`grid gap-10 ${s.shot ? "lg:grid-cols-[1.5fr_1fr] lg:items-start" : ""}`}>
          <div>
            <p className="lede muted mb-6 max-w-3xl">{s.lede}</p>
            <dl className="border-t-2 border-ink">
              {s.facts.map((f) => (
                <div key={f.head} className="grid gap-x-6 gap-y-1 py-3 border-b border-rule md:grid-cols-[10rem_1fr]">
                  <dt className="font-semibold">{f.head}</dt>
                  <dd>{f.text}</dd>
                </div>
              ))}
            </dl>
          </div>
          {s.shot && <Picture shot={s.shot} />}
        </div>
      </Section>
    ))}

    <Section id="not-built" kicker="Not in this build" title="Not built yet" aside={<Stamp tone="ink">Not released</Stamp>}>
      <ul className="grid gap-2 mb-8 max-w-3xl list-disc pl-5">
        {NOT_YET.map((n) => (
          <li key={n}>{n}</li>
        ))}
      </ul>
      <div className="flex flex-wrap gap-x-8 gap-y-3">
        <GoLink to="/docs/playtest">Play and report a bug</GoLink>
        <GoLink to="/gallery">Gallery</GoLink>
        <GoLink to="/roadmap">Roadmap</GoLink>
        <GoLink to="/docs/features">Full feature notes</GoLink>
      </div>
    </Section>
  </>
);

export default Features;
