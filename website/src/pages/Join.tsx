import { GoLink, Figure, PageHeader, Section, ClipPlayer } from "@/components/ui";
import { JoinCta } from "@/components/JoinCta";
import { RunYourOwn } from "@/components/RunYourOwn";
import { blob, siteData, still } from "@/lib/content";

// The page every outreach post links to (#568). Routes: /join and /join/<source>, where <source> is
// a community's tag from content/outreach-sources.json. The page is the same for every source; the tag
// is in the path so Cloudflare Web Analytics can count visits by community, and the Discord invite
// behind the button can be a per-source invite. Wording follows docs/community/discord.md: no hype, no
// dates, no promises, and say what is built. Written from docs/features.md and the /playtest page.

// The hero demo clip (#571) goes in media.json with this id. Until it exists, the slot is empty.
const HERO_CLIP_ID = "hero-demo";

// True when the build was given a Cloudflare Web Analytics token (VITE_CF_WEB_ANALYTICS_TOKEN, see
// scripts/prerender.mjs). The privacy line below is shown only then, so the page never claims a count that is off.
const ANALYTICS_ON = Boolean(import.meta.env.VITE_CF_WEB_ANALYTICS_TOKEN);

const Join = () => {
  const hero = siteData.media.clips.find((c) => c.id === HERO_CLIP_ID);
  return (
    <>
      <PageHeader kicker="Join" title="An open-source life and roleplay framework for Arma Reforger">
        <p>
          We are building the framework, and a 1989 Everon server to prove it. It is early: one small test town is playable, with a
          bank, a store, a car lot, police, medics, payphones, a fence and a few crimes. Most of it has had few players.
        </p>
        <div className="mt-6">
          <JoinCta />
        </div>
      </PageHeader>

      {hero ? (
        <div className="wrap pb-12 max-w-4xl">
          <ClipPlayer clip={hero} />
        </div>
      ) : null}

      <Section id="see" kicker="What it looks like" title="Pictures from the game">
        <div className="grid gap-8 md:grid-cols-3 max-w-6xl">
          <Figure still={still("spawn-square")} sizes="(min-width: 768px) 33vw, 100vw" />
          <Figure still={still("village-police")} sizes="(min-width: 768px) 33vw, 100vw" />
          <Figure still={still("village-street")} sizes="(min-width: 768px) 33vw, 100vw" />
        </div>
        <p className="mt-8">
          <GoLink to="/gallery">More pictures</GoLink>
        </p>
      </Section>

      <Section id="honest" kicker="What to expect" title="Built, not built, and who builds it">
        <div className="grid gap-10 md:grid-cols-3 max-w-6xl">
          <div>
            <h3 className="heading mb-2">Built</h3>
            <p>
              A small test town you play in rather than click through: a clerk and an ID card, a bank, a store, a car lot, police and
              dispatch, medics, payphones, a fence and a chop shop. What each does is on the{" "}
              <GoLink to="/features">Features page</GoLink>.
            </p>
          </div>
          <div>
            <h3 className="heading mb-2">Not built</h3>
            <p>
              A full map, a Workshop item for the test build, an always-on server, and many of the systems on the roadmap. There is
              no release date. The <GoLink to="/roadmap">roadmap</GoLink> says what is planned and what is not.
            </p>
          </div>
          <div>
            <h3 className="heading mb-2">Who builds it</h3>
            <p>
              AI agents write most of the code, under the maintainer's direction. The maintainer merges every change, and each one is a
              public pull request. The Discord bot is an AI and says so; the staff are people.
            </p>
          </div>
        </div>
      </Section>

      <Section id="how" kicker="How to take part" title="Two steps">
        <ol className="space-y-6 max-w-3xl list-decimal pl-6">
          <li>
            <strong>Join the Discord.</strong> New members agree to the rules first. The pinned FAQ answers the usual questions: is
            this Overthrow, is it made with AI, when does it release.
          </li>
          <li>
            <strong>Download the playtest.</strong> It is a zip for Windows with Arma Reforger from Steam: unzip it and run the
            launcher. The <GoLink to="/playtest">playtest page</GoLink> has the steps, what to test, and where to report what broke.
            The community server runs from a person's PC, so it is not always on.
          </li>
        </ol>
        <div className="mt-8">
          <JoinCta />
        </div>
        <div id="alpha" className="mt-12 max-w-3xl">
          <h3 className="heading mb-2">Joining the alpha server</h3>
          <p>
            In the server browser, look for <strong>Life Framework | Everon 1989 (Alpha)</strong>. It has a password. We do not
            print it here: ask for it in the Discord and a staff member will send it. The password is the only gate. The next
            session time is announced in the Discord.
          </p>
          <p className="mt-3">
            It is an alpha. Progress can be wiped, and a wipe comes with at least 24 hours' notice on Discord. Read the{" "}
            <GoLink to={blob("docs/community/wipe-policy.md")}>wipe policy</GoLink> before a long build.
          </p>
          <p className="mt-3">
            <GoLink to="/alpha">How to ask for an invite</GoLink>
          </p>
        </div>
        {ANALYTICS_ON ? (
          <p className="mt-10 max-w-prose text-[0.9375rem] muted">
            We count visits to this site with Cloudflare Web Analytics, which sets no cookies and keeps no personal data. We do not
            ask for your email.
          </p>
        ) : null}
      </Section>
      <Section id="run-your-own" kicker="Run your own" title="Want your own server?">
        <RunYourOwn />
      </Section>
    </>
  );
};

export default Join;
