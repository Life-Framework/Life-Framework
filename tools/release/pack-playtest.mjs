// tools/release/pack-playtest.mjs - build LifeFramework-Playtest.zip, the
// download a player unzips and double-clicks (docs/playtest.md "Download").
//
//   node tools/release/pack-playtest.mjs --rdb <file> [--out <zip>] [--version <name>]
//   node tools/release/pack-playtest.mjs --selftest
//
// The zip root holds `Play Life Framework.cmd`, `README.txt`, `version.txt`,
// `launcher/` and `addons/LifeFramework/`. The addon is every file git tracks
// under addons/LifeFramework, so nothing generated (rdb, log, .gproj.user,
// injected Workbench scripts) gets in by accident; EXCLUDE repeats golden
// rule 1 in case one is ever tracked. resourceDatabase.rdb is the exception:
// it is gitignored, but the game resolves {GUID}paths through it and a player
// has no Workbench to make one, so --rdb adds a copy, and pack refuses one that
// misses any resource the addon declares. Only Workbench writes the rdb, so a
// release starts on a PC with the tools: `tools\cli playtest release`
// (tools/release/release-playtest.mjs) refreshes it and attaches it to a
// pre-release; .github/workflows/playtest-release.yml then packs the zip from a
// clean checkout of the tag and publishes it.
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { crc32, deflateRawSync, inflateRawSync } from "node:zlib";
import { addonResources, describeMissing, missingFrom } from "../lib/rdb-fresh.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..");
export const ZIP_NAME = "LifeFramework-Playtest.zip";
export const ADDON = "addons/LifeFramework";
export const RDB_PATH = `${ADDON}/resourceDatabase.rdb`;
export const EXCLUDE = [/\.rdb$/i, /\.rdb\.lock$/i, /^addons\/LifeFramework\/log\//i, /\.gproj\.user$/i, /\/Scripts\/WorkbenchGame\//i, /(^|\/)\.git/];

export function isExcluded(path) {
  return EXCLUDE.some((re) => re.test(path));
}

// ------------------------------------------------------------------ zip writer
// Plain PKZIP (deflate, UTF-8 names, no zip64: the zip is about 215 MB, under
// the 4 GB limit). The timestamp is passed in, so the same inputs give the same bytes.
function dosTime(d) {
  const time = (d.getUTCHours() << 11) | (d.getUTCMinutes() << 5) | (d.getUTCSeconds() >> 1);
  const date = ((d.getUTCFullYear() - 1980) << 9) | ((d.getUTCMonth() + 1) << 5) | d.getUTCDate();
  return { time, date };
}

export function makeZip(entries, when = new Date(Date.UTC(2026, 0, 1))) {
  const { time, date } = dosTime(when);
  const locals = [];
  const centrals = [];
  let offset = 0;
  for (const e of entries) {
    const name = Buffer.from(e.name, "utf8");
    const data = Buffer.isBuffer(e.data) ? e.data : Buffer.from(e.data);
    const deflated = deflateRawSync(data, { level: 9 });
    const stored = deflated.length >= data.length;
    const body = stored ? data : deflated;
    const crc = crc32(data) >>> 0;
    const head = Buffer.alloc(30);
    head.writeUInt32LE(0x04034b50, 0);
    head.writeUInt16LE(20, 4);
    head.writeUInt16LE(0x0800, 6); // UTF-8 names
    head.writeUInt16LE(stored ? 0 : 8, 8);
    head.writeUInt16LE(time, 10);
    head.writeUInt16LE(date, 12);
    head.writeUInt32LE(crc, 14);
    head.writeUInt32LE(body.length, 18);
    head.writeUInt32LE(data.length, 22);
    head.writeUInt16LE(name.length, 26);
    head.writeUInt16LE(0, 28);
    locals.push(head, name, body);
    const cen = Buffer.alloc(46);
    cen.writeUInt32LE(0x02014b50, 0);
    cen.writeUInt16LE(20, 4);
    cen.writeUInt16LE(20, 6);
    cen.writeUInt16LE(0x0800, 8);
    cen.writeUInt16LE(stored ? 0 : 8, 10);
    cen.writeUInt16LE(time, 12);
    cen.writeUInt16LE(date, 14);
    cen.writeUInt32LE(crc, 16);
    cen.writeUInt32LE(body.length, 20);
    cen.writeUInt32LE(data.length, 24);
    cen.writeUInt16LE(name.length, 28);
    cen.writeUInt32LE(offset, 42);
    centrals.push(cen, name);
    offset += head.length + name.length + body.length;
  }
  const cdSize = centrals.reduce((n, b) => n + b.length, 0);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(cdSize, 12);
  end.writeUInt32LE(offset, 16);
  return Buffer.concat([...locals, ...centrals, end]);
}

// Read back what makeZip wrote (selftest, and `--list`).
export function readZip(buf) {
  const eocd = buf.lastIndexOf(Buffer.from([0x50, 0x4b, 0x05, 0x06]));
  const count = buf.readUInt16LE(eocd + 10);
  let p = buf.readUInt32LE(eocd + 16);
  const out = [];
  for (let i = 0; i < count; i++) {
    const method = buf.readUInt16LE(p + 10);
    const csize = buf.readUInt32LE(p + 20);
    const nlen = buf.readUInt16LE(p + 28);
    const local = buf.readUInt32LE(p + 42);
    const name = buf.toString("utf8", p + 46, p + 46 + nlen);
    const lnlen = buf.readUInt16LE(local + 26);
    const lxlen = buf.readUInt16LE(local + 28);
    const raw = buf.subarray(local + 30 + lnlen + lxlen, local + 30 + lnlen + lxlen + csize);
    out.push({ name, data: method === 8 ? inflateRawSync(raw) : Buffer.from(raw) });
    p += 46 + nlen + buf.readUInt16LE(p + 30) + buf.readUInt16LE(p + 32);
  }
  return out;
}

// ------------------------------------------------------------------ pack
function git(args) {
  return execFileSync("git", args, { cwd: ROOT, encoding: "utf8", maxBuffer: 64 * 1024 * 1024 });
}

export function addonFiles() {
  return git(["ls-files", "-z", "--", ADDON])
    .split("\0")
    .filter(Boolean)
    .filter((p) => !isExcluded(p))
    .filter((p) => existsSync(join(ROOT, p)));
}

// The launcher files, as zip entries. The .cmd keeps CRLF (cmd.exe needs it,
// .gitattributes keeps .cmd CRLF); the .ps1 and README get CRLF too, for Notepad.
export function launcherEntries() {
  const crlf = (s) => s.replace(/\r?\n/g, "\r\n");
  const read = (f) => readFileSync(join(HERE, "launcher", f), "utf8");
  return [
    { name: "Play Life Framework.cmd", data: crlf(read("Play Life Framework.cmd")) },
    // PowerShell 5.1 reads a BOM-less script as ANSI; the BOM keeps it UTF-8.
    { name: "launcher/play.ps1", data: Buffer.concat([Buffer.from([0xef, 0xbb, 0xbf]), Buffer.from(crlf(read("play.ps1")))]) },
    { name: "README.txt", data: crlf(read("README.txt")) },
  ];
}

// The resources the addon declares that `rdb` does not register. Without the
// rdb at all, or with one of these missing, the game logs `Wrong GUID/name` for
// them and the playtest world does not load (seen 2026-11-10 on a server boot).
export function rdbMissing(rdb) {
  return missingFrom(readFileSync(rdb), addonResources(join(ROOT, ADDON)));
}

export function pack({ out, version, rdb, allowNoRdb = false }) {
  if (!rdb && !allowNoRdb) throw new Error("no --rdb: the game cannot load the addon without resourceDatabase.rdb");
  if (rdb) {
    const missing = rdbMissing(rdb);
    if (missing.length) throw new Error(`${rdb} is stale: ${missing.length} resource(s) unregistered (${describeMissing(missing)}); refresh it with tools\\cli rdb in a worktree`);
  }
  const commit = git(["rev-parse", "HEAD"]).trim();
  const entries = [...launcherEntries(), { name: "version.txt", data: `${version}\r\n${commit}\r\n` }];
  for (const p of addonFiles()) entries.push({ name: p, data: readFileSync(join(ROOT, p)) });
  if (rdb) entries.push({ name: RDB_PATH, data: readFileSync(rdb) });
  // File dates are the commit's, so a re-pack of the same commit is byte-identical.
  const when = new Date(git(["log", "-1", "--format=%cI", "HEAD"]).trim());
  const zip = makeZip(entries, when);
  mkdirSync(dirname(out), { recursive: true });
  writeFileSync(out, zip);
  return { out, bytes: zip.length, files: entries.length, commit, rdb: !!rdb };
}

// ------------------------------------------------------------------ selftest
export function selftestPack() {
  const failures = [];
  for (const bad of ["addons/LifeFramework/resourceDatabase.rdb", "addons/LifeFramework/resourceDatabase.rdb.lock", "addons/LifeFramework/log/x.log", "addons/LifeFramework/LifeFramework.gproj.user", "addons/LifeFramework/Scripts/WorkbenchGame/a.c"]) {
    if (!isExcluded(bad)) failures.push(`${bad} is not excluded`);
  }
  for (const good of ["addons/LifeFramework/LifeFramework.gproj", "addons/LifeFramework/Scripts/Game/Core/LF_Debug.c", "addons/LifeFramework/Prefabs/EL_JailCell.et"]) {
    if (isExcluded(good)) failures.push(`${good} is excluded`);
  }
  const entries = [
    { name: "Play Life Framework.cmd", data: "@echo off\r\n" },
    { name: "addons/LifeFramework/a.txt", data: "x".repeat(5000) },
    { name: "empty.txt", data: "" },
  ];
  const back = readZip(makeZip(entries));
  if (back.length !== entries.length) failures.push(`zip round trip has ${back.length} entries, not ${entries.length}`);
  for (const e of entries) {
    const b = back.find((x) => x.name === e.name);
    if (!b || b.data.toString() !== e.data) failures.push(`zip round trip changed ${e.name}`);
  }
  if (!makeZip(entries).equals(makeZip(entries))) failures.push("zip is not deterministic");
  const files = addonFiles();
  if (!files.includes(`${ADDON}/LifeFramework.gproj`)) failures.push("addon file list misses LifeFramework.gproj");
  if (files.some(isExcluded)) failures.push("addon file list holds an excluded file");
  const names = launcherEntries().map((e) => e.name);
  for (const n of ["Play Life Framework.cmd", "launcher/play.ps1", "README.txt"]) if (!names.includes(n)) failures.push(`launcher misses ${n}`);
  const ps = readFileSync(join(HERE, "launcher", "play.ps1"), "utf8");
  for (const must of ["1874880", "libraryfolders.vdf", "playtest-server.json", "LF_PlaytestTown.conf", "-addonsDir"]) {
    if (!ps.includes(must)) failures.push(`play.ps1 does not mention ${must}`);
  }
  return failures;
}

// ------------------------------------------------------------------ cli
function argOf(list, flag) {
  const i = list.indexOf(flag);
  return i >= 0 ? list[i + 1] : undefined;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const argv = process.argv.slice(2);
  if (argv.includes("--selftest")) {
    const f = selftestPack();
    for (const x of f) console.log(`FAIL  ${x}`);
    console.log(f.length ? `pack-playtest selftest: ${f.length} failure(s)` : "pack-playtest selftest: ok");
    process.exit(f.length ? 1 : 0);
  }
  const out = resolve(argOf(argv, "--out") ?? join(ROOT, "tmp", "release", ZIP_NAME));
  const version = argOf(argv, "--version") ?? `dev-${git(["rev-parse", "--short", "HEAD"]).trim()}`;
  const rdb = argOf(argv, "--rdb");
  if (rdb && !existsSync(rdb)) {
    console.log(`FAIL  --rdb ${rdb} does not exist`);
    process.exit(1);
  }
  const allowNoRdb = argv.includes("--no-rdb");
  if (allowNoRdb) console.log("WARN  --no-rdb: the zip has no resourceDatabase.rdb and the game will not load it (test zips only)");
  let r;
  try {
    r = pack({ out, version, rdb, allowNoRdb });
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    process.exit(1);
  }
  console.log(`packed ${r.files} files, ${(r.bytes / 1048576).toFixed(1)} MB, ${version} (${r.commit.slice(0, 8)})${r.rdb ? " with rdb" : ""} -> ${r.out}`);
}
