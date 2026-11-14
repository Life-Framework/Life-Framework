// The base game's own resource database (addons/data/resourceDatabase.rdb in the game install):
// the one place that says which GUID a vanilla path has. The MCP file index has the paths but no
// GUIDs, and it does not list Common/ at all.
//
// Layout read on 2026-11-14 (game 1.9.0.37), only as far as this needs: each resource is a u32
// length (path bytes + NUL), the path, NUL, six bytes, then the GUID as 8 bytes little-endian.
// Checked against GUIDs the repo already uses (brick, concrete and metal .gamemat). guidOf() returns
// undefined for a path it cannot find, and gameDb() returns null when there is no database to read,
// so a caller can skip rather than fail on a machine without the game.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

const DEFAULT_GAME = "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";

export function gameDb(gameDir = process.env.ENFUSION_GAME_PATH || DEFAULT_GAME) {
  const files = ["data", "core"].map((a) => join(gameDir, "addons", a, "resourceDatabase.rdb")).filter((f) => existsSync(f));
  if (!files.length) return null;
  const bufs = files.map((f) => readFileSync(f));
  return { files, guidOf: (path) => guidIn(bufs, path) };
}

export function guidIn(bufs, path) {
  const needle = Buffer.from(path + "\0", "latin1");
  for (const b of bufs) {
    for (let i = b.indexOf(needle); i >= 0; i = b.indexOf(needle, i + 1)) {
      if (i < 4 || b.readUInt32LE(i - 4) !== needle.length) continue; // a longer path ending the same
      const at = i + needle.length + 6;
      if (at + 8 > b.length) continue;
      return Buffer.from(b.subarray(at, at + 8)).reverse().toString("hex").toUpperCase();
    }
  }
  return undefined;
}

// One record as the database holds it, for the selftest fixture.
export function rdbRecord(path, guid) {
  const p = Buffer.from(path + "\0", "latin1");
  const len = Buffer.alloc(4);
  len.writeUInt32LE(p.length);
  return Buffer.concat([len, p, Buffer.from([6, 0, 0, 0, 0, 0]), Buffer.from(guid, "hex").reverse(), Buffer.alloc(8)]);
}
