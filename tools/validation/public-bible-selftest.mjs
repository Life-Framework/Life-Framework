// tools/validation/public-bible-selftest.mjs
// inputs: tools/validation/**, website/scripts/live-now.mjs, docs/everon/**
//
// Proves validate-public-bible over tools/validation/fixtures/public-bible/: `clean` passes; the other cases are
// laid over a copy of it and each must fail with its own message: a page linking the internal file, a picture
// caption with a listed spoiler term, a public page with one, and an internal file with no term list. Also proves
// the "Live now" section: only a state in the header's list is shown, an empty list shows nothing (D4), and
// the section is placed before the page's contents list.
// red-proof: with checkPublicBible stubbed to return [] this printed "FAIL links-internal: expected a problem
// matching /names world-bible-internal/, got none" and four more, and exited 1 (2026-11-17).

import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { checkPublicBible, INTERNAL } from "./validate-public-bible.mjs";
import { collectLivePlaces, liveNowMarkdown, withLiveNow } from "../../website/scripts/live-now.mjs";

const here = path.dirname(fileURLToPath(import.meta.url));
const FIX = path.join(here, "fixtures", "public-bible");
let failed = 0;
const fail = (name, msg) => {
  failed++;
  console.log(`FAIL ${name}: ${msg}`);
};

function overlay(name, mutate) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "lf-public-bible-"));
  fs.cpSync(path.join(FIX, "clean"), dir, { recursive: true });
  if (name) fs.cpSync(path.join(FIX, name), dir, { recursive: true });
  if (mutate) mutate(dir);
  return dir;
}

function expect(name, dir, pattern) {
  const problems = checkPublicBible(dir);
  if (!pattern) {
    if (problems.length) fail(name, `expected a clean pass, got ${JSON.stringify(problems)}`);
  } else if (!problems.some((p) => pattern.test(p))) {
    fail(name, `expected a problem matching ${pattern}, got ${problems.length ? JSON.stringify(problems) : "none"}`);
  }
  fs.rmSync(dir, { recursive: true, force: true });
}

expect("clean", overlay(null), null);
expect("links-internal", overlay("links-internal"), /names world-bible-internal/);
expect("caption-spoiler", overlay("caption-spoiler"), /picture 1 caption: uses the spoiler term "sealed drums"/);
expect("page-spoiler", overlay("page-spoiler"), /world-bible\.md: uses the spoiler term "second copy"/);
expect(
  "no-term-list",
  overlay(null, (d) => fs.writeFileSync(path.join(d, INTERNAL), "# internal\n\nNo list here.\n")),
  /no `## Spoiler terms` list/,
);
expect(
  "linked-from-content",
  overlay(null, (d) => fs.writeFileSync(path.join(d, "website/content/site.json"), '{"see":"docs/everon/world-bible-internal.md"}')),
  /website\/content\/site\.json: names world-bible-internal/,
);

// Live now
{
  const dir = overlay(null);
  const places = collectLivePlaces(dir);
  if (places.map((p) => p.slug).join() !== "harbour") fail("live-now", `state 0 only should show harbour, got ${places.map((p) => p.slug)}`);
  const md = liveNowMarkdown(dir);
  if (!/^## Live now/m.test(md) || !/### Harbour/.test(md) || !/12 buildings, 9 you can walk into\./.test(md)) fail("live-now", `unexpected section:\n${md}`);
  if (/Inland/.test(md)) fail("live-now", "a place whose state is not live was shown");
  if (/the back room/.test(md)) fail("live-now", "a non-overview picture was used");
  const page = withLiveNow("Intro\n\n## Contents\n\n1. x\n", md);
  if (!(page.indexOf("## Live now") > 0 && page.indexOf("## Live now") < page.indexOf("## Contents"))) fail("live-now", "section is not before the contents list");
  const pub = path.join(dir, "pub");
  const withPics = liveNowMarkdown(dir, { publicDir: pub });
  if (!fs.existsSync(path.join(pub, "harbour.jpg")) || !withPics.includes("/everon-live/harbour.jpg")) fail("live-now", "picture was not copied and linked");
  fs.rmSync(dir, { recursive: true, force: true });
}
{
  const dir = overlay(null, (d) => fs.writeFileSync(path.join(d, "addons/LF_Everon1989/Missions/LF_Everon1989.conf"), 'LF_MissionHeader {\n m_sName "x"\n}\n'));
  if (liveNowMarkdown(dir) !== "") fail("live-now-empty", "a header with no live states must show nothing");
  fs.rmSync(dir, { recursive: true, force: true });
}

if (failed) {
  console.log(`public-bible-selftest: ${failed} failed`);
  process.exit(1);
}
console.log("OK public-bible-selftest: clean passes; internal link, spoiler caption, spoiler page and no term list fail; Live now follows the header");
