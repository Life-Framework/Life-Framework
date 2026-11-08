import { Code2, Users, Heart, Zap, Scale, Puzzle, Calendar, Compass, Hand, Gamepad2, Bot, ArrowRight } from "lucide-react";
import { Link } from "react-router-dom";
import type { ReactNode } from "react";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { AnimatedBackground } from "@/components/AnimatedBackground";
import { Markdown } from "@/components/Markdown";
import { siteData } from "@/lib/content";

const values = [
  {
    icon: <Code2 className="w-8 h-8" />,
    title: "Open Source First",
    description: "We believe in the power of open collaboration. Our framework is built by the community, for the community.",
  },
  {
    icon: <Users className="w-8 h-8" />,
    title: "Passion Driven",
    description:
      "We're doing this because it's fun and cool. Everyone here is contributing their time because we love life mods and want to play them again.",
  },
  {
    icon: <Heart className="w-8 h-8" />,
    title: "Accessible to All",
    description:
      "Run it on a massive server, with friends, or just locally for your own experiments. Your server, your rules, your way.",
  },
  {
    icon: <Zap className="w-8 h-8" />,
    title: "Playable, Provable, Easy to Contribute",
    description:
      "In that order. A stranger can join and finish a loop, every claim has proof, and every piece of work is a scope anyone can pick up cold.",
  },
];

const Section = ({
  icon,
  title,
  subtitle,
  source,
  children,
  highlight,
}: {
  icon: ReactNode;
  title: string;
  subtitle?: string;
  source?: { label: string; to: string };
  children: ReactNode;
  highlight?: boolean;
}) => (
  <div className="max-w-5xl mx-auto mb-16">
    <Card className={`bg-card border-border p-8 md:p-12 ${highlight ? "border-primary/40 shadow-lg shadow-primary/10" : ""}`}>
      <div className="flex items-start gap-4 mb-6">
        <div className="w-12 h-12 rounded-lg bg-primary/10 flex items-center justify-center flex-shrink-0 text-primary">{icon}</div>
        <div className="flex-1">
          <h2 className="text-2xl font-display font-bold mb-2">{title}</h2>
          {subtitle && <p className="text-sm text-primary font-semibold">{subtitle}</p>}
        </div>
      </div>
      {children}
      {source && (
        <p className="mt-6 text-xs text-muted-foreground">
          From{" "}
          <Link to={source.to} className="text-primary hover:underline">
            {source.label}
          </Link>{" "}
          in the repository.
        </p>
      )}
    </Card>
  </div>
);

const Philosophy = () => (
  <div className="min-h-screen pt-24 pb-16 relative">
    <AnimatedBackground />
    <div className="container mx-auto px-4 relative z-10">
      <div className="max-w-3xl mx-auto text-center mb-16">
        <h1 className="text-4xl md:text-5xl font-display font-bold mb-6">
          Philosophy & <span className="text-primary">Vision</span>
        </h1>
        <p className="text-lg text-muted-foreground mb-8">
          A life/roleplay framework for Arma Reforger that other communities build on, proven by a flagship server we
          run ourselves. The ambition is the most played and most built-upon mod in Arma's history.
        </p>
        <Button asChild variant="outline" className="border-primary/50 hover:bg-primary/10">
          <Link to="/docs/direction">
            Read the full direction
            <ArrowRight className="ml-2 h-4 w-4" />
          </Link>
        </Button>
      </div>

      <Section icon={<Compass className="w-6 h-6" />} title="What We Are Building" subtitle="A framework, proven by a flagship" source={{ label: "docs/direction.md", to: "/docs/direction" }}>
        <Markdown className="compact">{siteData.building}</Markdown>
      </Section>

      <Section
        icon={<Hand className="w-6 h-6" />}
        title="In-World First"
        subtitle="The place is the interface"
        source={{ label: "docs/decisions.md", to: "/docs/decisions#gameplay" }}
        highlight
      >
        <Markdown className="compact">{siteData.inWorldFirst}</Markdown>
      </Section>

      <Section icon={<Gamepad2 className="w-6 h-6" />} title="What Playable Means" subtitle="The loop a stranger finishes without help" source={{ label: "docs/direction.md", to: "/docs/direction" }}>
        <Markdown className="compact">{siteData.playable}</Markdown>
      </Section>

      {siteData.aiFirst && (
        <Section icon={<Bot className="w-6 h-6" />} title="How We Build: AI-First" subtitle="Short, true contract; proof for every claim" source={{ label: "docs/direction.md", to: "/docs/direction" }}>
          <Markdown className="compact">{siteData.aiFirst}</Markdown>
        </Section>
      )}

      <Section icon={<Scale className="w-6 h-6" />} title="Free & Open for Everyone" subtitle="MIT Licensed">
        <p className="text-muted-foreground mb-4">
          Life Framework is released under the <span className="font-semibold text-foreground">MIT License</span>. We
          grow through adoption and reputation; the framework is never monetized. Servers may follow Bohemia's rules on
          their own.
        </p>
        <p className="text-muted-foreground">
          Whether you're a solo modder, a small community server, or a large gaming network, Life Framework is yours to
          use, modify, and build upon freely.
        </p>
      </Section>

      <Section icon={<Puzzle className="w-6 h-6" />} title="Built for Extensibility" subtitle="Host, configure, and extend without forking">
        <p className="text-muted-foreground mb-4">
          The framework is the product: modular systems for economy, jobs, crime, police, ownership, and persistence
          that a server owner can host, configure, and extend without forking.
        </p>
        <ul className="space-y-2 text-muted-foreground">
          <li className="flex items-start gap-2">
            <span className="text-primary mt-1">•</span>
            <span>Prices, timers, and prefabs come from config and content, never hard-coded in systems</span>
          </li>
          <li className="flex items-start gap-2">
            <span className="text-primary mt-1">•</span>
            <span>Server owners mix and match features to create their own experience</span>
          </li>
          <li className="flex items-start gap-2">
            <span className="text-primary mt-1">•</span>
            <span>Add a job or a shop without writing script (the M3 framework release goal)</span>
          </li>
        </ul>
      </Section>

      <Section icon={<Calendar className="w-6 h-6" />} title="About the Base Mod" subtitle="1989 Everon is a content pack, not an assumption">
        <p className="text-muted-foreground mb-4">
          The flagship is set in <span className="font-semibold text-foreground">1989 Everon</span> to make the best
          use of Arma Reforger's native vehicles, weapons, and buildings. A taxi rank, a fuel delivery, a parcel run. No
          phone apps.
        </p>
        <p className="text-muted-foreground">
          The core is <span className="font-semibold text-foreground">era-agnostic</span>: a modern or alternate setting
          must be buildable on the same systems, with asset packs shifting the era without rewriting gameplay.
        </p>
      </Section>

      <div className="max-w-5xl mx-auto">
        <h2 className="text-3xl font-display font-bold text-center mb-12">
          Core <span className="text-primary">Values</span>
        </h2>
        <div className="grid md:grid-cols-2 gap-8">
          {values.map((value) => (
            <Card key={value.title} className="bg-card border-border p-6 hover:border-foreground transition-all duration-300 group">
              <div className="text-primary mb-4">{value.icon}</div>
              <h3 className="text-xl font-display font-bold mb-3">{value.title}</h3>
              <p className="text-muted-foreground">{value.description}</p>
            </Card>
          ))}
        </div>
      </div>
    </div>
  </div>
);

export default Philosophy;
