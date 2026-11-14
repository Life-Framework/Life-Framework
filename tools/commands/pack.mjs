// tools\cli pack new|check|explain: the content-pack tools (tools/pack/).
export const help = "pack new <Name> | check <addon> | explain <key>   scaffold addons/LF_<Name>; check a pack offline (ids, overrides, GUIDs, strings, version range); show which layer (core, pack, owner) set a value";

export async function main(args, ctx) {
  const [sub, ...rest] = args;
  if (sub === "check") return (await import("../pack/check.mjs")).main(rest, ctx);
  if (sub === "explain") return (await import("../pack/explain.mjs")).main(rest, ctx);
  return (await import("../pack/new.mjs")).main(args, ctx);
}
