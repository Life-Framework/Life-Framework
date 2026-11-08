import { Link } from "react-router-dom";
import { FileText, Github } from "lucide-react";
import { AnimatedBackground } from "@/components/AnimatedBackground";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { groupDocs } from "@/lib/docs";
import { useDocs } from "@/lib/useDocs";
import { REPO_URL } from "@/lib/content";

const blurbs: Record<string, string> = {
  Project: "Where we are going, and the design calls that set the taste.",
  Roadmap: "The milestone plan, the phases, and every release scope with its acceptance proof.",
  "Community guides": "Guides the community wrote on the first site, kept as markdown in website/content/docs.",
};

const Documentation = () => {
  const docs = useDocs();

  return (
    <div className="min-h-screen pt-24 pb-16 relative">
      <AnimatedBackground />
      <div className="container mx-auto px-4 relative z-10">
        <div className="max-w-4xl mx-auto">
          <div className="flex flex-col sm:flex-row sm:items-end justify-between gap-4 mb-8">
            <div>
              <h1 className="text-4xl font-display font-bold mb-2">
                <span className="text-primary">Documentation</span>
              </h1>
              <p className="text-muted-foreground">
                Rendered from the repository on every merge, so the docs always match the mod. Edits are pull requests.
              </p>
            </div>
            <Button asChild variant="outline" size="sm" className="gap-2 self-start sm:self-auto">
              <a href={`${REPO_URL}/tree/main/docs`} target="_blank" rel="noopener noreferrer">
                <Github className="w-4 h-4" />
                docs/ on GitHub
              </a>
            </Button>
          </div>

          {!docs ? (
            <div className="text-center py-16">
              <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary mx-auto mb-4" />
              <p className="text-muted-foreground">Loading documentation...</p>
            </div>
          ) : (
            <div className="space-y-10">
              {groupDocs(docs).map(([group, list]) => (
                <section key={group}>
                  <h2 className="text-2xl font-display font-bold mb-1">{group}</h2>
                  <p className="text-sm text-muted-foreground mb-4">{blurbs[group]}</p>
                  <div className="grid sm:grid-cols-2 gap-3">
                    {list.map((d) => (
                      <Link key={d.slug} to={`/docs/${d.slug}`} className="group">
                        <Card className="h-full bg-card/70 backdrop-blur-sm border-border p-4 group-hover:border-primary/50 transition-colors">
                          <div className="flex items-start gap-3">
                            <FileText className="w-4 h-4 text-primary mt-1 flex-shrink-0" />
                            <div className="min-w-0">
                              <h3 className="font-semibold text-foreground group-hover:text-primary transition-colors">{d.title}</h3>
                              <p className="text-xs text-muted-foreground font-mono truncate mb-1">{d.repoPath}</p>
                              {d.description && <p className="text-sm text-muted-foreground line-clamp-2">{d.description}</p>}
                            </div>
                          </div>
                        </Card>
                      </Link>
                    ))}
                  </div>
                </section>
              ))}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default Documentation;
