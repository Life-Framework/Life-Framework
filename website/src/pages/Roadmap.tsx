import { CheckCircle2, Circle, Users, TrendingUp, UserPlus, Github, ExternalLink, FileText, History } from "lucide-react";
import { Link } from "react-router-dom";
import { Card } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { AnimatedBackground } from "@/components/AnimatedBackground";
import { ProgressBar } from "@/components/ProgressBar";
import { DISCORD_URL, REPO_URL, githubData, siteData, type Issue, type ArchiveItem } from "@/lib/content";

const CURRENT = "M1";

const DiscordIcon = () => (
  <svg className="w-5 h-5" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
    <path d="M20.317 4.37a19.791 19.791 0 0 0-4.885-1.515a.074.074 0 0 0-.079.037c-.21.375-.444.864-.608 1.25a18.27 18.27 0 0 0-5.487 0a12.64 12.64 0 0 0-.617-1.25a.077.077 0 0 0-.079-.037A19.736 19.736 0 0 0 3.677 4.37a.07.07 0 0 0-.032.027C.533 9.046-.32 13.58.099 18.057a.082.082 0 0 0 .031.057a19.9 19.9 0 0 0 5.993 3.03a.078.078 0 0 0 .084-.028a14.09 14.09 0 0 0 1.226-1.994a.076.076 0 0 0-.041-.106a13.107 13.107 0 0 1-1.872-.892a.077.077 0 0 1-.008-.128a10.2 10.2 0 0 0 .372-.292a.074.074 0 0 1 .077-.01c3.928 1.793 8.18 1.793 12.062 0a.074.074 0 0 1 .078.01c.12.098.246.198.373.292a.077.077 0 0 1-.006.127a12.299 12.299 0 0 1-1.873.892a.077.077 0 0 0-.041.107c.36.698.772 1.362 1.225 1.993a.076.076 0 0 0 .084.028a19.839 19.839 0 0 0 6.002-3.03a.077.077 0 0 0 .032-.054c.5-5.177-.838-9.674-3.549-13.66a.061.061 0 0 0-.031-.03zM8.02 15.33c-1.183 0-2.157-1.085-2.157-2.419c0-1.333.956-2.419 2.157-2.419c1.21 0 2.176 1.096 2.157 2.42c0 1.333-.956 2.418-2.157 2.418zm7.975 0c-1.183 0-2.157-1.085-2.157-2.419c0-1.333.955-2.419 2.157-2.419c1.21 0 2.176 1.096 2.157 2.42c0 1.333-.946 2.418-2.157 2.418z" />
  </svg>
);

const plainCell = (s: string) => s.replace(/\*\*/g, "").replace(/`/g, "");

// "M1: M1-08 Registration form..." -> { code: "M1-08", title: "Registration form..." }
function splitTitle(title: string): { code: string | null; title: string } {
  const m = /^(?:M\d+:\s*)?(M\d+-\d+):?\s+(.*)$/.exec(title);
  if (m) return { code: m[1], title: m[2] };
  return { code: null, title: title.replace(/^M\d+:\s*/, "") };
}

function issueOrder(a: Issue, b: Issue) {
  const ca = splitTitle(a.title).code ?? "";
  const cb = splitTitle(b.title).code ?? "";
  return ca.localeCompare(cb, undefined, { numeric: true }) || a.number - b.number;
}

const IssueCard = ({ issue }: { issue: Issue }) => {
  const { code, title } = splitTitle(issue.title);
  const closed = issue.state === "closed";
  const unassigned = !closed && issue.assignees.length === 0;
  const labels = issue.labels.filter((l) => l !== "m1");
  return (
    <Card
      className={`bg-card/50 backdrop-blur-sm border-border hover:border-primary/30 hover:bg-card transition-all duration-300 group overflow-hidden ${
        closed ? "opacity-80" : ""
      }`}
    >
      {closed ? (
        <div className="bg-gradient-to-r from-emerald-500 to-emerald-600 px-5 py-2 flex items-center gap-2 text-white">
          <CheckCircle2 className="w-4 h-4" />
          <span className="text-sm font-semibold">Done</span>
        </div>
      ) : unassigned ? (
        <div className="bg-gradient-to-r from-orange-500 to-orange-600 px-5 py-2 flex items-center justify-between text-white">
          <div className="flex items-center gap-2">
            <UserPlus className="w-4 h-4" />
            <span className="text-sm font-semibold">Looking for Contributors</span>
          </div>
          <span className="text-xs hidden xl:inline whitespace-nowrap">Comment to pick it up</span>
        </div>
      ) : (
        <div className="bg-gradient-to-r from-blue-500 to-blue-600 px-5 py-2 flex items-center gap-2 text-white">
          <Users className="w-4 h-4" />
          <span className="text-sm font-semibold">Assigned and Progressing</span>
        </div>
      )}
      <div className="p-5 space-y-3">
        <div className="flex items-start justify-between gap-4">
          <div className="space-y-1 flex-1 min-w-0">
            <div className="flex items-center gap-2 text-xs text-muted-foreground">
              {code && <span className="font-mono font-semibold text-primary">{code}</span>}
              <span>#{issue.number}</span>
              {issue.wave && <span>· Wave {issue.wave}</span>}
            </div>
            <h3 className="text-lg font-semibold text-foreground group-hover:text-primary transition-colors">
              <a href={issue.url} target="_blank" rel="noopener noreferrer">
                {title}
              </a>
            </h3>
            {issue.excerpt && <p className="text-sm text-muted-foreground leading-relaxed line-clamp-3">{issue.excerpt}</p>}
          </div>
          <a href={issue.url} target="_blank" rel="noopener noreferrer" aria-label={`Open issue #${issue.number} on GitHub`} className="text-muted-foreground hover:text-primary">
            <ExternalLink className="w-4 h-4" />
          </a>
        </div>
        {(labels.length > 0 || issue.assignees.length > 0) && (
          <div className="flex flex-wrap items-center gap-2 pt-2 border-t border-border/50 text-xs text-muted-foreground">
            {labels.map((l) => (
              <Badge key={l} variant="outline" className="font-medium">
                {l}
              </Badge>
            ))}
            {issue.assignees.length > 0 && (
              <span className="flex items-center gap-1.5 ml-auto">
                <Users className="w-3.5 h-3.5" />
                {issue.assignees.join(", ")}
              </span>
            )}
          </div>
        )}
      </div>
    </Card>
  );
};

const statusDot = (s: ArchiveItem["status"]) => {
  if (s === "completed") return <CheckCircle2 className="w-4 h-4 text-primary flex-shrink-0 mt-0.5" />;
  if (s === "in-progress") return <TrendingUp className="w-4 h-4 text-orange-500 flex-shrink-0 mt-0.5" />;
  return <Circle className="w-4 h-4 text-muted-foreground flex-shrink-0 mt-0.5" />;
};

const Roadmap = () => {
  const { milestone, issues, source, fetchedAt } = githubData;
  const open = issues.filter((i) => i.state === "open").sort(issueOrder);
  const closed = issues.filter((i) => i.state === "closed").sort(issueOrder);
  const total = issues.length;
  const pct = total ? Math.round((closed.length / total) * 100) : 0;
  const lookingFor = open.filter((i) => i.assignees.length === 0).length;
  const fetched = new Date(fetchedAt).toLocaleDateString(undefined, { year: "numeric", month: "short", day: "numeric" });

  return (
    <div className="min-h-screen pt-24 pb-16 relative">
      <AnimatedBackground />
      <div className="container mx-auto px-4 relative z-10">
        <div className="max-w-7xl mx-auto">
          <div className="text-center mb-12">
            <h1 className="text-4xl md:text-5xl font-display font-bold mb-4">
              Development <span className="text-primary">Roadmap</span>
            </h1>
            <p className="text-lg text-muted-foreground max-w-2xl mx-auto">
              Each milestone ends with a public build and a filmed showcase. We do not start a milestone's features
              before the previous milestone is done: new systems wait, playability does not.
            </p>
          </div>

          <div className="grid lg:grid-cols-[380px_1fr] gap-8 items-start">
            {/* Left: info card */}
            <div className="lg:sticky lg:top-24">
              <div className="bg-white dark:bg-card border border-border rounded-lg p-8 text-left space-y-6 shadow-sm">
                <div className="space-y-3">
                  <div className="flex items-start gap-3">
                    <div className="w-10 h-10 rounded-lg bg-primary/10 flex items-center justify-center flex-shrink-0 mt-1">
                      <Github className="w-5 h-5 text-primary" />
                    </div>
                    <div className="flex-1">
                      <h2 className="text-xl font-bold text-foreground mb-2">Built From GitHub</h2>
                      <p className="text-sm text-foreground/80 leading-relaxed">
                        This page is generated from the{" "}
                        <a href={milestone?.url ?? `${REPO_URL}/milestones`} className="text-primary hover:underline font-semibold" target="_blank" rel="noopener noreferrer">
                          {milestone?.title ?? "M1"} milestone
                        </a>{" "}
                        and the plan in <Link to="/docs/roadmap" className="text-primary hover:underline font-semibold">docs/roadmap</Link>{" "}
                        every time the site is built.
                      </p>
                      <p className="text-xs text-muted-foreground mt-2">
                        Issues as of {fetched}
                        {source === "snapshot" ? " (committed snapshot)" : ""}.
                      </p>
                    </div>
                  </div>

                  <div className="bg-primary/5 border-l-4 border-primary rounded-r-lg p-4">
                    <p className="text-xs text-foreground font-semibold mb-2">Want to contribute?</p>
                    <p className="text-sm text-foreground/80 leading-relaxed">
                      {lookingFor > 0 ? `${lookingFor} open scopes have nobody on them. ` : ""}
                      Each one lists its files and acceptance proof. Comment on the issue, or ask on{" "}
                      <a href={DISCORD_URL} target="_blank" rel="noopener noreferrer" className="text-primary hover:underline font-semibold">
                        Discord
                      </a>
                      .
                    </p>
                  </div>
                </div>

                <div className="border-t border-border/50" />

                <div className="space-y-3">
                  <h3 className="text-sm font-bold text-foreground">Milestones</h3>
                  <ol className="space-y-3">
                    {siteData.milestones.map((m) => (
                      <li key={m.id} className="text-xs flex gap-3">
                        <span
                          className={`w-8 h-8 rounded-lg flex items-center justify-center flex-shrink-0 font-bold border-2 ${
                            m.id === CURRENT
                              ? "bg-gradient-to-br from-primary to-accent border-primary text-white shadow-lg shadow-primary/20"
                              : "border-primary/30 text-primary"
                          }`}
                        >
                          {m.id}
                        </span>
                        <span>
                          <span className={`font-bold ${m.id === CURRENT ? "text-primary" : "text-foreground"}`}>{m.name}</span>
                          <span className="block text-foreground/70 line-clamp-3" title={plainCell(m.doneWhen)}>
                            {plainCell(m.doneWhen)}
                          </span>
                        </span>
                      </li>
                    ))}
                  </ol>
                </div>

                <div className="border-t border-border/50" />

                <a
                  href={DISCORD_URL}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="flex items-center justify-center gap-2 w-full py-3 px-4 bg-[#5865F2] hover:bg-[#4752C4] text-white rounded-lg transition-colors font-semibold"
                >
                  <DiscordIcon />
                  Join Discord
                </a>
              </div>
            </div>

            {/* Right: current milestone */}
            <div className="space-y-16">
              <div className="space-y-6">
                <div className="flex items-center gap-2 mb-2">
                  <div className="px-3 py-1 rounded-full bg-primary/10 border border-primary/20">
                    <span className="text-xs font-semibold text-primary">Current Milestone</span>
                  </div>
                </div>

                <div className="space-y-4 relative bg-white dark:bg-card p-6 rounded-lg shadow-sm">
                  <div className="absolute -inset-1 bg-gradient-to-r from-primary/10 via-accent/10 to-primary/10 rounded-lg -z-10" />
                  <div className="flex items-center gap-4">
                    <div className="w-14 h-14 rounded-xl flex items-center justify-center border-2 bg-gradient-to-br from-primary to-accent border-primary shadow-lg shadow-primary/20">
                      <span className="text-xl font-bold text-white">{CURRENT}</span>
                    </div>
                    <div className="flex-1">
                      <h2 className="text-3xl font-display font-bold text-primary">
                        {siteData.milestones.find((m) => m.id === CURRENT)?.name ?? milestone?.title}
                      </h2>
                      <p className="text-base text-primary font-semibold mt-1">
                        <Link to="/docs/roadmap-m1-playtest-town" className="hover:underline inline-flex items-center gap-1">
                          <FileText className="w-4 h-4" /> Read the plan
                        </Link>
                      </p>
                    </div>
                    <div className="text-right">
                      <div className="text-2xl font-bold text-foreground">{pct}%</div>
                      <p className="text-xs text-muted-foreground">{total} scopes</p>
                    </div>
                  </div>

                  <div className="bg-white/50 dark:bg-card/50 rounded-lg p-5 border border-border">
                    <p className="text-base text-foreground/80 leading-relaxed">
                      {plainCell(siteData.milestones.find((m) => m.id === CURRENT)?.doneWhen ?? milestone?.description ?? "")}
                    </p>
                  </div>

                  <div className="space-y-1.5">
                    <ProgressBar value={pct} />
                    <div className="flex items-center justify-between text-xs text-muted-foreground">
                      <span>
                        {closed.length} done · {open.length - lookingFor} assigned · {lookingFor} looking for contributors
                      </span>
                      <span>{pct}% complete</span>
                    </div>
                  </div>
                </div>

                {open.length > 0 && (
                  <div className="grid md:grid-cols-2 gap-4">
                    {open.map((i) => (
                      <IssueCard key={i.number} issue={i} />
                    ))}
                  </div>
                )}

                {closed.length > 0 && (
                  <details className="group">
                    <summary className="cursor-pointer text-sm font-semibold text-foreground mb-4 select-none">
                      Done ({closed.length})
                    </summary>
                    <div className="grid md:grid-cols-2 gap-4">
                      {closed.map((i) => (
                        <IssueCard key={i.number} issue={i} />
                      ))}
                    </div>
                  </details>
                )}
              </div>

              {/* Scoped plan */}
              <div className="bg-white dark:bg-card p-6 rounded-lg shadow-sm space-y-4">
                <h2 className="text-2xl font-display font-bold">After M1: the scoped plan</h2>
                <p className="text-sm text-muted-foreground">
                  The release scopes are written so a human or an agent can pick one up cold: the player loop, the assets
                  it may use, the first shippable slice, and how to prove it.
                </p>
                <div className="flex flex-wrap gap-2">
                  {[
                    ["Roadmap index", "/docs/roadmap"],
                    ["Phases", "/docs/roadmap-phases"],
                    ["Release scopes", "/docs/roadmap-scopes"],
                    ["Testing plan", "/docs/roadmap-testing"],
                    ["What the scopes reuse", "/docs/roadmap-sources"],
                  ].map(([label, to]) => (
                    <Link key={to} to={to} className="px-3 py-1.5 rounded-full bg-primary/10 border border-primary/20 text-sm font-medium text-primary hover:bg-primary/20">
                      {label}
                    </Link>
                  ))}
                </div>
              </div>

              {/* 2025 archive */}
              <details className="bg-white/60 dark:bg-card/60 p-6 rounded-lg border border-border">
                <summary className="cursor-pointer select-none flex items-center gap-2 text-lg font-display font-bold">
                  <History className="w-5 h-5 text-primary" />
                  The 2025 community roadmap (archive)
                </summary>
                <p className="text-sm text-muted-foreground mt-3 mb-6">
                  The first plan, from the old site's database, exported {siteData.archive.exported}. Most of its core
                  systems now exist in the mod; the live plan is the milestone above.
                </p>
                <div className="space-y-6">
                  {siteData.archive.phases.map((p) => (
                    <div key={p.number}>
                      <h3 className="font-semibold text-foreground">
                        Phase {p.number}: {p.name}
                      </h3>
                      <p className="text-xs text-muted-foreground mb-2">{p.description}</p>
                      <ul className="space-y-1.5">
                        {p.items.map((it) => (
                          <li key={it.title} className="flex gap-2 text-sm">
                            {statusDot(it.status)}
                            <span>
                              <span className="font-medium text-foreground">{it.title}</span>
                              {it.description && <span className="text-muted-foreground"> · {it.description}</span>}
                            </span>
                          </li>
                        ))}
                      </ul>
                    </div>
                  ))}
                </div>
              </details>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Roadmap;
