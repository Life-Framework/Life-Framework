import { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";
import { DiscordIcon, DownloadIcon } from "@/components/icons";
import { DISCORD_URL } from "@/lib/content";
import { PLAYTEST_DOWNLOAD_URL } from "@/lib/release";
import sources from "../../content/outreach-sources.json";

// The call to action every outreach post lands on (#568): join the Discord, then download the playtest.
// It is a separate component so the homepage can drop it in without a copy of the wording; see
// docs/community/outreach/README.md ("Tagged links") for how a source tag reaches the invite. The
// buttons reuse the styles of PlaytestButtons, which has the same two actions without the source.

type Source = { id: string; label: string; invite: string | null };

// The Discord invite for a source tag: its own invite when one is set, so the invite's use count
// shows who joined from that community; otherwise the plain invite.
const inviteFor = (source: string | undefined): string => {
  const hit = (sources.sources as Source[]).find((s) => s.id === source);
  return hit?.invite || DISCORD_URL;
};

// Read after mount, so the prerendered HTML (which has no source) and the first client render match.
function useInvite(): string {
  const { source } = useParams();
  const [invite, setInvite] = useState(DISCORD_URL);
  useEffect(() => setInvite(inviteFor(source)), [source]);
  return invite;
}

export function JoinCta() {
  const invite = useInvite();
  return (
    <div>
      <div className="flex flex-col sm:flex-row flex-wrap gap-x-3 gap-y-4">
        <a href={invite} className="btn btn-big btn-discord">
          <DiscordIcon className="w-7 h-7 shrink-0" /> 1. Join our Discord
        </a>
        <a href={PLAYTEST_DOWNLOAD_URL} className="btn btn-big btn-download" download>
          <DownloadIcon className="w-7 h-7 shrink-0" /> 2. Download the playtest
        </a>
      </div>
      <p className="mt-4 text-[0.9375rem] muted max-w-prose">
        The Discord is where sessions are announced and where reports go: say you want to test in{" "}
        <span className="font-mono">#playtest-feedback</span>. The playtest is a zip for Windows with Arma Reforger from Steam; the{" "}
        <Link to="/playtest" className="link">
          playtest page
        </Link>{" "}
        has the steps.
      </p>
    </div>
  );
}
