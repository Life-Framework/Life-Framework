// tools\cli pack new: the content-pack scaffold (tools/pack/new.mjs).
export const help = "pack new <Name>              scaffold addons/LF_<Name>: .gproj on the core, folders, string table, mission header, manifest, empty overlay, README";

export async function main(args, ctx) {
  return (await import("../pack/new.mjs")).main(args, ctx);
}
