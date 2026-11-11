// Page data that is too big for the main bundle (docs, decisions, devlog) loads as its own chunk.
// The prerender primes the cache with every file; the client preloads the current page's file
// before hydrating, so the first render matches the static HTML.
import { useEffect, useState } from "react";

type Loader = () => Promise<{ default: unknown }>;
const loaders = import.meta.glob<{ default: unknown }>("../generated/data/*.json") as Record<string, Loader>;
const cache = new Map<string, unknown>();
const fileOf = (key: string) => `../generated/data/${key}.json`;

export const hasData = (key: string) => fileOf(key) in loaders;

export function prime(all: Record<string, { default: unknown }>) {
  for (const [file, mod] of Object.entries(all)) cache.set(/([^/]+)\.json$/.exec(file)![1], mod.default);
}

export async function preload(key: string | null): Promise<void> {
  if (!key || cache.has(key) || !hasData(key)) return;
  cache.set(key, (await loaders[fileOf(key)]()).default);
}

// The data file a route needs, if any.
export function routeDataKey(pathname: string): string | null {
  if (pathname === "/decisions") return "decisions";
  if (pathname === "/devlog") return "devlog";
  if (pathname === "/gallery") return "gallery";
  if (pathname === "/credits") return "doc-credits";
  const m = /^\/docs\/([^/]+)\/?$/.exec(pathname);
  return m ? `doc-${m[1]}` : null;
}

// undefined while loading, null when there is no such file.
export function useData<T>(key: string): T | null | undefined {
  const [value, setValue] = useState<T | null | undefined>(() => (cache.has(key) ? (cache.get(key) as T) : hasData(key) ? undefined : null));
  useEffect(() => {
    if (cache.has(key)) {
      setValue(cache.get(key) as T);
      return;
    }
    if (!hasData(key)) {
      setValue(null);
      return;
    }
    let live = true;
    setValue(undefined);
    preload(key).then(() => live && setValue(cache.get(key) as T));
    return () => {
      live = false;
    };
  }, [key]);
  return value;
}
