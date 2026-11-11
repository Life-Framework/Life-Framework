import { DiscordIcon, DownloadIcon } from "@/components/icons";
import { DISCORD_URL } from "@/lib/content";
import { PLAYTEST_DOWNLOAD_URL, formatSize, usePlaytestRelease } from "@/lib/release";

// The two playtest calls to action, on the homepage hero and at the top of /playtest.
// Plain words only: a player who has never opened a terminal reads this first.
export function PlaytestButtons({ className = "" }: { className?: string }) {
  const rel = usePlaytestRelease();
  return (
    <div className={className}>
      <div className="flex flex-col sm:flex-row flex-wrap gap-x-3 gap-y-4">
        <a href={PLAYTEST_DOWNLOAD_URL} className="btn btn-big btn-download" download>
          <DownloadIcon className="w-7 h-7 shrink-0" /> Download the playtest
        </a>
        <a href={DISCORD_URL} className="btn btn-big btn-discord">
          <DiscordIcon className="w-7 h-7 shrink-0" /> Join our Discord
        </a>
      </div>
      <p className="mt-4 text-[0.9375rem] muted">
        For Windows, with Arma Reforger from Steam.{" "}
        {rel ? (
          <>
            {rel.name} · {formatSize(rel.size)} zip.
          </>
        ) : (
          "Free zip download."
        )}{" "}
        Unzip it and double-click <strong className="text-ink">Play Life Framework</strong>.
      </p>
    </div>
  );
}
