import { Link } from "react-router-dom";
import { Figure, GoLink, PageHeader, Section, Stamp } from "@/components/ui";
import { DISCORD_URL, blob, milestone, siteData, still } from "@/lib/content";

const flagship: { rule: string; id: string }[] = [
  { rule: "Death drops what you carry. Bank, vehicles, and property are safe.", id: "death-drops-what-you-carry" },
  { rule: "No passive paycheck. Pay comes from finished runs, selling, and police duty.", id: "no-passive-paycheck-unemployment-benefit-at-the-counter" },
  { rule: "Hidden experience instead of an XP bar; professions show as titles.", id: "progression-is-pluggable" },
  { rule: "Everyone spawns civilian. Police clock in at the station duty desk.", id: "no-faction-screen-police-clock-in-at-the-duty-desk" },
  { rule: "A car left on the street at restart goes to the impound lot.", id: "garage-it-to-keep-it" },
  { rule: "A faster day and night cycle, so evening players still see daylight.", id: "accelerated-day-and-night-by-default" },
];

const configs = [
  ["Configs/Jobs/LF_Jobs.conf", "Duty pay, unemployment benefit, cooldowns"],
  ["Configs/Shop/ShopPrices.conf", "What shops and traders charge and pay"],
  ["Configs/Resources/ResourceChains.conf", "Gather and process chains"],
  ["Configs/Survival/LF_Survival.conf", "Hunger and thirst rates"],
  ["Configs/Vehicles/VehicleSettings.conf", "Vehicle lot and ownership"],
  ["Configs/Signs/LF_Signs.conf", "What each sign in town says"],
];

const Servers = () => {
  const m3 = siteData.milestones.find((m) => m.id === "M3");
  const m3Issues = milestone("M3");
  return (
    <>
      <PageHeader kicker="For server owners" title="Host it, tune it, build on it">
        <p>
          Life Framework is meant to be the base your community builds on, without forking it. It is not ready to host yet. The
          framework release is milestone M3. This page says what you will get, how it is configured, and which licence covers which
          copy.
        </p>
      </PageHeader>

      <div className="wrap">
        <div className="sheet p-5 md:p-8 grid gap-6 md:grid-cols-[auto_1fr] md:items-center">
          <Stamp tilt>Not released</Stamp>
          <div>
            <p className="heading">
              {m3?.id} {m3?.name}
            </p>
            <p className="muted text-[0.9375rem] mt-1">{m3?.doneWhen}</p>
            <p className="mt-3 flex flex-wrap gap-x-6 gap-y-2">
              <GoLink to="/roadmap">Where we are now</GoLink>
              {m3Issues && <GoLink to={m3Issues.url}>M3 on GitHub</GoLink>}
              <GoLink to={DISCORD_URL}>Ask on Discord</GoLink>
            </p>
          </div>
        </div>
      </div>

      <Section id="what" kicker="Two layers" title="Framework and flagship">
        <div className="grid gap-8 lg:grid-cols-2 lg:items-start">
          <article className="sheet p-6 md:p-8">
            <p className="kicker">Layer 1 · what you install</p>
            <h3 className="display-3 mt-1">The framework</h3>
            <p className="mt-3 muted">
              Modular systems for a life server. The core knows nothing about 1989: prices, places, and prefabs come from config and
              content, so a modern or alternate setting runs on the same code. Persistence uses Reforger's own{" "}
              <code>SCR_PersistenceSystem</code>; no third-party addon is needed.
            </p>
            <p className="kicker mt-6 mb-2">In the tree today</p>
            <ul className="border-t border-rule text-[0.9375rem]">
              {siteData.features.map((f) => (
                <li key={f.name} className="py-2 border-b border-rule grid gap-x-4 sm:grid-cols-[11rem_1fr]">
                  <span className="font-semibold">{f.name}</span>
                  <span className="muted">{f.what}</span>
                </li>
              ))}
            </ul>
            <p className="text-sm muted mt-3">
              Most systems are placed only in the test world so far, and none has been played on a dedicated server by strangers yet.
              That is what M1 is for.
            </p>
          </article>
          <article className="sheet p-6 md:p-8">
            <p className="kicker">Layer 2 · what we run</p>
            <h3 className="display-3 mt-1">The 1989 Everon flagship</h3>
            <p className="mt-3 muted">
              Our own server, built on the same framework, played by real people. It is the proof that the framework plays. Where a
              system has a range of settings, the flagship picks one. Some of its defaults:
            </p>
            <ul className="mt-4 border-t border-rule text-[0.9375rem]">
              {flagship.map((f) => (
                <li key={f.id} className="py-2.5 border-b border-rule">
                  <Link to={`/decisions#${f.id}`} className="link">
                    {f.rule}
                  </Link>
                </li>
              ))}
            </ul>
            <p className="text-sm muted mt-3">
              Each one is a setting on your server. The <Link to="/decisions" className="link">decisions</Link> page lists the range the
              framework supports for each.
            </p>
            <Figure still={still("village-overview")} className="mt-8" sizes="(min-width: 1024px) 500px, 100vw" />
          </article>
        </div>
      </Section>

      <Section id="config" kicker="Configuration" title="Files, not code">
        <div className="grid gap-10 lg:grid-cols-[1fr_1.1fr]">
          <div>
            <p className="muted">
              The rule is that every price, threshold, and timer lives in a config file under <code>addons/LifeFramework/Configs/</code>, never as a
              literal in a system. The set grows scope by scope; a documented reference ships with M3.
            </p>
            <ul className="mt-6 border-t-2 border-ink text-[0.9375rem]">
              {configs.map(([file, what]) => (
                <li key={file} className="py-2.5 border-b border-rule">
                  <a href={blob(`addons/LifeFramework/${file}`)} className="font-mono text-[0.8125rem] link break-all">
                    {file}
                  </a>
                  <span className="block muted">{what}</span>
                </li>
              ))}
            </ul>
          </div>
          <figure>
            <div className="terminal p-5 md:p-6 overflow-x-auto">
              <p className="font-mono text-[0.75rem] uppercase tracking-[0.08em] opacity-70 mb-3">Configs/Jobs/LF_Jobs.conf</p>
              <pre className="font-mono text-[0.875rem] leading-relaxed">
                {`LF_JobsConfig {
 m_fPoliceDutyPayPerSecond 0.5
 m_iBenefitBalanceThreshold 500
 m_iBenefitAmount 100
 m_fBenefitCooldownSeconds 1200
}`}
              </pre>
            </div>
            <figcaption className="text-sm muted mt-3">
              The flagship's job settings: $0.50 per second on duty, and a $100 benefit at the job counter for anyone below $500 in the
              bank, once every 20 minutes.
            </figcaption>
          </figure>
        </div>
      </Section>

      <Section id="licence" kicker="Licences" title="Which licence covers which copy">
        <div className="grid gap-8 md:grid-cols-2">
          <article className="sheet p-6 md:p-8">
            <span className="plate text-xl">MIT</span>
            <h3 className="display-3 mt-4">The source on GitHub</h3>
            <p className="mt-3 muted">
              Fork it, change it, ship it. Keep the copyright notice. If you want MIT terms for your server's build, build from
              GitHub.
            </p>
            <p className="mt-4">
              <GoLink to={blob("LICENSE")}>LICENSE</GoLink>
            </p>
          </article>
          <article className="sheet p-6 md:p-8">
            <span className="plate text-xl">APL</span>
            <h3 className="display-3 mt-4">The Workshop build</h3>
            <p className="mt-3 muted">
              Bohemia's Workshop terms require an APL-family licence on anything uploaded, and MIT is not on that list. So the build we
              publish on the Workshop carries the Arma Public License. The code is the same.
            </p>
            <p className="mt-4">
              <GoLink to="/decisions#the-workshop-build-is-apl-the-github-repo-stays-mit">The decision</GoLink>
            </p>
          </article>
        </div>
        <p className="mt-8 max-w-prose muted">
          The framework is free and stays free. Our flagship may take donations for cosmetics only, within Bohemia's server
          monetisation rules. It never sells money, gear, vehicles, property, queue priority, or a faster whitelist.{" "}
          <Link to="/decisions#the-framework-is-always-free-the-flagship-may-take-cosmetic-donations" className="link">
            Read the decision
          </Link>
          .
        </p>
      </Section>

      <Section id="hosting" kicker="Hosting" title="Running it today">
        <div className="grid gap-8 md:grid-cols-2 max-w-5xl">
          <p className="muted">
            Playtests are hosted from the maintainer's PC with <code>tools\cli host</code>, which runs the playtest scenario, continues
            the latest save, and backs saves up. You need Arma Reforger Server (Steam app 1874900). Rented hosting comes later.
          </p>
          <ul className="space-y-2">
            <li>
              <GoLink to={blob("docs/setup.md")}>Development setup</GoLink>
            </li>
            <li>
              <GoLink to={blob("server/README.md")}>Server tooling</GoLink>
            </li>
            <li>
              <GoLink to={blob("docs/playtest.md")}>Playtest guide</GoLink>
            </li>
          </ul>
        </div>
      </Section>
    </>
  );
};

export default Servers;
