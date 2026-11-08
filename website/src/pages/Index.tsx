import { Button } from "@/components/ui/button";
import { Card } from "@/components/ui/card";
import { ArrowRight, BookOpen, X, Sparkles, UserPlus, Map } from "lucide-react";
import { Link } from "react-router-dom";
import { useState, useEffect } from "react";
import { AnimatedBackground } from "@/components/AnimatedBackground";
import { ProgressBar } from "@/components/ProgressBar";
import { githubData, siteData } from "@/lib/content";

const BANNER_KEY = "m1BannerDismissed";

function readDismissed(): boolean {
  try {
    return localStorage.getItem(BANNER_KEY) === "true";
  } catch {
    return false;
  }
}

const features = [
  {
    title: "For the Community, By the Community",
    description:
      "Just like Altis Life, this is built by passionate modders who love roleplay. Every voice matters, every contribution counts.",
  },
  {
    title: "Truly Open Source",
    description: "MIT licensed with no strings attached. Fork it, modify it, make it yours. This belongs to everyone.",
  },
  {
    title: "Built to Share",
    description:
      "We're creating something that benefits every server owner and player. No gatekeeping, no paywalls, no restrictions.",
  },
  {
    title: "Giving Back",
    description:
      "The Arma community gave us Altis Life. Now it's our turn to create the next generation of roleplay for Reforger.",
  },
];

const Index = () => {
  const [showBanner, setShowBanner] = useState(true);
  useEffect(() => {
    if (readDismissed()) setShowBanner(false);
  }, []);
  const handleDismiss = () => {
    setShowBanner(false);
    try {
      localStorage.setItem(BANNER_KEY, "true");
    } catch {
      // Storage is blocked; the banner comes back on the next visit.
    }
  };

  const m1 = githubData.milestone;
  const m1Total = m1 ? m1.open + m1.closed : 0;
  const m1Pct = m1Total ? Math.round((m1!.closed / m1Total) * 100) : 0;
  const m1Row = siteData.milestones.find((m) => m.id === "M1");

  return (
    <div className="min-h-screen relative">
      <AnimatedBackground />
      <div className="relative z-10">
        {showBanner && (
          <div className="relative bg-gradient-to-r from-black via-blue-950 to-black pt-16 overflow-hidden">
            <div className="absolute inset-0 bg-gradient-to-r from-blue-600/20 via-blue-500/30 to-blue-600/20" />
            <div className="absolute inset-0">
              <div
                className="absolute inset-0 opacity-30"
                style={{
                  backgroundImage: `
                  linear-gradient(to right, rgba(59, 130, 246, 0.3) 1px, transparent 1px),
                  linear-gradient(to bottom, rgba(59, 130, 246, 0.3) 1px, transparent 1px)
                `,
                  backgroundSize: "30px 30px",
                }}
              />
            </div>
            <div className="container mx-auto px-4 py-4 relative z-10">
              <div className="flex items-center justify-between gap-4 max-w-5xl mx-auto">
                <div className="flex items-center gap-3 flex-1">
                  <div className="hidden sm:flex items-center justify-center w-10 h-10 rounded-full bg-blue-500/30 backdrop-blur-sm border border-blue-400/50 shadow-lg shadow-blue-500/50">
                    <Sparkles className="w-5 h-5 text-blue-300 drop-shadow-lg" />
                  </div>
                  <div className="flex-1">
                    <p className="text-white font-bold text-sm sm:text-base drop-shadow-md">
                      {m1 ? `${m1.title}: ${m1.closed} of ${m1Total} scopes done` : "M1 Playtest Town is in progress"}
                    </p>
                    <p className="text-blue-200 text-xs sm:text-sm drop-shadow-md font-medium">
                      Every scope is a GitHub issue that carries its own acceptance proof. Pick one up.
                    </p>
                  </div>
                </div>
                <div className="flex items-center gap-2">
                  <Button asChild size="sm" className="bg-blue-500 text-white hover:bg-blue-400 font-bold shadow-lg shadow-blue-500/50 whitespace-nowrap">
                    <Link to="/contribute">
                      Join Now
                      <ArrowRight className="ml-1 h-3 w-3" />
                    </Link>
                  </Button>
                  <Button
                    size="icon"
                    variant="ghost"
                    aria-label="Dismiss"
                    className="h-8 w-8 text-blue-200 hover:bg-blue-500/20 backdrop-blur-sm"
                    onClick={handleDismiss}
                  >
                    <X className="h-4 w-4 drop-shadow-md" />
                  </Button>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Hero */}
        <section className={`relative ${showBanner ? "pt-16" : "pt-32"} pb-20 px-4`}>
          <div className="container mx-auto">
            <div className="max-w-5xl mx-auto text-center">
              <div className="inline-block px-4 py-2 bg-primary/10 border border-primary/20 rounded-full text-sm font-medium mb-6 backdrop-blur-sm">
                🎯 Inspired by Altis Life
              </div>
              <h1 className="text-5xl md:text-7xl font-display font-bold mb-6 leading-tight text-foreground">
                Building the Future of{" "}
                <span className="relative inline-block">
                  <span className="relative z-10 text-primary font-bold">Arma Life</span>
                  <span className="absolute bottom-2 left-0 w-full h-4 bg-primary/20 -z-10" />
                </span>{" "}
                Roleplay
              </h1>

              <div className="flex flex-wrap items-center justify-center gap-6 mb-8 text-sm">
                <div className="flex items-center gap-2 text-muted-foreground">
                  <div className="w-2 h-2 rounded-full bg-primary animate-pulse" />
                  <span>Framework + flagship server</span>
                </div>
                <div className="flex items-center gap-2 text-muted-foreground">
                  <div className="w-2 h-2 rounded-full bg-green-500" />
                  <span>100% Open Source (MIT)</span>
                </div>
                <div className="flex items-center gap-2 text-muted-foreground">
                  <div className="w-2 h-2 rounded-full bg-accent" />
                  <span>Community Owned</span>
                </div>
              </div>

              <div className="bg-gradient-to-r from-primary/5 via-accent/5 to-primary/5 border border-primary/20 rounded-lg p-6 mb-8 max-w-3xl mx-auto">
                <p className="text-foreground font-semibold mb-3 flex items-center justify-center gap-2">
                  <span className="text-2xl">❤️</span>
                  <span>A Love Letter to the Community</span>
                </p>
                <p className="text-sm text-muted-foreground mb-2">
                  Altis Life showed us what's possible when a community comes together around open-source roleplay. It
                  was free, it was collaborative, and it created countless amazing memories.
                </p>
                <p className="text-sm text-muted-foreground">
                  <span className="font-semibold text-foreground">This is our chance to give back.</span> We're not
                  building this for profit or control. We're building a framework that other communities build on, proven
                  by a 1989 Everon server we run ourselves. Everyone can enjoy it, modify it, and make it their own.
                </p>
              </div>

              <div className="flex flex-col sm:flex-row gap-4 justify-center">
                <Button asChild size="lg" className="bg-primary hover:bg-primary/90 text-primary-foreground group shadow-lg shadow-primary/20">
                  <Link to="/contribute">
                    <UserPlus className="mr-2 h-4 w-4" />
                    Start Contributing
                    <ArrowRight className="ml-2 h-4 w-4 group-hover:translate-x-1 transition-transform" />
                  </Link>
                </Button>
                <Button asChild size="lg" variant="outline" className="border-border hover:border-primary group">
                  <Link to="/roadmap">
                    <BookOpen className="mr-2 h-4 w-4" />
                    View Our Vision & Roadmap
                  </Link>
                </Button>
              </div>
            </div>
          </div>
        </section>

        {/* Current milestone */}
        <section className="py-12 px-4">
          <div className="container mx-auto">
            <Card className="bg-card border-border p-8 max-w-5xl mx-auto">
              <div className="flex flex-col md:flex-row md:items-center gap-6">
                <div className="w-14 h-14 rounded-xl flex items-center justify-center bg-gradient-to-br from-primary to-accent border-2 border-primary shadow-lg shadow-primary/20 flex-shrink-0">
                  <Map className="w-6 h-6 text-white" />
                </div>
                <div className="flex-1">
                  <p className="text-xs font-semibold text-primary uppercase tracking-wide mb-1">Current milestone</p>
                  <h2 className="text-2xl font-display font-bold mb-2">
                    {m1Row ? `${m1Row.id} ${m1Row.name}` : "M1 Playtest Town"}
                  </h2>
                  {m1Row && <p className="text-sm text-muted-foreground">{m1Row.doneWhen.replace(/`/g, "")}</p>}
                </div>
                {m1 && (
                  <div className="md:w-48 flex-shrink-0">
                    <div className="text-3xl font-bold text-foreground">{m1Pct}%</div>
                    <ProgressBar value={m1Pct} className="my-2" />
                    <p className="text-xs text-muted-foreground">
                      {m1.closed} closed · {m1.open} open
                    </p>
                  </div>
                )}
              </div>
              <div className="mt-6 flex flex-wrap gap-3">
                <Button asChild size="sm" className="bg-primary hover:bg-primary/90 text-primary-foreground">
                  <Link to="/roadmap">
                    See the roadmap
                    <ArrowRight className="ml-2 h-4 w-4" />
                  </Link>
                </Button>
                <Button asChild size="sm" variant="outline">
                  <Link to="/docs/roadmap-m1-playtest-town">Read the M1 plan</Link>
                </Button>
              </div>
            </Card>
          </div>
        </section>

        {/* Promise */}
        <section className="py-20 px-4">
          <div className="container mx-auto">
            <div className="max-w-6xl mx-auto">
              <div className="text-center mb-16">
                <h2 className="text-3xl md:text-4xl font-display font-bold mb-4">
                  Our <span className="text-primary">Promise</span> to the Community
                </h2>
                <p className="text-muted-foreground max-w-2xl mx-auto">
                  Following in Altis Life's footsteps, we're committed to creating something that truly belongs to
                  everyone.
                </p>
              </div>

              <div className="grid md:grid-cols-2 gap-8">
                {features.map((feature) => (
                  <Card key={feature.title} className="bg-card border-border p-8 hover:border-foreground transition-all duration-300 group">
                    <h3 className="text-xl font-display font-bold mb-3 group-hover:text-primary transition-colors">
                      {feature.title}
                    </h3>
                    <p className="text-muted-foreground">{feature.description}</p>
                  </Card>
                ))}
              </div>
            </div>
          </div>
        </section>

        {/* CTA */}
        <section className="py-20 px-4">
          <div className="container mx-auto">
            <Card className="bg-card border-border p-12 text-center max-w-4xl mx-auto">
              <h2 className="text-3xl md:text-4xl font-display font-bold mb-4">
                Be Part of <span className="text-primary">Something Special</span>
              </h2>
              <p className="text-muted-foreground mb-8 max-w-2xl mx-auto">
                Every change to the mod, the docs, and this site is a pull request. Find a scope, open a PR, and help
                build the next great Arma roleplay framework.
              </p>
              <div className="flex flex-col sm:flex-row gap-4 justify-center">
                <Button asChild size="lg" className="bg-primary hover:bg-primary/90 text-primary-foreground">
                  <Link to="/team">
                    <UserPlus className="mr-2 h-4 w-4" />
                    Join the Founding Team
                  </Link>
                </Button>
                <Button asChild size="lg" variant="outline" className="border-border hover:border-foreground">
                  <Link to="/philosophy">
                    <BookOpen className="mr-2 h-4 w-4" />
                    View Our Vision
                  </Link>
                </Button>
              </div>
            </Card>
          </div>
        </section>
      </div>
    </div>
  );
};

export default Index;
