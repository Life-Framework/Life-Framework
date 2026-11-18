// Every pass-2 e2e check for newer save data asks LF_E2E first (#2812).
// inputs: addons/**/Scripts/Game/Tests/*.c, addons/LifeFramework/Scripts/Game/Persistence/Serializers/LF_PersistenceComponentSerializer.c, tools/lib/e2e-payload-guard.mjs
//
// The main guard's pre-move e2e writes pass 1 with the pinned ref build and reloads it on the merged tree; a
// pass-2 check for a payload the ref never wrote fails and the guard reverted correct PRs (#2480, #2798). The
// rules (tools/lib/e2e-payload-guard.mjs): no hand-rolled BootSaveVersion() vs PayloadSinceVersion() compare,
// LF_E2E guards name a registered payload, and every Owner.VerifyE2E( call is guarded by LF_E2E.Requires*.
//
// LF_E2E_GUARD_ROOT points the scan at another tree (the selftest uses a temp one).
//
// red-proof: 2026-11-18, tools/validation/e2e-payload-guard-selftest.mjs runs this over a tree holding
// tools/validation/fixtures/e2e-payload-guard/LF_Test_E2E_Bad.c; it fails with `FAIL ...:N ... not guarded` and
// passes once the file is gone.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { guardFindings, registeredPayloads } from "../lib/e2e-payload-guard.mjs";

const root = process.env.LF_E2E_GUARD_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const files = [];
const addons = join(root, "addons");
for (const addon of existsSync(addons) ? readdirSync(addons) : []) {
  const dir = join(addons, addon, "Scripts", "Game", "Tests");
  if (!existsSync(dir)) continue;
  for (const f of readdirSync(dir)) if (f.endsWith(".c")) files.push({ path: `addons/${addon}/Scripts/Game/Tests/${f}`, text: readFileSync(join(dir, f), "utf8") });
}
const serializer = join(addons, "LifeFramework", "Scripts", "Game", "Persistence", "Serializers", "LF_PersistenceComponentSerializer.c");
const payloads = existsSync(serializer) ? registeredPayloads(readFileSync(serializer, "utf8")) : [];

const bad = guardFindings(files, payloads).map((m) => `FAIL ${m}`);
if (bad.length) {
  console.log(bad.join("\n"));
  console.log(`validate-e2e-payload-guard: ${bad.length} finding(s). A pass-2 check for newer data uses LF_E2E.RequiresPayload / SaveHas (Tests/LF_E2E.c, #2812).`);
  process.exit(1);
}
console.log(`validate-e2e-payload-guard: OK (${files.length} test file(s), ${payloads.length} payload(s))`);
