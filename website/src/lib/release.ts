// The playtest download: one stable URL that always serves the newest release
// (tools/release/pack-playtest.mjs, .github/workflows/playtest-release.yml).
import { useEffect, useState } from "react";
import { REPO, githubData, type PlaytestRelease } from "@/lib/content";

export const PLAYTEST_ZIP = "LifeFramework-Playtest.zip";
export const PLAYTEST_DOWNLOAD_URL = `https://github.com/${REPO}/releases/latest/download/${PLAYTEST_ZIP}`;

const CACHE_KEY = "lf-playtest-release";

// Shown only when the build could not read GitHub and the browser has not either (offline, rate
// limited). Update it when you cut a new playtest; the live lookup below replaces it as soon as it
// answers, so an old value here never wins over the real latest release.
export const FALLBACK_RELEASE: PlaytestRelease = {
  tag: "playtest-m2-6",
  name: "Playtest M2, build 6",
  publishedAt: "2026-11-14T07:51:16Z",
  size: 424522916,
};

// "Playtest M2, build 6" -> "Build 6"; any other name is shown as it is.
export const buildLabel = (rel: PlaytestRelease) => {
  const m = /build\s+(\d+)/i.exec(rel.name) || /-(\d+)$/.exec(rel.tag);
  return m ? `Build ${m[1]}` : rel.name;
};

export const formatSize = (bytes: number) => `${Math.round(bytes / 1048576)} MB`;

// The release written at build time, then the live one from the GitHub API once the page runs,
// so a release published after the last site build still shows its own version and size.
export function usePlaytestRelease(): PlaytestRelease | null {
  const [rel, setRel] = useState<PlaytestRelease | null>(githubData.playtestRelease ?? FALLBACK_RELEASE);
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
