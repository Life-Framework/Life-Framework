import type { ReactNode } from "react";
import { Link } from "react-router-dom";
import { Hammer, Layers, Code, Map, Sparkles, Globe, Users, BookOpen, GitPullRequest, Bug, Gamepad2, ExternalLink, UserPlus } from "lucide-react";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { AnimatedBackground } from "@/components/AnimatedBackground";
import { DISCORD_URL, REPO_URL, githubData, type Issue } from "@/lib/content";

const blob = (p: string) => `${REPO_URL}/blob/main/${p}`;
const issuesUrl = (q: string) => `${REPO_URL}/issues?q=${encodeURIComponent(q)}`;

const steps: { icon: ReactNode; title: string; body: ReactNode }[] = [
  {
    icon: <BookOpen className="w-5 h-5" />,
    title: "1. Read the contract",
    body: (
      <>
        <a href={blob("CONTRIBUTING.md")} className="text-primary hover:underline" target="_blank" rel="noopener noreferrer">CONTRIBUTING.md</a>{" "}
        for the community side, <a href={blob("docs/setup.md")} className="text-primary hover:underline" target="_blank" rel="noopener noreferrer">docs/setup.md</a>{" "}
        for the toolchain, and <a href={blob("AGENTS.md")} className="text-primary hover:underline" target="_blank" rel="noopener noreferrer">AGENTS.md</a>{" "}
        before you touch script or prefabs. It is the one rulebook for humans and AI agents.
      </>
    ),
  },
  {
    icon: <Hammer className="w-5 h-5" />,
    title: "2. Pick a scope",
    body: (
      <>
        Every piece of work is a GitHub issue written as a scope: the files, the acceptance proof, no hidden context.
        Start with an issue labelled <code>good first issue</code> or an unassigned <code>m1</code> scope, and comment
        that you are on it.
      </>
    ),
  },
  {
    icon: <GitPullRequest className="w-5 h-5" />,
    title: "3. Open a pull request",
    body: (
      <>
        Work on a branch, prove it (tests that can go red, a screenshot or clip from the world), and open a PR. CI
        runs <code>tools\cli validate</code> on every PR. Docs and this website are edited the same way.
      </>
    ),
  },
];

const areas = [
  { icon: <Code className="w-5 h-5" />, label: "Scripting", description: "Enforce Script systems and tests", href: blob("AGENTS.md") },
  { icon: <Map className="w-5 h-5" />, label: "Mapping", description: "DebugWorld sites and the flagship town", href: blob("docs/roadmap/m1-playtest-town.md") },
  { icon: <Layers className="w-5 h-5" />, label: "3D & assets", description: "Props, signs, real in-world items", href: blob("assets-source/LICENSES.md") },
  { icon: <Sparkles className="w-5 h-5" />, label: "UI/UX", description: "Only where in-world adds friction", href: blob("docs/decisions.md") },
  { icon: <Gamepad2 className="w-5 h-5" />, label: "Playtesting", description: "Join a session, file what broke", href: blob("docs/playtest.md") },
  { icon: <Globe className="w-5 h-5" />, label: "Website & docs", description: "website/ and docs/ in the same repo", href: `${REPO_URL}/tree/main/website` },
];

const ScopeRow = ({ issue }: { issue: Issue }) => (
  <a href={issue.url} target="_blank" rel="noopener noreferrer" className="flex items-start gap-3 p-3 rounded-lg hover:bg-primary/5 transition-colors group">
    <span className="text-xs font-mono text-muted-foreground mt-0.5 w-10 flex-shrink-0">#{issue.number}</span>
    <span className="flex-1 min-w-0">
      <span className="text-sm font-medium text-foreground group-hover:text-primary">{issue.title.replace(/^M\d+:\s*/, "")}</span>
      <span className="flex flex-wrap gap-1 mt-1">
        {issue.labels.map((l) => (
          <Badge key={l} variant="outline" className="text-[10px] py-0">
            {l}
          </Badge>
        ))}
      </span>
    </span>
    <ExternalLink className="w-3.5 h-3.5 text-muted-foreground group-hover:text-primary mt-1" />
  </a>
);

const Contribute = () => {
  const openM1 = githubData.issues.filter((i) => i.state === "open" && i.assignees.length === 0);
  const goodFirst = githubData.goodFirst;
  const pick = goodFirst.length > 0 ? goodFirst : openM1;

  return (
    <div className="min-h-screen pt-24 pb-16 relative">
      <AnimatedBackground />
      <div className="container mx-auto px-4 relative z-10">
        <div className="max-w-3xl mx-auto text-center mb-12">
          <div className="inline-block px-4 py-2 bg-primary/10 border border-primary/20 rounded-full text-sm font-medium mb-6 backdrop-blur-sm">
            🛠️ Everything happens on GitHub
          </div>
          <h1 className="text-4xl md:text-5xl font-display font-bold mb-6">
            Help Build <span className="text-primary">Life Framework</span>
          </h1>
          <p className="text-lg text-muted-foreground">
            No forms and no sign-up. The mod, the docs, and this site live in one repository, and every change is a pull
            request. Whether you're an experienced developer or just getting started, there is a scope for you.
          </p>
          <div className="flex flex-col sm:flex-row gap-3 justify-center mt-8">
            <Button asChild size="lg" className="bg-primary hover:bg-primary/90 text-primary-foreground shadow-lg shadow-primary/20">
              <a href={issuesUrl('is:open is:issue label:"good first issue"')} target="_blank" rel="noopener noreferrer">
                <UserPlus className="w-4 h-4 mr-2" />
                Good first issues
              </a>
            </Button>
            <Button asChild size="lg" variant="outline" className="border-primary/50 hover:bg-primary/10">
              <a href={issuesUrl("is:open is:issue label:m1")} target="_blank" rel="noopener noreferrer">
                Open M1 scopes
              </a>
            </Button>
          </div>
        </div>

        <div className="grid md:grid-cols-3 gap-6 max-w-6xl mx-auto mb-16">
          {steps.map((s) => (
            <Card key={s.title} className="bg-card/70 backdrop-blur-sm border-border p-6">
              <div className="w-10 h-10 rounded-lg bg-primary/10 flex items-center justify-center text-primary mb-4">{s.icon}</div>
              <h2 className="text-lg font-display font-bold mb-2">{s.title}</h2>
              <p className="text-sm text-muted-foreground leading-relaxed">{s.body}</p>
            </Card>
          ))}
        </div>

        <div className="grid lg:grid-cols-[1fr_360px] gap-8 max-w-6xl mx-auto mb-16 items-start">
          <Card className="bg-card/70 backdrop-blur-sm border-border p-6">
            <div className="flex items-center justify-between mb-4">
              <h2 className="text-xl font-display font-bold">
                {goodFirst.length > 0 ? "Good first issues" : "M1 scopes nobody is on yet"}
              </h2>
              <span className="text-xs text-muted-foreground">{pick.length} open</span>
            </div>
            {pick.length === 0 ? (
              <p className="text-sm text-muted-foreground">
                Nothing unclaimed right now. Check the{" "}
                <Link to="/roadmap" className="text-primary hover:underline">roadmap</Link> or ask on Discord what needs a hand.
              </p>
            ) : (
              <div className="divide-y divide-border/50 max-h-[32rem] overflow-y-auto -mx-3">
                {pick.map((i) => (
                  <ScopeRow key={i.number} issue={i} />
                ))}
              </div>
            )}
          </Card>

          <div className="space-y-6">
            <Card className="bg-card/70 backdrop-blur-sm border-border p-6">
              <h2 className="text-lg font-display font-bold mb-4">Where you can help</h2>
              <div className="space-y-2">
                {areas.map((a) => (
                  <a key={a.label} href={a.href} target="_blank" rel="noopener noreferrer" className="flex items-center gap-3 p-2 rounded-lg hover:bg-primary/5 group">
                    <span className="w-9 h-9 rounded-lg bg-primary/10 flex items-center justify-center text-primary flex-shrink-0">{a.icon}</span>
                    <span>
                      <span className="block text-sm font-semibold group-hover:text-primary">{a.label}</span>
                      <span className="block text-xs text-muted-foreground">{a.description}</span>
                    </span>
                  </a>
                ))}
              </div>
            </Card>

            <Card className="bg-card/70 backdrop-blur-sm border-border p-6 space-y-3">
              <h2 className="text-lg font-display font-bold">Found a bug or played a session?</h2>
              <p className="text-sm text-muted-foreground">
                File it on GitHub with the bug or playtest template, or post it in the Discord forums.
              </p>
              <div className="flex flex-col gap-2">
                <Button asChild variant="outline" size="sm" className="justify-start gap-2">
                  <a href={`${REPO_URL}/issues/new/choose`} target="_blank" rel="noopener noreferrer">
                    <Bug className="w-4 h-4" />
                    Open an issue
                  </a>
                </Button>
                <Button asChild size="sm" className="justify-start gap-2 bg-[#5865F2] hover:bg-[#4752C4] text-white">
                  <a href={DISCORD_URL} target="_blank" rel="noopener noreferrer">
                    <Users className="w-4 h-4" />
                    Join Discord
                  </a>
                </Button>
              </div>
            </Card>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Contribute;
