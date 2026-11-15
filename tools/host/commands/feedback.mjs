// tools\cli host feedback <export.txt> [--out <dir>]
// Reads a copied Discord feedback channel, drops thanks, merges the messages that report the same
// thing, and writes one issue draft per report (the repo's bug or feature template layout) to
// tmp/feedback/<date>/<n>.md. Prints the `gh issue create` commands; it never files an issue
// itself (tools/host/feedback.mjs holds the parsing and drafting).

import { existsSync, readFileSync } from "node:fs";
import { join, relative, resolve } from "node:path";
import { answerLine, draftFeedback, findSessionReport, ghCreateCommand, parseExport, writeDrafts } from "../feedback.mjs";

export const help =
  "feedback <export.txt> [--out <dir>]   draft one issue per report from a copied Discord feedback channel: drops thanks and duplicates, strips Discord ids, labels each draft bug, enhancement, question or playtest, writes tmp/feedback/<date>/<n>.md and prints the gh issue create commands (never runs them); a question prints 'answer in the channel' instead";

function today() {
  const d = new Date();
  const p = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}`;
}

export async function main(args, ctx) {
  const outAt = args.indexOf("--out");
  const outFlag = outAt >= 0 ? args[outAt + 1] : undefined;
  const positional = args.filter((a, i) => !a.startsWith("--") && i !== outAt + 1);
  if (!positional[0]) {
    console.log("host feedback: pass the export file (the text copied from the feedback channel)");
    return 1;
  }
  const file = resolve(positional[0]);
  if (!existsSync(file)) {
    console.log(`host feedback: no export at ${positional[0]}`);
    return 1;
  }
  const messages = parseExport(readFileSync(file, "utf8"));
  if (!messages.length) {
    console.log(`host feedback: no messages found in ${positional[0]} (expected "Name — Today at 8:02 PM" headers or "[time] Name: text" lines)`);
    return 1;
  }
  const date = today();
  const session = { date, report: findSessionReport(join(ctx.hostDir, "sessions"), date) };
  const { drafts, thanks, duplicates } = draftFeedback(messages, { session });
  const dir = outFlag ? resolve(outFlag) : join(ctx.root, "tmp", "feedback", date);
  const files = writeDrafts(drafts, dir);
  if (drafts.length) {
    console.log(`drafts in ${dir}; nothing is filed. Review each draft, then run from ${ctx.root}:`);
    drafts.forEach((d, i) => {
      const abs = join(dir, files[i]);
      const rel = relative(ctx.root, abs).replace(/\\/g, "/");
      const path = rel.startsWith("..") ? abs.replace(/\\/g, "/") : rel;
      console.log(d.kind === "question" ? answerLine(d, path) : ghCreateCommand(d, path));
    });
  } else {
    console.log("host feedback: nothing to draft (every message was thanks)");
  }
  const count = (k) => drafts.filter((d) => d.kind === k).length;
  console.log(
    `SUMMARY host-feedback ok messages=${messages.length} thanks=${thanks} duplicates=${duplicates} drafts=${drafts.length} questions=${count("question")} playtest=${count("playtest")} out=${dir}`,
  );
  return 0;
}
