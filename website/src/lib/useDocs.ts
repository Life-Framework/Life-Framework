import { useEffect, useState } from "react";
import { loadDocs, type Doc } from "@/lib/content";

export function useDocs(): Doc[] | null {
  const [docs, setDocs] = useState<Doc[] | null>(null);
  useEffect(() => {
    let live = true;
    loadDocs().then((d) => {
      if (live) setDocs(d);
    });
    return () => {
      live = false;
    };
  }, []);
  return docs;
}
