// tools/release/playtest-server.mjs - point the playtest launcher's "Join the
// community server" at a host, or say none runs.
//
//   tools\cli playtest server <address[:port]> [--message "<text>"]
//   tools\cli playtest server off [--message "<text>"]
//
// The launcher (tools/release/launcher/play.ps1) reads
// https://life-framework.org/playtest-server.json, which is
// website/public/playtest-server.json on main. This writes that file on a
// fresh branch through the GitHub API (no checkout touched), opens a PR and
// turns on auto-merge; the website workflow deploys it a few minutes after the
// merge. `release` is the latest playtest release tag, so a player whose
// download is older is told to download again: host the same commit
// (cut a release first with `gh workflow run playtest-release.yml`).
import { spawnSync } from "node:child_process";

export const REPO = "Life-Framework/Life-Framework";
export const SERVER_FILE = "website/public/playtest-server.json";

export function serverJson({ address = null, release = null, message = null } = {}) {
  const out = { address: address || null, release: address ? release || null : null };
  if (message) out.message = message;
  return JSON.stringify(out, null, 2) + "\n";
}

// "1.2.3.4", "1.2.3.4:2001", "host.example.org:2001". No scheme, no spaces.
export function validAddress(a) {
  return /^[A-Za-z0-9.-]+(:\d{1,5})?$/.test(String(a)) && !/^[.-]|[.-]$/.test(String(a).split(":")[0]);
}

function gh(args, input) {
  const r = spawnSync("gh", args, { encoding: "utf8", input, shell: false });
  if (r.status !== 0) throw new Error(`gh ${args.slice(0, 3).join(" ")}: ${(r.stderr || r.stdout).trim()}`);
  return r.stdout;
}

export function latestPlaytestTag() {
  try {
    const rel = JSON.parse(gh(["api", `repos/${REPO}/releases/latest`]));
    return /^playtest-/.test(rel.tag_name) ? rel.tag_name : null;
  } catch {
    return null;
  }
}

export function publishServer({ address, message }) {
  const release = address ? latestPlaytestTag() : null;
  const body = serverJson({ address, release, message });
  const mainSha = JSON.parse(gh(["api", `repos/${REPO}/git/ref/heads/main`])).object.sha;
  const stamp = new Date().toISOString().replace(/[-:]/g, "").slice(0, 13);
  const branch = `playtest-server-${stamp}`;
  gh(["api", `repos/${REPO}/git/refs`, "-f", `ref=refs/heads/${branch}`, "-f", `sha=${mainSha}`]);
  let fileSha = null;
  try {
    fileSha = JSON.parse(gh(["api", `repos/${REPO}/contents/${SERVER_FILE}?ref=main`])).sha;
  } catch {}
  const title = address ? `Playtest server: ${address}` : "Playtest server: none running";
  const put = { message: title, content: Buffer.from(body).toString("base64"), branch, ...(fileSha ? { sha: fileSha } : {}) };
  gh(["api", "-X", "PUT", `repos/${REPO}/contents/${SERVER_FILE}`, "--input", "-"], JSON.stringify(put));
  const prBody = `Sets \`${SERVER_FILE}\`, which the playtest launcher reads for "Join the community server".\n\n\`\`\`json\n${body}\`\`\`\n\nWritten by \`tools\\cli playtest server\`.`;
  const url = gh(["pr", "create", "--repo", REPO, "--base", "main", "--head", branch, "--title", title, "--body", prBody]).trim();
  try {
    gh(["pr", "merge", url, "--auto", "--merge", "--delete-branch"]);
  } catch (e) {
    console.log(`WARN  auto-merge not set (${e.message}); merge ${url} by hand`);
  }
  return { url, body, release };
}

export function cmdPlaytestServer(args) {
  const target = args[0];
  const mi = args.indexOf("--message");
  const message = mi >= 0 ? args[mi + 1] : null;
  if (!target) {
    console.log('usage: tools\\cli playtest server <address[:port]>|off [--message "<text>"]');
    return 1;
  }
  const address = target === "off" || target === "none" ? null : target;
  if (address && !validAddress(address)) {
    console.log(`FAIL  "${address}" is not an address such as 203.0.113.7 or 203.0.113.7:2001`);
    return 1;
  }
  try {
    const r = publishServer({ address, message });
    console.log(`playtest server: ${address ?? "none"}${r.release ? ` (players need ${r.release})` : ""}`);
    console.log(`  PR ${r.url} (auto-merge); live at https://life-framework.org/playtest-server.json a few minutes after it merges`);
    if (address && !r.release) console.log("WARN  no playtest-* release is latest; players' downloads are not checked against the server");
    return 0;
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  }
}

export function selftestServer() {
  const failures = [];
  const off = JSON.parse(serverJson());
  if (off.address !== null || off.release !== null) failures.push("off file is not { address: null, release: null }");
  const on = JSON.parse(serverJson({ address: "203.0.113.7:2001", release: "playtest-m2-1", message: "Tonight 20:00 UTC" }));
  if (on.address !== "203.0.113.7:2001" || on.release !== "playtest-m2-1" || on.message !== "Tonight 20:00 UTC") failures.push("on file drops a field");
  if (JSON.parse(serverJson({ release: "x" })).release !== null) failures.push("an off file names a release");
  for (const ok of ["203.0.113.7", "203.0.113.7:2001", "play.example.org:2001"]) if (!validAddress(ok)) failures.push(`${ok} rejected`);
  for (const bad of ["", "http://x", "a b", "1.2.3.4:", "-x.org"]) if (validAddress(bad)) failures.push(`"${bad}" accepted`);
  return failures;
}
