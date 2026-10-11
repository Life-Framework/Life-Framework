// DebugWorld street manifest. Reads site name and world x y z, checks the
// terrain box, and writes a layer only to the path it is given.

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

export const TERRAIN_MIN = 0;
export const TERRAIN_MAX = 512;
export const STREET_Z = 120;
export const STREET_GAP_M = 30;
export const PAD_OVERLAP_M = 0.05;

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const debugWorld = path.join(repoRoot, "addons", "LifeFramework", "Worlds", "DebugWorld");
const scriptPath = fileURLToPath(import.meta.url);

const NAME = /^[A-Za-z_][A-Za-z0-9_]*$/;
const NUMBER = /^[-+]?(?:\d+\.?\d*|\.\d+)$/;

export function formatCoord(value) {
  const rounded = Math.round(value * 1000) / 1000;
  if (rounded === 0) return "0";
  if (Number.isInteger(rounded)) return String(rounded);
  return String(rounded);
}

export function parseManifest(text) {
  const sites = [];
  const errors = [];
  const lines = String(text).split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].trim();
    if (!line || line.startsWith("#")) continue;
    const parts = line.split(/\s+/);
    const coords = parts.slice(1);
    if (parts.length !== 4 || !NAME.test(parts[0]) || coords.some((part) => !NUMBER.test(part))) {
      errors.push(`line ${i + 1} is not a site (name x y z)`);
      continue;
    }
    sites.push({
      name: parts[0],
      x: Number(coords[0]),
      y: Number(coords[1]),
      z: Number(coords[2]),
      line: i + 1,
    });
  }
  return { sites, errors };
}

function horizontal(a, b) {
  return Math.hypot(a.x - b.x, a.z - b.z);
}

export function checkSites(sites) {
  const failures = [];
  for (const site of sites) {
    if (site.x < TERRAIN_MIN || site.x > TERRAIN_MAX || site.z < TERRAIN_MIN || site.z > TERRAIN_MAX) {
      failures.push(`${site.name} is off the terrain at ${formatCoord(site.x)} ${formatCoord(site.y)} ${formatCoord(site.z)}`);
    }
    if (site.y !== 0) failures.push(`${site.name} is raised at Y=${formatCoord(site.y)}`);
  }
  for (let i = 0; i < sites.length; i++) {
    for (let j = i + 1; j < sites.length; j++) {
      if (horizontal(sites[i], sites[j]) < PAD_OVERLAP_M) {
        failures.push(`${sites[i].name} is on top of another site (${sites[j].name})`);
      }
    }
  }
  if (sites.length >= 2) {
    for (let i = 0; i < sites.length; i++) {
      let nearest = Infinity;
      for (let j = 0; j < sites.length; j++) {
        if (i === j) continue;
        const distance = horizontal(sites[i], sites[j]);
        if (distance < nearest) nearest = distance;
      }
      if (nearest > STREET_GAP_M) {
        failures.push(`${sites[i].name} is more than 30 m from the nearest other site (${formatCoord(nearest)} m)`);
      }
    }
  }
  return { ok: failures.length === 0, failures };
}

export function checkManifest(text) {
  const parsed = parseManifest(text);
  const checked = checkSites(parsed.sites);
  const failures = [...parsed.errors, ...checked.failures];
  if (!parsed.errors.length && parsed.sites.length === 0) failures.push("manifest has no sites");
  return { ok: failures.length === 0, failures, sites: parsed.sites };
}

export function renderLayer(sites) {
  if (!sites.length) return "";
  const blocks = sites.map((site) => {
    return `GenericEntity ${site.name} {\r\n coords ${formatCoord(site.x)} ${formatCoord(site.y)} ${formatCoord(site.z)}\r\n}`;
  });
  return `${blocks.join("\r\n")}\r\n`;
}

export function isInsideDebugWorld(target) {
  const resolved = path.resolve(target);
  const relative = path.relative(debugWorld, resolved);
  if (relative === "") return true;
  if (path.isAbsolute(relative)) return false;
  return relative !== ".." && !relative.startsWith(`..${path.sep}`);
}

export function writeLayer(sites, outPath) {
  if (isInsideDebugWorld(outPath)) {
    throw new Error(`refusing to write inside DebugWorld: ${outPath}`);
  }
  const resolved = path.resolve(outPath);
  writeFileSync(resolved, renderLayer(sites));
  return resolved;
}

function printResult(prefix, result) {
  if (result.ok) {
    console.log(`street-manifest: OK (${result.sites.length} sites)`);
    return 0;
  }
  console.log(`street-manifest: FAIL ${prefix}`.trim());
  for (const failure of result.failures) console.log(`- ${failure}`);
  return 1;
}

function readManifest(file) {
  return readFileSync(file, "utf8");
}

function commandCheck(file) {
  if (!file) return usage();
  const result = checkManifest(readManifest(file));
  return printResult("", result);
}

function commandGenerate(file, outPath) {
  if (!file || !outPath) return usage();
  const result = checkManifest(readManifest(file));
  if (!result.ok) return printResult("(not written)", result);
  try {
    const written = writeLayer(result.sites, outPath);
    console.log(`street-manifest: wrote ${written}`);
    return 0;
  } catch (error) {
    console.log(`street-manifest: FAIL`);
    console.log(`- ${error.message}`);
    return 1;
  }
}

function usage() {
  console.log("usage: node tools/street/manifest.mjs check <manifest>");
  console.log("       node tools/street/manifest.mjs generate <manifest> <out.layer>");
  console.log("       node tools/street/manifest.mjs selftest");
  return 1;
}

function runCli(args) {
  return spawnSync(process.execPath, [scriptPath, ...args], { encoding: "utf8", cwd: repoRoot });
}

function assertCli(args, expectOk, needle, label) {
  const result = runCli(args);
  const output = `${result.stdout ?? ""}\n${result.stderr ?? ""}`;
  const ok = result.status === 0;
  if (ok !== expectOk) {
    throw new Error(`${label}: expected exit ${expectOk ? 0 : 1}, got ${result.status}\n${output}`);
  }
  if (needle && !output.includes(needle)) throw new Error(`${label}: missing ${JSON.stringify(needle)}\n${output}`);
  return output;
}

function selftest() {
  const dir = mkdtempSync(path.join(tmpdir(), "street-manifest-"));
  const forbidden = path.join(debugWorld, "DebugWorld_Layers", "StreetManifest_Selftest_DoNotWrite.layer");
  try {
    const past = path.join(dir, "past-x.txt");
    const overlap = path.join(dir, "overlap.txt");
    const row = path.join(dir, "row.txt");
    const layer = path.join(dir, "row.layer");
    writeFileSync(past, "PastA 500 0 120\nPastB 520 0 120\n");
    writeFileSync(overlap, "PadA 240 0 120\nPadB 240 0 120\n");
    writeFileSync(row, `# release street Z=${STREET_Z}\nStreetA 210 0 ${STREET_Z}\n\nStreetB 228 0 ${STREET_Z}\n`);

    assertCli(["check", past], false, "off the terrain", "row past x=512");
    assertCli(["check", overlap], false, "on top of another site", "overlapping pair");
    assertCli(["check", row], true, "street-manifest: OK", "short row");
    assertCli(["generate", row, layer], true, "wrote", "generate short row");

    const written = readFileSync(layer, "utf8");
    if (!written.includes("GenericEntity StreetA {\r\n coords 210 0 120\r\n}")) {
      throw new Error(`generate wrote an unexpected layer:\n${written}`);
    }
    if (!written.includes("GenericEntity StreetB {\r\n coords 228 0 120\r\n}")) {
      throw new Error(`generate omitted StreetB:\n${written}`);
    }
    if (isInsideDebugWorld(layer)) throw new Error("temp layer resolved inside DebugWorld");

    const raised = path.join(dir, "raised.txt");
    const gap = path.join(dir, "gap.txt");
    const edge = path.join(dir, "edge.txt");
    writeFileSync(raised, "HighA 210 1 120\nHighB 228 1 120\n");
    writeFileSync(gap, "FarA 100 0 120\nFarB 180 0 120\n");
    writeFileSync(edge, "EdgeA 494 0 512\nEdgeB 512 0 512\n");
    assertCli(["check", raised], false, "raised", "raised row");
    assertCli(["check", gap], false, "more than 30 m", "gap row");
    assertCli(["check", edge], true, "street-manifest: OK", "inclusive terrain edge");

    assertCli(["generate", row, forbidden], false, "refusing to write inside DebugWorld", "generate DebugWorld path");
    if (existsSync(forbidden)) {
      rmSync(forbidden, { force: true });
      throw new Error("generate wrote inside DebugWorld");
    }
    if (existsSync(path.join(dir, "bad.layer"))) throw new Error("unexpected layer in temp dir");

    const blocked = path.join(dir, "blocked.layer");
    assertCli(["generate", past, blocked], false, "off the terrain", "generate does not write a failing row");
    if (existsSync(blocked)) throw new Error("generate wrote a layer for a failing manifest");
  } finally {
    if (existsSync(forbidden)) rmSync(forbidden, { force: true });
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("street-manifest selftest: OK");
  return 0;
}

function main() {
  const [command, ...args] = process.argv.slice(2);
  try {
    if (command === "check") return commandCheck(args[0]);
    if (command === "generate") return commandGenerate(args[0], args[1]);
    if (command === "selftest") return selftest();
    return usage();
  } catch (error) {
    console.log("street-manifest: FAIL");
    console.log(`- ${error.message}`);
    return 1;
  }
}

function isDirectRun() {
  if (!process.argv[1]) return false;
  return pathToFileURL(path.resolve(process.argv[1])).href.toLowerCase() === import.meta.url.toLowerCase();
}

if (isDirectRun()) {
  process.exitCode = main();
}
