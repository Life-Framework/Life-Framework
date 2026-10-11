// Suite stamp and suite-valid path rules.
//
// A fast run and a full run share server/profile/test. test-e2e.ps1 records the
// summary that is about to be deleted. suite-valid decides whether a later
// commit still rides on that summary.

export function isSuiteSafePath(file) {
  const path = String(file).replace(/\\/g, "/").replace(/^\.\//, "");
  if (path === "docs/features.md") return true;
  if (path.endsWith("/EL_TestRegistrations.generated.c") || path === "EL_TestRegistrations.generated.c") return true;
  const parts = path.split("/");
  if (parts.includes("Language")) return true;
  if (/everonlife_localization\.[A-Za-z0-9_]+\.conf$/.test(path)) return true;
  return false;
}

// 0 when every path is localization, the generated test registry, or docs/features.md.
// 1 when any other path changed, including a gameplay script, a persistence
// serializer, a prefab, or a world layer.
export function suiteValidCode(paths) {
  for (const path of paths) {
    if (!isSuiteSafePath(path)) return 1;
  }
  return 0;
}

export function newestSummaryLine(jsonl, tier) {
  const lines = String(jsonl).replace(/^\uFEFF/, "").split(/\n/);
  let newest = null;
  for (const raw of lines) {
    const line = raw.trim();
    if (!line) continue;
    let row;
    try {
      row = JSON.parse(line);
    } catch {
      continue;
    }
    if (row && row.tier === tier) newest = line;
  }
  return newest;
}

export function selftestSuiteStamp() {
  const failures = [];
  const safe = [
    "addons/LifeFramework/Language/everonlife_localization.st",
    "addons/LifeFramework/Language/everonlife_localization.en_us.conf",
    "addons/LifeFramework/Scripts/Game/Tests/EL_TestRegistrations.generated.c",
    "docs/features.md",
  ];
  if (suiteValidCode(safe) !== 0) failures.push("safe fixture names should exit 0");
  if (suiteValidCode([]) !== 0) failures.push("no changed paths should exit 0");

  const unsafe = [
    ["addons/LifeFramework/Scripts/Game/Feature/Money/EL_MoneyUtils.c", "gameplay script"],
    ["addons/LifeFramework/Scripts/Game/Persistence/Serializers/EL_PersistenceComponentSerializer.c", "persistence serializer"],
    ["addons/LifeFramework/Prefabs/Items/Currencies/MoneyStack.et", "prefab"],
    ["addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Roadmap_Breach.layer", "world layer"],
  ];
  for (const [path, label] of unsafe) {
    if (suiteValidCode([path]) !== 1) failures.push(`${label} fixture should exit 1`);
    if (suiteValidCode([...safe, path]) !== 1) failures.push(`${label} mixed with safe paths should exit 1`);
  }

  const jsonl = [
    JSON.stringify({ sha: "aaa", tier: "fast", passed: 1, failed: 0, total: 1, consoleLog: "old.log" }),
    JSON.stringify({ sha: "bbb", tier: "all", passed: 2, failed: 0, total: 2, consoleLog: "all.log" }),
    JSON.stringify({ sha: "ccc", tier: "fast", passed: 3, failed: 1, total: 4, consoleLog: "new.log" }),
  ].join("\n");
  const newest = newestSummaryLine(jsonl, "fast");
  if (!newest || !newest.includes('"sha":"ccc"') || !newest.includes("new.log")) {
    failures.push("suite-stamp should print the newest line for the tier");
  }
  if (newestSummaryLine(jsonl, "missing")) failures.push("missing tier should have no line");
  return failures;
}
