import { useEffect, useState, type ReactNode } from "react";
import { Link, NavLink, useLocation } from "react-router-dom";
import { DiscordIcon, GitHubIcon, Mark } from "@/components/icons";
import { DISCORD_URL, REPO_URL, githubData, milestoneProgress, CURRENT_MILESTONE, CURRENT_MILESTONE_NAME, formatDate, supportOpen } from "@/lib/content";

const NAV = [
  { to: "/roadmap", label: "Roadmap" },
  { to: "/features", label: "Features" },
  { to: "/gallery", label: "Gallery" },
  { to: "/devlog", label: "Devlog" },
  { to: "/servers", label: "Server owners" },
  { to: "/contribute", label: "Contribute" },
  { to: "/docs", label: "Docs" },
];

export const Wordmark = ({ className = "" }: { className?: string }) => (
  <span className={`inline-flex items-center gap-2.5 ${className}`}>
    <Mark className="w-9 h-9 shrink-0" />
    <span className="font-display font-bold uppercase text-[1.4rem] leading-none tracking-[0.01em]">Life Framework</span>
  </span>
);

function ThemeToggle() {
  // The server renders "Theme"; the real label arrives after hydration, so the markup matches.
  const [theme, setTheme] = useState<"light" | "dark" | null>(null);
  useEffect(() => {
    setTheme(document.documentElement.getAttribute("data-theme") === "dark" ? "dark" : "light");
  }, []);
  const next = theme === "dark" ? "light" : "dark";
  return (
    <button
      type="button"
      className="kicker !text-ink hover:bg-inset px-2 py-2 min-h-[2.75rem] min-w-[2.75rem] border-2 border-transparent"
      onClick={() => {
        document.documentElement.setAttribute("data-theme", next);
        try {
          localStorage.setItem("theme", next);
        } catch {
          /* private mode: the choice lasts for this page only */
        }
        setTheme(next);
      }}
    >
      <span className="sr-only">Switch to </span>
      {theme === "dark" ? "Day" : theme === "light" ? "Night" : "Theme"}
      <span className="sr-only"> {theme ? "theme" : ""}</span>
    </button>
  );
}

function Header() {
  const [open, setOpen] = useState(false);
  const { pathname } = useLocation();
  useEffect(() => setOpen(false), [pathname]);
  const { done, total } = milestoneProgress(CURRENT_MILESTONE);

  return (
    <header className="bg-paper border-b-2 border-ink sticky top-0 z-40">
      <div className="bg-ink text-paper">
        <div className="wrap flex items-center justify-between gap-4 py-1.5 font-mono text-[0.7rem] tracking-[0.08em] uppercase">
          <span className="truncate">
            Everon, 1989 · <span className="hidden sm:inline">open-source </span>life/RP for Arma Reforger
          </span>
          <Link to="/roadmap" className="hidden sm:inline whitespace-nowrap hover:underline">
            {CURRENT_MILESTONE} {CURRENT_MILESTONE_NAME}: {done}/{total} issues done
          </Link>
        </div>
      </div>
      <div className="wrap flex items-center justify-between gap-4 h-16">
        <Link to="/" className="no-underline" aria-label="Life Framework, home">
          <Wordmark />
        </Link>
        <nav aria-label="Main" className="hidden lg:block">
          <ul className="flex items-center gap-1">
            {NAV.map((n) => (
              <li key={n.to}>
                <NavLink
                  to={n.to}
                  className={({ isActive }) =>
                    `font-mono text-[0.8rem] uppercase tracking-[0.06em] px-2.5 py-2 border-b-2 ${
                      isActive ? "border-stamp text-ink" : "border-transparent text-ink-2 hover:text-ink hover:border-rule"
                    }`
                  }
                >
                  {n.label}
                </NavLink>
              </li>
            ))}
          </ul>
        </nav>
        <div className="flex items-center gap-1">
          <a href={REPO_URL} className="hidden sm:inline-flex p-2.5 hover:bg-inset" aria-label="Life Framework on GitHub">
            <GitHubIcon />
          </a>
          <a href={DISCORD_URL} className="hidden sm:inline-flex p-2.5 hover:bg-inset" aria-label="Life Framework Discord">
            <DiscordIcon />
          </a>
          <ThemeToggle />
          <button
            type="button"
            className="lg:hidden kicker !text-ink border-2 border-ink px-3 min-h-[2.75rem]"
            aria-expanded={open}
            aria-controls="mobile-nav"
            onClick={() => setOpen((o) => !o)}
          >
            {open ? "Close" : "Menu"}
          </button>
        </div>
      </div>
      <nav id="mobile-nav" aria-label="Main" hidden={!open} className="lg:hidden border-t border-rule bg-card">
        <ul className="wrap py-2">
          {NAV.map((n) => (
            <li key={n.to}>
              <NavLink
                to={n.to}
                className={({ isActive }) =>
                  `flex items-center justify-between py-3 border-b border-rule font-display font-bold uppercase text-2xl ${isActive ? "text-stamp" : ""}`
                }
              >
                {n.label}
              </NavLink>
            </li>
          ))}
          <li className="flex gap-6 py-4 font-mono text-sm uppercase tracking-[0.06em]">
            <a href={REPO_URL} className="inline-flex items-center gap-2">
              <GitHubIcon /> GitHub
            </a>
            <a href={DISCORD_URL} className="inline-flex items-center gap-2">
              <DiscordIcon /> Discord
            </a>
          </li>
        </ul>
      </nav>
    </header>
  );
}

function Footer() {
  const built = formatDate(githubData.fetchedAt);
  const cols: { title: string; links: { label: string; href: string }[] }[] = [
    {
      title: "The project",
      links: [
        { label: "Roadmap", href: "/roadmap" },
        { label: "Features", href: "/features" },
        { label: "Gallery", href: "/gallery" },
        { label: "Devlog", href: "/devlog" },
        { label: "Decisions", href: "/decisions" },
        { label: "Direction", href: "/docs/direction" },
      ],
    },
    {
      title: "Take part",
      links: [
        { label: "Server owners", href: "/servers" },
        { label: "Contribute", href: "/contribute" },
        { label: "Docs", href: "/docs" },
        { label: "Discord", href: DISCORD_URL },
        ...(supportOpen ? [{ label: "Support the project", href: "/support" }] : []),
      ],
    },
    {
      title: "Source",
      links: [
        { label: "GitHub repository", href: REPO_URL },
        { label: "AGENTS.md", href: `${REPO_URL}/blob/main/AGENTS.md` },
        { label: "Licence (MIT)", href: `${REPO_URL}/blob/main/LICENSE` },
        { label: "This site's source", href: `${REPO_URL}/tree/main/website` },
      ],
    },
  ];
  return (
    <footer className="mt-24 border-t-2 border-ink bg-card">
      <div className="wrap py-12 grid grid-cols-2 gap-10 md:grid-cols-[1.4fr_repeat(3,1fr)]">
        <div className="space-y-3 max-w-sm col-span-2 md:col-span-1">
          <Wordmark />
          <p className="text-sm muted">
            An open-source life and roleplay framework for Arma Reforger, and the 1989 Everon server that proves it. Not affiliated with
            Bohemia Interactive.
          </p>
        </div>
        {cols.map((c) => (
          <div key={c.title}>
            <h2 className="kicker mb-3">{c.title}</h2>
            <ul className="space-y-1.5 text-[0.9375rem]">
              {c.links.map((l) => (
                <li key={l.label}>
                  {l.href.startsWith("/") ? (
                    <Link to={l.href} className="link">
                      {l.label}
                    </Link>
                  ) : (
                    <a href={l.href} className="link">
                      {l.label}
                    </a>
                  )}
                </li>
              ))}
            </ul>
          </div>
        ))}
      </div>
      <div className="border-t border-rule">
        <div className="wrap py-4 flex flex-col sm:flex-row gap-2 justify-between font-mono text-[0.75rem] uppercase tracking-[0.06em] text-ink-2">
          <span>Source: MIT · Workshop build: APL</span>
          <span>
            Built from the repo · GitHub read {built}
            {githubData.source === "snapshot" ? " (snapshot)" : ""}
          </span>
        </div>
      </div>
    </footer>
  );
}

export function Layout({ children }: { children: ReactNode }) {
  return (
    <>
      <a href="#main" className="skip">
        Skip to content
      </a>
      <Header />
      <main id="main" tabIndex={-1} className="outline-none">
        {children}
      </main>
      <Footer />
    </>
  );
}
