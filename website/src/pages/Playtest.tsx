import { useState } from "react";
import { Link } from "react-router-dom";
import { PlaytestButtons } from "@/components/PlaytestButtons";
import { GoLink, PageHeader, Section, Stamp } from "@/components/ui";
import { DISCORD_URL, REPO_URL, blob } from "@/lib/content";

// Written from docs/playtest.md, docs/features.md, docs/setup.md and the output of
// `tools\cli playtest` (tools/host/playtest.mjs, friendLines). Player-facing facts only: no dates,
// no promises. Change this page when those change.

// The Discord feedback channel, created 2026-11-10 under COMMUNITY. The channel link opens for
// members of the server; the invite works for everyone.
const FEEDBACK_CHANNEL_URL = "https://discord.com/channels/1444353039430582324/1569806155461562441";
const ISSUE_FORM_URL = `${REPO_URL}/issues/new?template=playtest.yml`;

const TEMPLATE = `What I did:
What I expected:
What happened:
Where (place name, sign or coordinates):
Screenshot or clip:
Build (the first line of version.txt in the playtest folder):
Role and Steam name (civilian, police or medic):`;

const CLONE_CMD = `git clone https://github.com/Life-Framework/Life-Framework.git
git -C Life-Framework checkout <commit>`;

const LAUNCH_CMD = `& "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Arma Reforger\\ArmaReforgerSteam.exe" \`
  -client <host address> \`
  -addonsDir "C:\\LF\\Life-Framework\\addons" \`
  -addons LifeFramework`;

type Area = { id: string; title: string; players: string; items: string[] };

const AREAS: Area[] = [
  {
    id: "arrival",
    title: "Arrival and the post office",
    players: "1 player",
    items: [
      "Fill in registration form R-89 (name and age). Check the clerk on the square gives you an ID card, points you to the town board and offers a parcel run.",
      "Sign up at the post office counter and deliver the parcel. Check the run pays $40.",
      "Five parcel runs should open fuel delivery, fifteen the taxi.",
    ],
  },
  {
    id: "money",
    title: "Bank, store and cars",
    players: "1 player",
    items: [
      "Deposit and withdraw at the two teller counters either side of the bank door (about 177 and 187, 259).",
      "Buy food, drink and a dressing at the general store from its price sheet.",
      "Buy a car at the lot (desk about 211, 262). Retrieve the starter S105 from the garage and the UAZ from the impound desk for the fee.",
    ],
  },
  {
    id: "police",
    title: "Police",
    players: "2 players",
    items: [
      "Police needs a roster spot: on a fresh save the first player to join gets it, otherwise the host adds you. Clock in at the station desk (about 237, 241) and check the duty kit, the station car key and the radio (41000 kHz).",
      "A second player robs the fuel till (about 263, 240). Check the officer hears it on the radio, sees the wanted board, can arrest the robber, and the robber ends up in the cell.",
      "Clock out at the same desk and check your civilian things come back.",
    ],
  },
  {
    id: "comms",
    title: "Payphones and dispatch",
    players: "2 players",
    items: [
      "Four payphones stand in town (square, fuel station, hospital, police station). A call takes one coin from the cash you carry. Try Report a crime, Report an emergency and Report someone suspicious.",
      "With an officer on duty, check the call is heard on the radio and listed on the dispatch board, and that taking it tells the caller a unit is on the way.",
      "With nobody on duty nothing should connect and no coin should be taken. One player can call once a minute.",
    ],
  },
  {
    id: "medic",
    title: "Medic and downed players",
    players: "2 players",
    items: [
      "A lethal hit should leave a player downed, not dead: lying down, unable to act, bleeding out after three minutes. Holding X (pad Y) after 30 seconds gives up, which is death.",
      "Clock in at the hospital desk (about 262, 256) and revive a downed player. Check the $100 treatment bill and that a bandage or saline bag adds time before a revive.",
      "Drag or carry a downed player, and load them into the ambulance.",
    ],
  },
  {
    id: "investigation",
    title: "Investigation",
    players: "2 or more",
    items: [
      "After a crime, check a payphone witness can give a description and that an officer can use Ask what you saw on a witness.",
      "Shots near a crime should leave casings, and dropped items a paper marker. Bag them as evidence and log them at the locker in the police station (about 234, 238).",
      "Read the case file. A serial'd fence tool should lead back to the buyer.",
    ],
  },
  {
    id: "underworld",
    title: "The underworld",
    players: "1 to 3 players",
    items: [
      "The fence, Vince, stands behind the barn (about 186, 311). He sells a lockpick to anyone and sells a drill, a breaching charge and a keycard only after you have completed 3, 5 and 8 crimes.",
      "Pick the lock-up door (about 306, 241), then the locked car east of it (about 312, 246) and hotwire it. Take it to the chop shop (about 322, 300) and check the payout is 30 percent of the car's value.",
      "Drill the cash van's box at the bank doorstep for money sacks; the bank vault (about 194, 265) opens only to a fence tool.",
      "Find the moonshine camp east of the chop shop (about 380 to 388, 300 to 305): load the three barrels, then work the still.",
    ],
  },
  {
    id: "moderation",
    title: "Staff tools and zones",
    players: "2 players",
    items: [
      "On the spawn square players cannot hurt each other. Check weapons still fire and that world damage still passes.",
      "Type /report in chat after something happens to you. You should get a Staff were told toast; a second report within two minutes is refused.",
      "Walk into the quarry (about 178, 172): a red Lawless Area tag and a toast should appear, and go when you leave.",
      "Hold up another player within 2 m with a weapon in hand, and hold a raised weapon on someone for 3 seconds.",
    ],
  },
  {
    id: "org",
    title: "Organization and hideout",
    players: "2 players",
    items: [
      "Found an organization at the registry desk (about 289, 256), deposit cash and buy the processing upgrade.",
      "Take the alley yard lease (about 302, 288), open the hideout door and store something in the stash.",
      "Run contraband from the barn to the pub.",
    ],
  },
  {
    id: "restart",
    title: "Leave and rejoin",
    players: "1 player",
    items: [
      "Leave and rejoin with the same Steam name. Cash, bank, job, organization and cars should be back.",
      "If the host restarts the server, rejoin and check the same things again. This is the check that matters most.",
    ],
  },
];

const NOT_BUILT = [
  "A travel globe at the spawn that teleports you to any area (issue #607). Until it exists, walk or drive.",
  "Buying a house",
  "A general crafting bench",
  "A fence, gate or lawless-only rules for the quarry (the zone and its signs are in)",
  "A police mission board",
  "World events",
  "New guns, cars or aircraft",
];

const ROUGH_EDGES = [
  "Esc does not close the wanted card or the organization ledger right after founding; use the button or walk away (#199).",
  "Some NPCs may appear half-dressed, and sign titles can be hard to read from the road.",
  "The death screen title is clipped, the organization ledger cuts off the lease status, and the moonshine still renders as a plain white boiler (#493, #494, #497).",
  "If an arrest leaves you standing where you were instead of in the cell, say so.",
];

function CopyButton({ text, label }: { text: string; label: string }) {
  const [state, setState] = useState<"idle" | "copied" | "failed">("idle");
  return (
    <button
      type="button"
      className="btn btn-ghost"
      onClick={() => {
        if (!navigator.clipboard) {
          setState("failed");
          return;
        }
        navigator.clipboard.writeText(text).then(
          () => setState("copied"),
          () => setState("failed"),
        );
      }}
    >
      {state === "copied" ? "Copied" : state === "failed" ? "Select and copy it" : label}
    </button>
  );
}

const Code = ({ children }: { children: string }) => (
  <pre className="font-mono text-[0.8125rem] leading-relaxed bg-inset border border-rule p-4 overflow-x-auto whitespace-pre">{children}</pre>
);

const STEPS = [
  {
    n: "1",
    title: "Download",
    body: "Press the yellow Download the playtest button. Your browser saves one zip file, usually in Downloads.",
    img: "/playtest-steps/step-download.webp",
    alt: "The yellow Download the playtest button on this website",
  },
  {
    n: "2",
    title: "Unzip it anywhere",
    body: "Right-click the zip and choose Extract All, then Extract. You get a folder with Play Life Framework in it. Keep the files in that folder together.",
    img: "/playtest-steps/step-unzip.webp",
    alt: "The unzipped folder in Windows Explorer, with Play Life Framework, README, addons and launcher",
  },
  {
    n: "3",
    title: "Double-click Play Life Framework",
    body: "Pick Play on my own for the town to yourself, or Join the community server to play with others. The game opens and loads the town in a minute or two.",
    img: "/playtest-steps/step-play.webp",
    alt: "The Life Framework window with two buttons: Play on my own, and Join the community server",
  },
];

const Playtest = () => (
  <>
    <PageHeader kicker="Playtest" title="Download the playtest and play">
      <p>
        A small 1989 Everon town with jobs, a bank, police, medics and crime. All you need is Arma Reforger from Steam on a Windows PC.
      </p>
    </PageHeader>
    <div className="wrap pb-12 -mt-2">
      <PlaytestButtons />
      <p className="mt-6 text-[1rem] muted">
        <a href="#install" className="link">How to install</a> · <a href="#run" className="link">Play with others</a> ·{" "}
        <a href="#what" className="link">What it is</a> · <a href="#test" className="link">What to test</a> ·{" "}
        <a href="#feedback" className="link">Feedback</a>
      </p>
    </div>

    <Section id="install" kicker="Install" title="Three steps, no setup">
      <ol className="grid gap-8 md:grid-cols-3">
        {STEPS.map((st) => (
          <li key={st.n} className="sheet p-5 flex flex-col">
            <div className="flex items-center gap-3">
              <span className="btn-download inline-flex items-center justify-center w-11 h-11 shrink-0 border-[3px] border-[#1b1d19] font-display font-bold text-2xl">
                {st.n}
              </span>
              <h3 className="display-3">{st.title}</h3>
            </div>
            <div className="mt-4 h-44 flex items-center justify-center bg-inset border border-rule overflow-hidden">
              <img src={st.img} alt={st.alt} loading="lazy" decoding="async" className="max-h-full w-full object-contain" />
            </div>
            <p className="mt-4">{st.body}</p>
          </li>
        ))}
      </ol>
      <div className="mt-8 max-w-3xl space-y-3">
        <p>
          <strong>If Windows warns you.</strong> A blue box saying "Windows protected your PC" means Windows has not seen this file
          before. Click <strong>More info</strong>, then <strong>Run anyway</strong>. If it asks whether to open or run the file, say yes.
        </p>
        <p>
          <strong>If the game is not found.</strong> Install Arma Reforger from Steam and start it once, then double-click Play Life
          Framework again.
        </p>
        <p className="muted">
          <strong>Use your own Steam name.</strong> The server keeps your character by your Steam name, so your cash, bank, job and cars
          are there when you come back. Two players with the same name share one character.
        </p>
      </div>
    </Section>

    <Section id="what" kicker="What it is" title="M2 Living Town, in one small town" aside={<Stamp tone="ink">Test build</Stamp>}>
      <div className="grid gap-10 lg:grid-cols-2 max-w-6xl">
        <div className="space-y-4">
          <p>
            M2 adds five systems on top of the M1 town: moderation, comms, medics, investigation, and the underworld. You can call the
            police from a payphone, go down and be revived, be questioned about what you saw, buy a lockpick from a fence, steal a car
            and break it up at a chop shop. The town is the same village throughout; nothing here is the finished map.
          </p>
          <p className="muted">
            This is a test build, and it has rough edges. What each system does is on the{" "}
            <Link to="/features" className="link">Features page</Link>; the full guide is{" "}
            <Link to="/docs/playtest" className="link">the playtest doc</Link>.
          </p>
        </div>
        <div className="space-y-4">
          <h3 className="heading">You start with money</h3>
          <p>
            The playtest town runs a money top-up so nobody stalls on cash. On spawn and every 60 seconds, a bank balance under $100,000
            is filled to $1,000,000 and carried cash under $5,000 is filled to $20,000. Buy what you want to test.
          </p>
          <h3 className="heading">The spawn square</h3>
          <p>
            You start on the square with the clerk, the town board and a payphone. A spawn hub with a travel globe that jumps you to any
            area is wanted (<a href={`${REPO_URL}/issues/607`} className="link">#607</a>); it is not in the build yet.
          </p>
        </div>
      </div>
    </Section>

    <Section id="run" kicker="Play with others" title="Join the community server">
      <div className="grid gap-10 lg:grid-cols-2 max-w-6xl">
        <div className="space-y-4">
          <p>
            Play sessions are announced on <a href={DISCORD_URL} className="link">Discord</a>. When one is on, double-click Play Life
            Framework and pick <strong>Join the community server</strong>. The launcher looks up where the server is and joins it.
          </p>
          <p className="muted">
            The server is hosted from a person's own PC, so it is not always on and it can be full. If nobody is hosting, the launcher
            says so, and Play on my own works any time.
          </p>
        </div>
        <div className="space-y-4">
          <p>
            <strong>A new build came out?</strong> The launcher tells you when the server runs a newer playtest than yours. Download the
            zip again from this page and play from the new folder.
          </p>
        </div>
      </div>

      <details className="mt-12 sheet p-5 md:p-6 max-w-4xl">
        <summary className="heading cursor-pointer">For developers: run from the source, or host a server</summary>
        <div className="mt-5 space-y-8">
          <div>
            <h3 className="heading">Run from a git checkout</h3>
            <p className="mt-2">
              The download is the <span className="font-mono">addons/LifeFramework</span> folder of a tagged commit, its resource
              database, and the launcher. To play a commit no release covers, clone the repository at the host's commit:
            </p>
            <div className="mt-3">
              <Code>{CLONE_CMD}</Code>
            </div>
            <p className="mt-3">
              Then start the game with the addon loaded. Change the game path, the host address and the path after{" "}
              <span className="font-mono">-addonsDir</span> to match your PC:
            </p>
            <div className="mt-3">
              <Code>{LAUNCH_CMD}</Code>
            </div>
            <p className="mt-3 muted">
              Do not add other launch options: extra script defines change the checksum the game compares with the host. A fresh clone
              has no <span className="font-mono">resourceDatabase.rdb</span>; open the project in Workbench once (or run{" "}
              <span className="font-mono">{"tools\\cli rdb"}</span> in a worktree) or the game cannot find the addon's files.
            </p>
          </div>
          <div>
            <h3 className="heading">Host your own</h3>
            <p className="mt-2">
              Set up the project's tools from the <a href={blob("docs/setup.md")} className="link">setup guide</a>, then, in your clone:
            </p>
            <div className="mt-3">
              <Code>{`tools\\cli playtest
tools\\cli playtest stop`}</Code>
            </div>
            <p className="mt-3">
              The first runs the town as a dedicated server on your PC, joins it, and prints the commit and the client line your friends
              use. The second stops it. Saves are kept between sessions. Over the internet, forward UDP 2001 to your PC. How the
              launcher's community server is pointed at a host is in the <Link to="/docs/playtest" className="link">playtest guide</Link>.
            </p>
          </div>
        </div>
      </details>
    </Section>

    <Section id="test" kicker="What to test" title="A short list for each area">
      <div className="max-w-3xl space-y-4 mb-10">
        <p>
          Signs and prompts in the world lead the way. Pick an area, try what is listed, and report what does not match. Places are given
          as approximate map positions (x, z), the same ones the maintainers use when they check a build. Most of the live work is
          better with a second player.
        </p>
        <p className="muted">
          The automated checks cover a lot already. What they cannot do is press the real keys, judge sound and animation, drive, feel
          three players crowding a till, or play over a real network. Those are the most useful things you can report.
        </p>
      </div>
      <div className="grid gap-6 md:grid-cols-2">
        {AREAS.map((a) => (
          <article key={a.id} id={a.id} className="sheet p-6 scroll-mt-28">
            <div className="flex items-baseline justify-between gap-4">
              <h3 className="heading">{a.title}</h3>
              <span className="font-mono text-[0.75rem] uppercase tracking-[0.06em] text-ink-2 whitespace-nowrap">{a.players}</span>
            </div>
            <ul className="mt-4 space-y-3">
              {a.items.map((item) => (
                <li key={item} className="flex gap-3 text-[0.9375rem]">
                  <input type="checkbox" className="mt-1 w-4 h-4 shrink-0 accent-[var(--green)]" aria-label={item} />
                  <span>{item}</span>
                </li>
              ))}
            </ul>
          </article>
        ))}
      </div>

      <div className="mt-14 grid gap-10 md:grid-cols-2 max-w-6xl">
        <div>
          <h3 className="heading mb-3">Not in this build</h3>
          <p className="muted mb-3">Do not report these as broken. Say so under "build next" if you want them.</p>
          <ul className="list-disc pl-5 space-y-1.5">
            {NOT_BUILT.map((n) => (
              <li key={n}>{n}</li>
            ))}
          </ul>
        </div>
        <div>
          <h3 className="heading mb-3">Known rough edges</h3>
          <p className="muted mb-3">Already on our list. A new detail is still welcome.</p>
          <ul className="list-disc pl-5 space-y-1.5">
            {ROUGH_EDGES.map((n) => (
              <li key={n}>{n}</li>
            ))}
          </ul>
        </div>
      </div>
    </Section>

    <Section id="feedback" kicker="Feedback" title="Tell us in one place">
      <div className="grid gap-8 lg:grid-cols-2 max-w-6xl">
        <article className="sheet p-6 md:p-8">
          <p className="kicker">Discord</p>
          <h3 className="display-3 mt-1">#playtest-feedback</h3>
          <p className="mt-3">
            Post in the <span className="font-mono">#playtest-feedback</span> channel of the Life Framework Discord. One message per
            problem, using the template here. The same template is pinned in the channel.
          </p>
          <p className="mt-5 flex flex-wrap gap-3">
            <a href={FEEDBACK_CHANNEL_URL} className="btn btn-primary">Open #playtest-feedback</a>
            <a href={DISCORD_URL} className="btn btn-ghost">Join the Discord first</a>
          </p>
          <p className="mt-4 text-[0.9375rem] muted">
            New members agree to the server rules before they can post. A screenshot or a short clip helps more than a long description.
          </p>
        </article>
        <article className="sheet p-6 md:p-8">
          <p className="kicker">GitHub</p>
          <h3 className="display-3 mt-1">Open an issue</h3>
          <p className="mt-3">
            If you would rather use GitHub, the Playtest form asks for the same things and files the report where the maintainers track
            the work. You need a free GitHub account.
          </p>
          <p className="mt-5">
            <a href={ISSUE_FORM_URL} className="btn btn-ghost">Open the Playtest form</a>
          </p>
        </article>
      </div>

      <div className="mt-10 max-w-3xl">
        <h3 className="heading mb-2">The template</h3>
        <p className="muted mb-3">One message per problem. Leave a line empty if you do not know it.</p>
        <Code>{TEMPLATE}</Code>
        <p className="mt-3">
          <CopyButton text={TEMPLATE} label="Copy template" />
        </p>
        <ul className="mt-6 list-disc pl-5 space-y-1.5 text-[0.9375rem]">
          <li>
            <strong>Where</strong> is the place name, the nearest sign, or the map position if you can see it. Say whether you were a
            civilian, police or medic.
          </li>
          <li>
            <strong>Build</strong> is the first line of <span className="font-mono">version.txt</span> in your playtest folder, such as
            playtest-m2-1. Without it we cannot tell which version you played.
          </li>
          <li>
            A report is read by a person. It is not a promise that the thing will be built or fixed.
          </li>
        </ul>
      </div>

      <p className="mt-12 flex flex-wrap gap-x-8 gap-y-3">
        <GoLink to="/features">Features</GoLink>
        <GoLink to="/docs/playtest">Full playtest guide</GoLink>
        <GoLink to="/roadmap">Roadmap</GoLink>
      </p>
    </Section>
  </>
);

export default Playtest;
