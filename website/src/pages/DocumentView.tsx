import { useEffect, useState } from "react";
import { useParams, Link, useLocation } from "react-router-dom";
import { ArrowLeft, Pencil, Github } from "lucide-react";
import { AnimatedBackground } from "@/components/AnimatedBackground";
import { Button } from "@/components/ui/button";
import { Markdown } from "@/components/Markdown";
import { DocTree } from "@/components/DocTree";
import { useDocs } from "@/lib/useDocs";
import { REPO_URL } from "@/lib/content";

const DEFAULT_TITLE = "Life Framework | Community-Driven Arma Reforger Framework";

const DocumentView = () => {
  const { slug } = useParams();
  const { hash } = useLocation();
  const docs = useDocs();
  const doc = docs?.find((d) => d.slug === slug);
  const [activeId, setActiveId] = useState("");

  useEffect(() => {
    if (doc) document.title = `${doc.title} | Life Framework`;
    return () => {
      document.title = DEFAULT_TITLE;
    };
  }, [doc]);

  // The doc renders after the docs chunk loads, so jump to the hash afterwards.
  useEffect(() => {
    if (!doc || !hash) return;
    const el = document.getElementById(decodeURIComponent(hash.slice(1)));
    if (el) el.scrollIntoView();
  }, [doc, hash]);

  useEffect(() => {
    if (!doc) return;
    const els = doc.headings.map((h) => document.getElementById(h.id)).filter((e): e is HTMLElement => !!e);
    const observer = new IntersectionObserver(
      (entries) => {
        for (const e of entries) if (e.isIntersecting) setActiveId(e.target.id);
      },
      { rootMargin: "-100px 0px -66%" },
    );
    els.forEach((e) => observer.observe(e));
    return () => observer.disconnect();
  }, [doc]);

  if (!docs) {
    return (
      <div className="min-h-screen pt-24 pb-16 flex items-center justify-center">
        <div className="text-center">
          <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary mx-auto mb-4" />
          <p className="text-muted-foreground">Loading document...</p>
        </div>
      </div>
    );
  }

  if (!doc) {
    return (
      <div className="min-h-screen pt-24 pb-16 flex items-center justify-center relative">
        <AnimatedBackground />
        <div className="text-center relative z-10">
          <h2 className="text-2xl font-bold mb-4">Document not found</h2>
          <Button asChild>
            <Link to="/documentation">Back to Documentation</Link>
          </Button>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen relative">
      <AnimatedBackground />
      <div className="flex pt-16 relative z-10">
        <aside className="hidden lg:block w-72 flex-shrink-0 border-r border-border bg-background/50 backdrop-blur-sm">
          <div className="sticky top-16 h-[calc(100vh-4rem)] overflow-y-auto py-6 px-4">
            <Link to="/documentation" className="text-sm font-semibold mb-4 block hover:text-primary">
              Documentation
            </Link>
            <DocTree docs={docs} currentSlug={slug} />
          </div>
        </aside>

        <main className="flex-1 min-w-0">
          <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-12 py-8">
            <div className="flex items-center justify-between mb-4">
              <Button asChild variant="ghost" size="sm">
                <Link to="/documentation">
                  <ArrowLeft className="w-4 h-4 mr-2" />
                  Back to Docs
                </Link>
              </Button>
              <Button asChild variant="outline" size="sm" className="gap-2">
                <a href={doc.editUrl} target="_blank" rel="noopener noreferrer">
                  <Pencil className="w-4 h-4" />
                  Edit on GitHub
                </a>
              </Button>
            </div>
            <article className="bg-white dark:bg-card rounded-lg shadow-sm p-6 sm:p-8 lg:p-12">
              <h1 className="text-4xl font-display font-bold mb-3 text-foreground">{doc.title}</h1>
              <div className="flex flex-wrap items-center gap-3 text-xs text-muted-foreground mb-8">
                <a
                  href={`${REPO_URL}/blob/main/${doc.repoPath}`}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="inline-flex items-center gap-1 font-mono hover:text-primary"
                >
                  <Github className="w-3.5 h-3.5" />
                  {doc.repoPath}
                </a>
                {doc.source === "lovable-export" && (
                  <span className="px-2 py-0.5 bg-muted/40 rounded font-semibold">
                    From the 2025 site{doc.updated ? `, ${doc.updated}` : ""}
                  </span>
                )}
              </div>
              <Markdown>{doc.markdown}</Markdown>
            </article>
          </div>
        </main>

        {doc.headings.length > 1 && (
          <aside className="hidden xl:block w-64 flex-shrink-0 border-l border-border bg-background/50 backdrop-blur-sm">
            <div className="sticky top-16 h-[calc(100vh-4rem)] overflow-y-auto py-6 px-4">
              <h3 className="text-sm font-semibold mb-3">On This Page</h3>
              <nav className="space-y-1">
                {doc.headings.map((h) => (
                  <a
                    key={h.id}
                    href={`#${h.id}`}
                    className={`block text-sm py-1 px-2 rounded transition-colors ${
                      h.level === 3 ? "pl-5" : h.level === 2 ? "pl-3" : "pl-2"
                    } ${
                      activeId === h.id
                        ? "text-primary font-medium bg-primary/10"
                        : "text-muted-foreground hover:text-foreground hover:bg-muted/30"
                    }`}
                  >
                    {h.text}
                  </a>
                ))}
              </nav>
            </div>
          </aside>
        )}
      </div>
    </div>
  );
};

export default DocumentView;
