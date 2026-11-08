import { Link } from "react-router-dom";
import { FileText } from "lucide-react";
import type { Doc } from "@/lib/content";
import { groupDocs } from "@/lib/docs";

// Roadmap docs nest by folder: scopes/crime/jailbreak.md sits under scopes/README.md.
function depth(d: Doc): number {
  if (d.group !== "Roadmap" || d.slug === "roadmap") return 0;
  const parts = d.repoPath.replace(/^docs\/roadmap\//, "").split("/");
  return parts[parts.length - 1] === "README.md" ? parts.length - 1 : parts.length;
}

export const DocTree = ({ docs, currentSlug }: { docs: Doc[]; currentSlug?: string }) => (
  <div className="space-y-6">
    {groupDocs(docs).map(([group, list]) => (
      <div key={group}>
        <h4 className="text-xs font-semibold uppercase tracking-wide text-muted-foreground mb-2">{group}</h4>
        <div className="space-y-0.5">
          {list.map((d) => {
            const active = d.slug === currentSlug;
            return (
              <Link
                key={d.slug}
                to={`/docs/${d.slug}`}
                className={`flex items-center gap-2 py-1.5 px-2 rounded text-sm transition-colors ${
                  active ? "text-primary font-medium bg-primary/10" : "text-foreground hover:text-primary hover:bg-muted/30"
                }`}
                style={{ paddingLeft: `${0.5 + depth(d) * 0.75}rem` }}
              >
                <FileText className="w-3.5 h-3.5 text-muted-foreground flex-shrink-0" />
                <span className="truncate">{d.title}</span>
              </Link>
            );
          })}
        </div>
      </div>
    ))}
  </div>
);
