// Proves a remote run with a huge argument (the bulk test shard's 566-class --only-classes list) still fits
// one ssh command: the argument travels as a file and the box reads it. Does not touch a machine.
// inputs: tools/remote/testbox.mjs, tools/validation/remote-bigargs-selftest.mjs, addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistrations.generated.c
// red-proof 2026-11-15 (#1590 #1579 #1484 #1703 shard 4 "remote script too long for one ssh command (8835 chars gzipped)"): with
// remoteArgPlan returning the args unchanged "the bulk shard's command line does not fit one ssh command".
import { createHash } from "node:crypto";
import { fileURLToPath } from "node:url";
import { join } from "node:path";
import * as tb from "../remote/testbox.mjs";
import * as shards from "../lib/test-shards.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const root = join(fileURLToPath(new URL(".", import.meta.url)), "..", "..");
const rDir = "C:/LF/Life-Framework-lane-server-test-4";
const classes = shards.readRegistry(root).map((t) => t.cls);
check(classes.length > 100, `the registry parses (${classes.length} tests)`);

// The shard shape: three heavy tests alone, every other test in the last slice.
const bulk = classes.slice(3);
const args = ["test", "--no-build", "--local", "--no-cache", "--tier", "all", "--only-classes", bulk.join(","), "--core-only", "--budget", "336"];
const line = (a) => `& node ${tb.psQuote("tools\\cli.mjs")} ${a.map(tb.psArg).join(" ")}`;
// What psCheckout and the log lookup add around the node call, rounded up (the real one is about 3 KB).
let filler = "";
for (let i = 0; filler.length < 3000; i++) filler += createHash("sha256").update(String(i)).digest("base64");

const plan = tb.remoteArgPlan(rDir, args);
check(plan.args.length === args.length, "one planned argument per argument");
check(plan.args.join(" ").length < 1500, `the planned command line is short (${plan.args.join(" ").length} chars)`);
check(plan.uploads.length === 1, `exactly the long argument is uploaded (${plan.uploads.length})`);
const up = plan.uploads[0];
check(up && up.content === bulk.join(","), "the uploaded file holds the whole class list, unchanged");
check(up && up.to.startsWith(`${rDir}/tmp/remote-in/`), `the file lands in the lane's tmp/remote-in (${up?.to})`);
check(plan.args.slice(0, 7).join(" ") === args.slice(0, 7).join(" "), "short arguments pass through untouched");
check(tb.remoteArgPlan(rDir, args).uploads[0]?.to === up?.to, "the same list gets the same file (a retry reuses it)");
check(tb.remoteArgPlan(rDir, [...args.slice(0, 7), `${bulk.join(",")},LF_Test_X`]).uploads[0]?.to !== up?.to, "another list gets another file");
const small = tb.remoteArgPlan(rDir, ["test", "--only", "LF_Test_A"]);
check(small.uploads.length === 0 && small.args.join(" ") === "test --only LF_Test_A", "a short command line is not rewritten");

// The real constraint: the whole script, as ssh sends it.
try {
  tb.sshPsArgs("h", `${filler}\n${line(plan.args)}`);
} catch (e) {
  failures.push(`the bulk shard's command line does not fit one ssh command: ${e.message}`);
}

if (failures.length) {
  console.log(`remote-bigargs-selftest: FAIL\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log(`remote-bigargs-selftest: OK (${bulk.length}-class list travels as a file, short arguments untouched, the ssh command fits)`);
