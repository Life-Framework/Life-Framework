import { GoLink, PageHeader, Section, Stamp } from "@/components/ui";
import { REPO_URL } from "@/lib/content";

// Player-facing facts for the systems in the 0.3.0 build, written from docs/releases/0.3.0-notes.md,
// docs/features/*.md and the merged pull requests. No dates, no promises: if it is not in the build,
// it is not here. Every picture file must be listed in content/gallery.json.
type Shot = { file: string; alt: string };
type System = {
  id: string;
  title: string;
  epic?: number;
  lede: string;
  facts: { head: string; text: string }[];
  shot?: Shot;
};

const NEW_SYSTEMS: System[] = [
  {
    id: "work",
    title: "Mining, logging and smelting",
    lede: "Real jobs with real tools: dig it, cut it, smelt it and sell it.",
    facts: [
      {
        head: "Iron, start to finish",
        text: "Mine iron at Highstone hill, dig coal at the East Rise seam and smelt both into an iron bar at the furnace on the east compound. The furnace takes two iron nuggets and one coal and runs for a minute. If you lack the trade or level, the refusal says which.",
      },
      {
        head: "Coal",
        text: "The East Rise seam opens at Mining level 4. Sell coal lumps at the coal buyer by the power plant.",
      },
      {
        head: "Lumber",
        text: "Fell trees, saw the logs and sell planks and boards at the timber yard. Planks carry as bundles of four.",
      },
      {
        head: "Tools that wear",
        text: "Pickaxes, axes and rods come in three tiers: Worn, Standard and Fine. They wear down as you use them, and the hardware store's bench repairs and sharpens them for a fee.",
      },
      {
        head: "Levels and perks",
        text: "Every trade levels up with a sound and a card that names what the new level opens. At level 10 you pick a perk at the labor office: double yield, faster work or less tool wear. Your first shift of the day pays double experience for 30 minutes.",
      },
      {
        head: "Shift boards",
        text: "A board at the mine and the sawmill ranks the workers of the week.",
      },
    ],
    shot: { file: "3244/session-5-smelter-prompt-after", alt: "The smelter prompt: two nuggets and one coal make a bar." },
  },
  {
    id: "trucks",
    title: "Trucks and hauling",
    lede: "A truck license opens the truck yard, and a truck opens the heavy loads.",
    facts: [
      {
        head: "Truck license",
        text: "The truck check costs $300. An examiner lends you a truck and you drive four gates in order. A wrong turn fails the course, and you wait before a retake.",
      },
      {
        head: "Truck yard",
        text: "The yard on the north-east compound sells trucks to people with a truck license. Its price list marks which rows need the license, so you know before you save up.",
      },
      {
        head: "Loads",
        text: "A cargo truck bed takes a full load of logs. A full bed refuses the next one and says why.",
      },
      {
        head: "Impound",
        text: "A truck you left on the street across a restart goes to the impound. You collect it there for a fee.",
      },
      {
        head: "Export board",
        text: "At Hauling level 10 the board at the dock warehouse offers export orders: iron bars, coal or planks for the ships. Bring the goods to the crane spot and the hand-in pays. A short load pays nothing and is refused.",
      },
    ],
    shot: { file: "5144/truck-yard-sheet-after", alt: "The truck yard price list. Both trucks are marked as needing a truck license." },
  },
  {
    id: "police",
    title: "Police: the booking desk",
    lede: "Arrest someone and the paperwork is part of the game.",
    facts: [
      {
        head: "Booking",
        text: "Cuff someone and book them at the Saint-Philippe station. The officer ticks the charges on a charge sheet and signs. Nothing is charged until the officer signs.",
      },
      {
        head: "Pay or take the time",
        text: "The suspect chooses to pay the fine at once or take the time. Crimes that carry jail only offer the cell. Jail sentences top out at 15 minutes.",
      },
      {
        head: "My file",
        text: "Your F2 menu lists your tickets, your bookings and any license that was taken.",
      },
      {
        head: "Run a plate",
        text: "An officer can run the plate of the car in front and read the answer on the pager.",
      },
      {
        head: "Pay order",
        text: "Police duty is the lowest-paid steady work. Medics earn a little more, and civilian trades at the high tiers pay more than both. Police and medics also earn small bonuses for outcomes, capped per hour.",
      },
    ],
  },
  {
    id: "gangs",
    title: "Gangs and the north outpost",
    lede: "Found a crew, fly your flag and talk on your own radio channel.",
    facts: [
      {
        head: "Registry",
        text: "Found a gang at the registry in town for $250, pick a flag from the flag wall and invite people by scroll.",
      },
      {
        head: "Gang radio",
        text: "Buy a gang radio for $60 at the gun counter and tune it. Your gang shares a channel.",
      },
      {
        head: "North outpost",
        text: "A rebel camp in the pines with gate guards, a quartermaster and a chalk board. It sells a gang radio to anyone and a pistol once the camp knows you. A rifle needs more standing than that.",
      },
      {
        head: "Rumors",
        text: "Ask the barkeepers at the Chotain and Levie bars, and the shopkeepers, “Heard anything?” to hear a rumor. You get one a day.",
      },
    ],
    shot: { file: "4978/camp-night-after", alt: "An outpost at night, lit by lanterns and a fire, with guards behind the sandbags." },
  },
  {
    id: "homes",
    title: "Homes and cars",
    lede: "Buy a house, buy a car and see your name on both.",
    facts: [
      {
        head: "Buy a house",
        text: "A house nobody owns has a For sale sign. Stand at it with the price in carried cash and choose Buy house. You get the key, and the house stays yours after a server restart.",
      },
      {
        head: "SOLD",
        text: "Once a house or plot is sold, its sign reads SOLD for everyone, including players who join later.",
      },
      {
        head: "License plates",
        text: "Every car you can buy now draws its license plate, front and rear.",
      },
      {
        head: "Respray bay",
        text: "Park your own car on the pad at the garage and pick a paint job at the bench for $150.",
      },
      {
        head: "Garage",
        text: "Taking your car out of the garage shows one card with your plate. Its hazard lights blink and a marker shows it for 30 seconds, so you can find it.",
      },
    ],
    shot: { file: "sold-sign-board/sold_buyer_after_purchase", alt: "A SOLD sign beside a house door." },
  },
  {
    id: "shops",
    title: "Shops, signs and the night",
    lede: "The town looks lived in, at any hour.",
    facts: [
      {
        head: "Goods on the shelves",
        text: "The pharmacy, hardware store, general store, gun store, butcher, bakery and village bar show single items on their shelves instead of empty boards. Shop doors carry opening-hours plates.",
      },
      {
        head: "Readable price lists",
        text: "A price list shows six to eight rows at a time, and each lot names its own list, such as “Truck yard - price list”. Every row has a plain name.",
      },
      {
        head: "Gun counter",
        text: "It sells a pistol ($350) and a rifle ($1,500) with their magazines, all behind a firearm permit ($250). Scopes, holsters and a pouch are on the list, and so is the gang radio, which needs no permit.",
      },
      {
        head: "Night lighting",
        text: "At 22:00 the street lamps and shop fronts light up, and so do the insides of the hostel, pub, fuel station, gun counter, newsagent and registry. Lamps at the dock light the export board and the crane spot.",
      },
      {
        head: "Signs",
        text: "New signs mark the respray bay, the seed and feed barn, the labor office, the shift boards, the bus timetable and the road junctions into town.",
      },
      {
        head: "Voices and sounds",
        text: "The clerk, registrar, barkeepers and outpost people speak recorded lines. The church bell rings at 07:00, a ship’s horn sounds at 19:00 and a whistle starts the shift at the mine and the sawmill.",
      },
      {
        head: "Newsagent",
        text: "It sells the Everon Courier, a road map and a tourist map.",
      },
    ],
    shot: { file: "night-lamps-pub/bar-after", alt: "Inside the pub with the lights on: the bar, stools and the barman." },
  },
  {
    id: "first-hour",
    title: "Your first hour",
    lede: "From the hostel to steady work, with fewer cards in the way.",
    facts: [
      {
        head: "Steady work",
        text: "After you sell your juice at the market, a card titled “Find steady work” points you to the town board and the clerk. A sale reads “Sold for $18!”.",
      },
      {
        head: "What to do next",
        text: "Your F2 menu lists ways to earn a living. A row that pays nothing by the hour says “no wage”. Once every arrival errand is done, the F3 welcome guide opens on a What next page that sends you to F2.",
      },
      {
        head: "Map waypoints",
        text: "Shift-click on the map to set a waypoint, or on a place to set it there. It shows the distance, and Backspace clears it.",
      },
      {
        head: "Courier runs",
        text: "The pickup card says to carry the parcel to the delivery stop marked on your map and names your cash deposit, which comes back on delivery.",
      },
    ],
    shot: { file: "5124/market-find-steady-work-after", alt: "The market stall after a sale, with a Find steady work card on the right." },
  },
];

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
        text: "An on-duty medic within about two metres of a downed player can pick Revive. It comes first in the menu wherever the medic aims at the body. The patient gets up with a sliver of health and keeps what they carry.",
      },
      {
        head: "Medic calls",
        text: "Clock in at the clinic and casualties appear around the island: the mine, the woodlot, the farm, the dock and the roadside. The call shows on the board and your pager. Take it, draw an ambulance from the car pool, then treat the patient or carry them to a clinic bed. Treating and delivering pay bonuses at clock-out, up to an hourly cap.",
      },
      {
        head: "Load into a vehicle",
        text: "A downed body is loaded straight from where it lies: stand beside it with a vehicle's cargo seat within reach and pick Load into vehicle. A medic's bed delivery takes the body lying beside them. There is no carry or drag.",
      },
      {
        head: "Clinic and ambulance",
        text: "The village clinic has a ward with hospital beds. The ambulance waits in its bay by the road and runs its siren only while its driver is on duty.",
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
      {
        head: "Taser",
        text: "The station desk holds a taser. Fired within about ten metres it drops the target into the real down state: the body ragdolls and falls, cannot move or shoot for eight seconds, then wakes lying prone instead of snapping upright. An NPC falls on his back and gets up.",
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
        text: "A walk-in village shop: goods stand on the shelves as real items and are paid for at the till inside. Walking out unpaid trips the bell box on the shop front and the shopkeeper shouts after you. The price list still sells over the counter.",
      },
      {
        head: "Drug store",
        text: "A pharmacy on the north row of the main street, under the vanilla DROGERIE fascia: self-serve aspirin, gauze and tourniquets, a pharmacist in a white coat behind the counter, and a dispensary that sells morphine and saline to licensed medics only.",
      },
      {
        head: "Supermarket",
        text: "A walk-in supermarket with gondolas, coolers and three tills. Goods come off the shelves and are paid for at a till; walking out unpaid sets the alarm going, calls police dispatch and raises the wanted level.",
      },
      {
        head: "Town board",
        text: "A cork board of eight index cards on the spawn square: work, shops, bank, police recruitment, hospital, car lot, organizations and rules. Each card says where its place is, and Set waypoint marks it on screen with the distance.",
      },
      {
        head: "Spawn square",
        text: "The payphone moved off the tree to the edge of the square. The clerk has a desk and chair, and a handcart stands by the benches on the north side.",
      },
      {
        head: "Market square",
        text: "Five stalls stand on cobbled ground under red cloth canopies on wooden posts, with chalk price boards, sacks, a handcart and a delivery van. The juicer sits on one table.",
      },
      {
        head: "Bank vault",
        text: "The strong room sits against the bank's east wall. Its front matches the bank's light plaster, with a corrugated hood and a caged lamp over the door.",
      },
      {
        head: "Ore rocks",
        text: "Iron rocks read as stone, with the ore only in the cracks and thin veins.",
      },
      {
        head: "Paperwork",
        text: "The fine ticket, receipt, hospital bill, sentence slip and R-89 ID card are printed 1989 forms.",
      },
      {
        head: "Object models",
        text: "The cement bag, police scanner, post parcel, moonshine still, smelter furnace and dispatch console now have period models.",
      },
    ],
  },
];

const NOT_YET = [
  "A fence, a gate or lawless-only rules at the quarry (the zone and its signs are in)",
  "A police mission board",
  "A general crafting bench",
  "World events",
];

// Straight from the known issues in the 0.3.0 release notes.
const ROUGH = [
  "The first ambulance draw can be refused with “a vehicle stands in the bay” when nothing is there. Try again a few minutes later.",
  "Bells, whistles and voices start, but nobody has judged them by ear yet. Expect uneven volume or timing.",
  "The Everon Courier is on sale, but a fresh world has no weekly edition to read.",
  "Supermarket shelves can look empty in a new world until the store restocks.",
  "This is still an alpha server. A wipe is announced 24 hours before it happens.",
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

function SystemSection({ s, kicker }: { s: System; kicker: string }) {
  return (
    <Section
      id={s.id}
      kicker={kicker}
      title={s.title}
      aside={s.epic ? <GoLink to={`${REPO_URL}/issues/${s.epic}`}>Discussion on GitHub</GoLink> : undefined}
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
  );
}

const Features = () => (
  <>
    <PageHeader kicker="Features" title="What the town does in 0.3.0">
      <p>
        Everything on this page runs in the 0.3.0 alpha build. The first group is what 0.3.0 adds: jobs from mine to furnace, trucks,
        a police booking desk, gangs, homes, stocked shops and a town that lights up at night. The second group is what was already
        there. If a system is not in the build, it is not on this page. The {" "}
        <a href="#rough-edges" className="link">rough edges</a> are listed at the bottom.
      </p>
      <p className="text-[1rem] muted">
        New: {NEW_SYSTEMS.map((s, i) => (
          <span key={s.id}>
            {i > 0 && " · "}
            <a href={`#${s.id}`} className="link">{s.title}</a>
          </span>
        ))}
      </p>
      <p className="text-[1rem] muted">
        Earlier: {SYSTEMS.map((s, i) => (
          <span key={s.id}>
            {i > 0 && " · "}
            <a href={`#${s.id}`} className="link">{s.title}</a>
          </span>
        ))}
      </p>
    </PageHeader>

    {NEW_SYSTEMS.map((s) => (
      <SystemSection key={s.id} s={s} kicker="New in 0.3.0" />
    ))}

    {SYSTEMS.map((s) => (
      <SystemSection key={s.id} s={s} kicker={s.epic ? "Living town" : "Playtest village"} />
    ))}

    <Section id="rough-edges" kicker="Know before you play" title="Rough edges">
      <ul className="grid gap-2 max-w-3xl list-disc pl-5">
        {ROUGH.map((n) => (
          <li key={n}>{n}</li>
        ))}
      </ul>
    </Section>

    <Section id="not-built" kicker="Not in this build" title="Not built yet" aside={<Stamp tone="ink">Not released</Stamp>}>
      <ul className="grid gap-2 mb-8 max-w-3xl list-disc pl-5">
        {NOT_YET.map((n) => (
          <li key={n}>{n}</li>
        ))}
      </ul>
      <div className="flex flex-wrap gap-x-8 gap-y-3">
        <GoLink to="/playtest">Join the playtest</GoLink>
        <GoLink to="/gallery">Gallery</GoLink>
        <GoLink to="/roadmap">Roadmap</GoLink>
        <GoLink to="/docs/features">Full feature notes</GoLink>
      </div>
    </Section>
  </>
);

export default Features;
