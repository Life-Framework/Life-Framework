import { DOC_GROUPS, type Doc } from "@/lib/content";

export function groupDocs(docs: Doc[]): [string, Doc[]][] {
  const groups = new Map<string, Doc[]>();
  for (const g of DOC_GROUPS) groups.set(g, []);
  for (const d of docs) {
    if (!groups.has(d.group)) groups.set(d.group, []);
    groups.get(d.group)!.push(d);
  }
  for (const list of groups.values()) list.sort((a, b) => a.order - b.order || a.title.localeCompare(b.title));
  return [...groups.entries()].filter(([, list]) => list.length > 0);
}
