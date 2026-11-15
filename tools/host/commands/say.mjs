// `tools\cli host say "<text>"` (#1453). The logic is tools/host/say.mjs.
import { MAX_LENGTH, sayHost } from "../say.mjs";

export const help = "say \"<text>\"   put an operator message on every player's screen (the server reads it within 10 s; max 200 characters)";

export async function main(args, ctx) {
  const r = sayHost(ctx.hostDir, args.join(" "));
  if (!r.ok) {
    console.error(`[LFDebug:HostSay] refused: ${r.reason}`);
    return 1;
  }
  if (r.cut) console.log(`[LFDebug:HostSay] the message was cut to ${MAX_LENGTH} characters`);
  if (r.replaced) console.log("[LFDebug:HostSay] replaced an earlier message the server had not read yet");
  console.log(`[LFDebug:HostSay] queued: ${r.text}`);
  console.log(`[LFDebug:HostSay] the server toasts it within 10 s: ${r.file}`);
  return 0;
}
