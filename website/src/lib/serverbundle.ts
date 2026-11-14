// The server.json half of the bundle (#381). The shape is the base game's dedicated-server config
// (the BIKI page Arma_Reforger:Server_Config), taken from docs/playtest/server-config.example.json
// at build time; the addon ids come from the two .gproj files. Plain functions: scripts/configgen-selftest.mjs runs them.

export type ServerData = {
  template: { bindPort?: number; a2s?: { port?: number }; game?: { maxPlayers?: number } } & Record<string, unknown>;
  core: { id: string; name: string; scenario: string };
  pack: { id: string; name: string; scenario: string };
};
export type ServerOpts = { name: string; scenario: "pack" | "core"; port: number; a2sPort: number; maxPlayers: number; adminPassword: string };

export const defaultServerOpts = (d: ServerData): ServerOpts => ({
  name: "My Life Framework server",
  scenario: "pack",
  port: d.template.bindPort ?? 2001,
  a2sPort: d.template.a2s?.port ?? 17777,
  maxPlayers: d.template.game?.maxPlayers ?? 16,
  adminPassword: "CHANGE-ME",
});

const isPort = (n: number) => Number.isInteger(n) && n >= 1 && n <= 65535;

// Messages for what the server would refuse or what would lock the owner out.
export function serverProblems(o: ServerOpts): { level: "error" | "warn"; msg: string }[] {
  const out: { level: "error" | "warn"; msg: string }[] = [];
  const err = (msg: string) => out.push({ level: "error", msg });
  if (!o.name.trim()) err("server.json game.name is empty; the server browser needs a name");
  if (!isPort(o.port)) err(`server.json bindPort ${o.port} is not a port between 1 and 65535`);
  if (!isPort(o.a2sPort)) err(`server.json a2s.port ${o.a2sPort} is not a port between 1 and 65535`);
  if (isPort(o.port) && o.port === o.a2sPort) err("server.json bindPort and a2s.port are both " + o.port + "; they must differ");
  if (!Number.isInteger(o.maxPlayers) || o.maxPlayers < 1 || o.maxPlayers > 128) err(`server.json game.maxPlayers ${o.maxPlayers} is not a whole number between 1 and 128`);
  if (/\s/.test(o.adminPassword)) err("server.json game.passwordAdmin has a space; #login takes it as one word");
  if (!o.adminPassword) err("server.json game.passwordAdmin is empty; nobody could log in as admin");
  else if (o.adminPassword === "CHANGE-ME") out.push({ level: "warn", msg: "game.passwordAdmin is still CHANGE-ME; set your own before you start" });
  return out;
}

// Core always; the pack too when the scenario is the 1989 one (the pack depends on the core, so the core goes first).
export const modsOf = (d: ServerData, o: ServerOpts) => (o.scenario === "pack" ? [d.core, d.pack] : [d.core]).map((m) => ({ modId: m.id, name: m.name }));

export function serverJson(d: ServerData, o: ServerOpts): string {
  const t = JSON.parse(JSON.stringify(d.template)) as { [k: string]: any }; // eslint-disable-line @typescript-eslint/no-explicit-any
  t.bindPort = o.port;
  t.publicPort = o.port;
  t.a2s = { ...t.a2s, port: o.a2sPort };
  t.game = { ...t.game, name: o.name, passwordAdmin: o.adminPassword, scenarioId: d[o.scenario].scenario, maxPlayers: o.maxPlayers, mods: modsOf(d, o) };
  return JSON.stringify(t, null, 2) + "\n";
}

// For panel hosts that take the start line and the mod list as separate boxes.
export const startupLine = () => "ArmaReforgerServer.exe -config server.json -profile ./profile -maxFPS 60";
export const modIdList = (d: ServerData, o: ServerOpts) => modsOf(d, o).map((m) => m.modId).join(",");
