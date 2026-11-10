import { useState } from "react";
import { Link } from "react-router-dom";
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
Build (commit hash, or the one the host gave):
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

const Playtest = () => (
  <>
    <PageHeader kicker="Playtest" title="Run a playtest, tell us what broke">
      <p>
        A community playtest of the M2 Living Town build is open. It is a small 1989 Everon town, hosted by a person from their own PC,
        so it is not always online and it can be full. You do not need to be a developer to play it.
      </p>
      <p className="text-[1rem] muted">
        <a href="#what" className="link">What it is</a> · <a href="#install" className="link">Install</a> ·{" "}
        <a href="#run" className="link">Join or host</a> · <a href="#test" className="link">What to test</a> ·{" "}
        <a href="#feedback" className="link">Feedback</a>
      </p>
    </PageHeader>

    <Section id="what" kicker="What it is" title="M2 Living Town, in one small town" aside={<Stamp tone="ink">Not released</Stamp>}>
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

    <Section id="install" kicker="Install" title="Get the game and the addon">
      <ol className="space-y-8 max-w-4xl">
        <li>
          <h3 className="heading">1. Arma Reforger on Steam</h3>
          <p className="mt-2">
            Install Arma Reforger from Steam. The playtest needs the game and nothing else from the Workshop.
          </p>
        </li>
        <li>
          <h3 className="heading">2. The Life Framework addon, at the host's commit</h3>
          <p className="mt-2">
            There is no public Workshop item for the playtest yet, so the addon comes from the GitHub repository. The host gives you a{" "}
            <strong>commit</strong>, a code such as <span className="font-mono">8c824c08</span>. Use exactly that one: a different build
            has different scripts, and the game will not match the host's.
          </p>
          <p className="mt-3 font-semibold">Without Git (easiest)</p>
          <ol className="mt-1 list-decimal pl-6 space-y-1">
            <li>
              Open <span className="font-mono text-[0.9375rem] break-all">{REPO_URL}/archive/&lt;commit&gt;.zip</span> in your browser, with
              the host's commit in place of <span className="font-mono">&lt;commit&gt;</span>. It downloads a zip file.
            </li>
            <li>
              Extract it to a short path such as <span className="font-mono">C:\LF</span>. You end up with a folder like{" "}
              <span className="font-mono">C:\LF\Life-Framework-8c824c08</span> that contains an <span className="font-mono">addons</span> folder.
            </li>
          </ol>
          <p className="mt-4 font-semibold">With Git</p>
          <p className="mt-1 mb-2 muted">In a terminal, in the folder you want it in (here <span className="font-mono">C:\LF</span>):</p>
          <Code>{CLONE_CMD}</Code>
        </li>
        <li>
          <h3 className="heading">3. Start the game with the addon loaded</h3>
          <p className="mt-2">
            The game loads the addon from your folder through its command line. Open PowerShell and run the line below, with these
            changed to match your PC:
          </p>
          <ul className="mt-2 mb-3 list-disc pl-6 space-y-1">
            <li>
              the path to <span className="font-mono">ArmaReforgerSteam.exe</span> (in Steam: right-click Arma Reforger, Manage, Browse
              local files);
            </li>
            <li>
              <span className="font-mono">&lt;host address&gt;</span>, which the host gives you;
            </li>
            <li>
              the path after <span className="font-mono">-addonsDir</span>: the <span className="font-mono">addons</span> folder you just
              got.
            </li>
          </ul>
          <Code>{LAUNCH_CMD}</Code>
          <p className="mt-3">
            The game starts and joins the host. Do not add other launch options: the host runs without them, and extra script options
            change the checksum the game compares.
          </p>
        </li>
        <li>
          <h3 className="heading">4. Use your own Steam name</h3>
          <p className="mt-2">
            The server keys your character to your Steam name, so you keep your cash, bank, job, cars and organization when you come
            back. Two players with the same Steam name share one character, so every tester needs a name of their own.
          </p>
        </li>
      </ol>
    </Section>

    <Section id="run" kicker="Run a playtest" title="Join a host, or host your own">
      <div className="grid gap-10 lg:grid-cols-2 max-w-6xl">
        <div className="sheet p-6 md:p-8">
          <p className="kicker">Join</p>
          <h3 className="display-3 mt-1">Join a host</h3>
          <p className="mt-3">
            Session times are announced on{" "}
            <a href={DISCORD_URL} className="link">Discord</a>. A host gives you two things: an address to join and the commit the build
            was made from. Install that commit as above and run the launch line with the host's address in it. The host's own tool prints
            the exact line for you to copy.
          </p>
          <p className="mt-3 muted">
            Over the internet, the host has to forward a UDP port to their PC. On a home network, use the host's LAN address.
          </p>
        </div>
        <div className="sheet p-6 md:p-8">
          <p className="kicker">Host</p>
          <h3 className="display-3 mt-1">Host your own</h3>
          <p className="mt-3">
            This is for someone who is comfortable in a terminal. Set up the project's tools from the{" "}
            <a href={blob("docs/setup.md")} className="link">setup guide</a>, then, in your clone:
          </p>
          <Code>{`tools\\cli playtest
tools\\cli playtest stop`}</Code>
          <p className="mt-3">
            The first runs the town as a dedicated server on your PC, joins it, and prints the commit and the client line your friends
            use. The second stops it. Saves are kept, so characters continue between sessions.
          </p>
        </div>
      </div>
      <p className="mt-8 max-w-prose muted">
        A host's PC is one machine: if it is full or off, nothing is wrong with your install. More on the setup in the{" "}
        <Link to="/docs/playtest" className="link">playtest guide</Link>.
      </p>
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
            <strong>Build</strong> is the commit the host gave you. Without it we cannot tell which version you played.
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
