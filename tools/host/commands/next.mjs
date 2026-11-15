// `tools\cli host next` (#1416): the next scheduled restart with its warning times, and the next announced
// session, both in local time. Read only. The logic is tools/host/next.mjs.
//
// The restart schedule is the profile's config.json (what the running server reads, written by `host config
// apply`), or server/flagship/config.json in the repo when the host has none yet. The session is
// website/content/sessions.json (LF_ANNOUNCE_SESSIONS moves it, as for `host announce`); only a posted
// session, one with an `announced` time, counts.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ownerConfigPath } from "../config-apply.mjs";
import { clock, hasDraft, localStamp, nextRestart, nextSession, span } from "../next.mjs";

export const help = "next   the next scheduled restart with its warning times and the next announced session, in local time";

const TAG = "host next:";

function readJson(file) {
  if (!file || !existsSync(file)) return { missing: true };
  try {
    return { value: JSON.parse(readFileSync(file, "utf8")) };
  } catch (e) {
    return { error: e.message };
  }
}

// ctx.now, ctx.configFile and ctx.sessionsFile inject the clock and the files (the selftest).
export async function main(args, ctx) {
  if (args.length) {
    console.error("usage: tools\\cli host next");
    return 1;
  }
  const now = ctx.now ?? new Date();
  let code = 0;

  let configFile = ctx.configFile;
  if (!configFile) {
    const profileCfg = ownerConfigPath(ctx.hostDir);
    configFile = existsSync(profileCfg) ? profileCfg : join(ctx.root, "server", "flagship", "config.json");
  }
  const cfg = readJson(configFile);
  if (cfg.error) {
    console.log(`${TAG} restarts: ${configFile} is not readable JSON (${cfg.error})`);
    code = 1;
  } else {
    const r = nextRestart(cfg.value && cfg.value.restarts, now);
    if (!r) console.log(`${TAG} restart: no scheduled restarts (restarts.times is empty or missing in ${configFile})`);
    else {
      console.log(`${TAG} restart: ${localStamp(r.at)} (in ${span(r.untilMs)})`);
      for (const w of r.warnings) console.log(`${TAG}   warning ${String(w.lead).padStart(3)} min before: ${clock(w.at)} ${w.state === "passed" ? "(passed)" : "(ahead)"}`);
    }
  }

  const sessionsFile = ctx.sessionsFile ?? process.env.LF_ANNOUNCE_SESSIONS ?? join(ctx.root, "website", "content", "sessions.json");
  const ses = readJson(sessionsFile);
  if (ses.error) {
    console.log(`${TAG} session: ${sessionsFile} is not readable JSON (${ses.error})`);
    code = 1;
  } else {
    const s = nextSession(ses.value, now);
    if (s) console.log(`${TAG} session: ${localStamp(s.at)} (in ${span(s.at.getTime() - now.getTime())}), build ${s.build || "unnamed"}${s.wipe ? ", WIPE" : ""}, announced ${localStamp(s.announced)}`);
    else if (hasDraft(ses.value)) console.log(`${TAG} session: no announced session (a draft is in ${sessionsFile}; post it with \`host post session --send\`)`);
    else console.log(`${TAG} session: no announced session`);
  }
  return code;
}
