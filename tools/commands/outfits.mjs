// tools\cli outfits: NPC roles (outfits + talk) from tools/assets/outfits/roles.json.
export const help = "outfits generate [--dry-run]   write the NPC outfit prefabs, role table, strings and layer references from tools/assets/outfits/roles.json";

export async function main(args) {
  const sub = args[0];
  if (sub !== "generate") {
    console.error("usage: tools\\cli outfits generate [--dry-run]");
    return 2;
  }
  return (await import("../outfits/generate.mjs")).main(args.slice(1));
}
