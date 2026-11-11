// The playtest download: one stable URL that always serves the newest release
// (tools/release/pack-playtest.mjs, .github/workflows/playtest-release.yml).
import { useEffect, useState } from "react";
import { REPO, githubData, type PlaytestRelease } from "@/lib/content";

export const PLAYTEST_ZIP = "LifeFramework-Playtest.zip";
export const PLAYTEST_DOWNLOAD_URL = `https://github.com/${REPO}/releases/latest/download/${PLAYTEST_ZIP}`;

const CACHE_KEY = "lf-playtest-release";

export const formatSize = (bytes: number) => `${Math.round(bytes / 1048576)} MB`;

// The release written at build time, then the live one from the GitHub API once the page runs,
// so a release published after the last site build still shows its own version and size.
export function usePlaytestRelease(): PlaytestRelease | null {
  const [rel, setRel] = useState<PlaytestRelease | null>(githubData.playtestRelease ?? null);
  useEffect(() => {
    let cancelled = false;
    try {
      const cached = sessionStorage.getItem(CACHE_KEY);
      if (cached) {
        setRel(JSON.parse(cached));
        return;
      }
    } catch {
      /* no storage: fetch every time */
    }
    fetch(`https://api.github.com/repos/${REPO}/releases/latest`, { headers: { Accept: "application/vnd.github+json" } })
      .then((r) => (r.ok ? r.json() : null))
      .then((body) => {
        const asset = body?.assets?.find((a: { name: string }) => a.name === PLAYTEST_ZIP);
        if (cancelled || !asset) return;
        const live: PlaytestRelease = { tag: body.tag_name, name: body.name || body.tag_name, publishedAt: body.published_at, size: asset.size };
        setRel(live);
        try {
          sessionStorage.setItem(CACHE_KEY, JSON.stringify(live));
        } catch {
          /* no storage */
        }
      })
      .catch(() => {
        /* offline or rate limited: keep the build-time value */
      });
    return () => {
      cancelled = true;
    };
  }, []);
  return rel;
}
